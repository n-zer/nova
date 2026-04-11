#pragma once

#ifndef NOVA_H
#define NOVA_H

#include <vector>
#include <array>
#include <thread>
#include <mutex>
#include <tuple>
#include <memory>
#include <cmath>
#include <algorithm>
#include <iterator>
#include <atomic>
#include <functional>
#include <cassert>
#include <boost/context/continuation.hpp>

namespace nova 
{
    namespace detail
    {
        template<typename T, std::size_t Size>
        class mpmc_ring_buffer
        {
            struct Node
            {
                T data;
                std::atomic<std::size_t> sequence;
            };

            static_assert((Size& (Size - 1)) == 0, "Size must be power of 2");

            std::array<Node, Size> buffer;
            alignas(64) std::atomic<std::size_t> enqueue_pos;
            alignas(64) std::atomic<std::size_t> dequeue_pos;

        public:
            mpmc_ring_buffer()
            {
                for (std::size_t i = 0; i < Size; ++i)
                {
                    buffer[i].sequence.store(i);
                }
                enqueue_pos.store(0);
                dequeue_pos.store(0);
            }

            bool try_push(T&& data)
            {
                auto pos = enqueue_pos.load(std::memory_order_relaxed);
                while (true)
                {
                    auto* node = &buffer[pos & (Size - 1)];
                    auto seq = node->sequence.load(std::memory_order_acquire);
                    auto diff = (intptr_t)seq - (intptr_t)pos;

                    // Slot is ready for a new item
                    if (diff == 0)
                    {
                        if (enqueue_pos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                        {
                            node->data = std::move(data);
                            node->sequence.store(pos + 1, std::memory_order_release);
                            return true;
                        }
                    }
                    // Buffer is full
                    else if (diff < 0)
                    {
                        return false;
                    }
                    // Another producer won this slot
                    else
                    { 
                        pos = enqueue_pos.load(std::memory_order_relaxed);
                    }
                }
            }

            bool try_pop(T& data)
            {
                auto pos = dequeue_pos.load(std::memory_order_relaxed);
                while (true)
                {
                    auto* node = &buffer[pos & (Size - 1)];
                    auto seq = node->sequence.load(std::memory_order_acquire);
                    auto diff = (intptr_t)seq - (intptr_t)(pos + 1);

                    // Slot has data to be consumed
                    if (diff == 0)
                    {
                        if (dequeue_pos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                        {
                            data = std::move(node->data);
                            node->sequence.store(pos + Size, std::memory_order_release);
                            return true;
                        }
                    }
                    // Buffer is empty
                    else if (diff < 0)
                    {
                        return false;
                    }
                    // Someone else grabbed this item
                    else
                    {
                        pos = dequeue_pos.load(std::memory_order_relaxed);
                    }
                }
            }
        };

        using job = std::function<void()>;

        struct worker_state
        {
            std::mutex private_queue_lock;
            std::vector<job> private_queue;

            std::mutex signal_mutex;
            std::condition_variable signal_cv;

            std::vector<boost::context::continuation> free_continuations;
            boost::context::continuation queued_continuation;
        };

        thread_local size_t g_thread_id = 0;
    }

    class job_system
    {
    public:
        job_system(
            size_t numThreads)
            : num_threads(numThreads)
        {
            assert(numThreads > 0);

            worker_state.reset(new detail::worker_state[numThreads]);
            if (numThreads > 1)
            {
                worker_threads.reserve(numThreads - 1);

                // Main thread is ID 0. Worker threads are IDs 1 and onward.
                for (size_t n = 1; n <= worker_threads.capacity(); ++n)
                {
                    worker_threads.emplace_back(
                        [this, n]()
                    { 
                        detail::g_thread_id = n;
                        _job_loop();
                    });
                }
            }
        }

        template<typename Func, typename... Funcs>
        void fork(Func&& func, Funcs&&... funcs)
        {
            // Attach a resume context to all queued jobs so this fiber resumes when all jobs have completed.
            auto context = std::make_shared<resume_context>(*this);
            (_push_to_queue([context, funcs]() { funcs(); }), ...);

            func();

            // Don't bother yielding if we're the only ones still holding the context
            if (context.use_count() < 1)
            {
                auto continuation = _yield_to_job_loop(std::move(context));

                // When control is returned, store the fiber that yielded back to us for later reuse
                _get_worker_state().free_continuations.emplace_back(std::move(continuation));
            }
        }

    private:
        void _job_loop()
        {
            while (true)
            {
                while (_get_worker_state().queued_continuation)
                {
                    _get_worker_state().queued_continuation.resume();
                }

                if (detail::job j; job_queue.try_pop(j))
                {
                    j();
                }
            }
        }

        // On expiration, queue the stored continuation for resumption
        struct resume_context
        {
            resume_context(job_system& jobSystem)
                : jobSystem(jobSystem)
            {
            }

            ~resume_context()
            {
                jobSystem._get_worker_state().queued_continuation = std::move(continuation);
            }

            boost::context::continuation continuation;

        private:
            job_system& jobSystem;
        };

        // Yields to a fiber that runs the job loop. When the context expires, the current fiber will be queued for resumption.
        // Returns the continuation for whatever fiber eventually yields control back. Note that this may not be the same fiber we yielded to.
        boost::context::continuation _yield_to_job_loop(
            std::shared_ptr<resume_context> context)
        {
            auto& workerState = _get_worker_state();

            if (workerState.free_continuations.empty())
            {
                return boost::context::callcc(
                    [this, context = std::move(context)](boost::context::continuation&& continuation) mutable
                {
                    context->continuation = std::move(continuation);
                    context.reset();
                    _job_loop();
                    return boost::context::continuation();
                });
            }

            boost::context::continuation continuation = std::move(workerState.free_continuations.back());
            workerState.free_continuations.pop_back();
            return continuation.resume_with([this, context = std::move(context)](boost::context::continuation&& continuation) mutable
            { 
                context->continuation = std::move(continuation);
                context.reset();
                return boost::context::continuation();
            });
        }

        detail::worker_state& _get_worker_state() { return worker_state[detail::g_thread_id]; }

        template<typename Func>
        void _push_to_queue(Func&& func)
        {
            assert(job_queue.try_push(std::forward<Func>(func)));
        }

        size_t num_threads;
        std::unique_ptr<detail::worker_state[]> worker_state;
        std::vector<std::jthread> worker_threads;
        detail::mpmc_ring_buffer<detail::job, 128> job_queue;
    };
}

#endif // !NOVA_H
