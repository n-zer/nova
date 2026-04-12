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
#include <syncstream>
#include <stacktrace>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

void* get_fiber_id() {
#ifdef _WIN32
    // NT_TIB is the Thread Information Block
    // StackBase is a unique pointer to the top of the current stack
    return ((PNT_TIB)NtCurrentTeb())->StackBase;
#else
    return nullptr;
#endif
}

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

        using job = std::move_only_function<void()>;

        thread_local size_t g_thread_id = 0;
        thread_local size_t g_async_depth = 0;
        std::atomic<size_t> g_resume_contexts = 0;
    }

    template<typename... T>
    static void log(T&&... args)
    {
        //(std::osyncstream(std::cout) << ... << args) << " thread_id: " << detail::g_thread_id << " fiber ID: " << get_fiber_id() << std::endl;
    }

    class thread_pool
    {
        struct context_token
        {
            context_token() { ++detail::g_resume_contexts; }
            ~context_token() { --detail::g_resume_contexts; }
        };
        // On expiration, queue the stored continuation for resumption
        struct resume_context
        {
            resume_context(
                thread_pool& threadPool)
                : threadPool(threadPool)
            {
            }

            ~resume_context()
            {
                //log("resume context destruction");
                if (thread_affinity == std::numeric_limits<size_t>::max())
                {
                    thread_affinity = threadPool.thread_id();
                }

                auto& workerState = threadPool.worker_states[thread_affinity];
                auto& queuedContinuation = workerState.queued_continuation;

                {
                    std::scoped_lock guard(workerState.queued_continuation_mutex);

                    // If no continuation is currently queued, we can use the slot.
                    // Otherwise, we need to marshal the continuation to the thread
                    // using its private queue.

                    if (!queuedContinuation)
                    {
                        queuedContinuation = std::move(continuation);
                        return;
                    }
                }

                bool success = workerState.private_queue.try_push(
                    [&queuedContinuation, continuation = std::move(continuation)]() mutable
                {
                    assert(!queuedContinuation);
                    queuedContinuation = std::move(continuation);
                });
                assert(success);
            }

            size_t thread_affinity = std::numeric_limits<size_t>::max();
            boost::context::continuation continuation;
            context_token token;
            thread_pool& threadPool;
        };

        struct worker_state
        {
            detail::mpmc_ring_buffer<detail::job, 8> private_queue;

            std::mutex signal_mutex;
            std::condition_variable signal_cv;

            std::vector<boost::context::continuation> free_continuations;
            std::mutex queued_continuation_mutex;
            boost::context::continuation queued_continuation;

            // Needed because continuation::resume_with will copy the given lambda and
            // hold a copy on the suspended stack, because who the fuck knows why
            std::shared_ptr<resume_context> reuse_context;
        };

    public:
        thread_pool(
            size_t numThreads)
            : num_threads(numThreads)
        {
            assert(numThreads > 0);

            worker_states.reset(new worker_state[numThreads]);
            if (numThreads > 1)
            {
                worker_threads.reserve(numThreads - 1);

                // Main thread is ID 0. Worker threads are IDs 1 and onward.
                for (size_t n = 1; n <= worker_threads.capacity(); ++n)
                {
                    worker_threads.emplace_back(
                        [this, n]()
                    {
                        // Enforced affinity on first async ensures jumping directly into the job loop is safe.
                        // Otherwise we would need to yield to a new fiber before starting the job loop,
                        // to allow us to resume the original callstack before terminating.
                        detail::g_thread_id = n;
                        _job_loop();
                    });
                }
            }
        }

        ~thread_pool()
        {
            kill_signal = true;
        }

        class task
        {
        public:
            task(const task&) = delete;
            task(task&&) = default;
            void wait()
            {
                if (context.use_count() > 1)
                {
                    context->threadPool._yield_to_job_loop(context);
                    log("resumed");
                }
            }
        private:
            friend class thread_pool;
            task(thread_pool& threadPool)
                : context(std::make_shared<resume_context>(threadPool))
            {
            }

            std::shared_ptr<resume_context> context;
        };

        template<typename... Funcs>
        task async(Funcs&&... funcs)
        {
            log("beginning async");
            task handle(*this);

            // The first async on each thread has affinity, to ensure that the original callstack
            // is returned to the thread when the tree unwinds.
            if (detail::g_async_depth == 0)
            {
                handle.context->thread_affinity = thread_id();
            }

            struct async_depth_guard
            {
                async_depth_guard() { ++detail::g_async_depth; }
                ~async_depth_guard() { --detail::g_async_depth; }
                async_depth_guard(const async_depth_guard&) = delete;
            } guard;

            // Attach the resume context to all queued jobs so this fiber resumes when all jobs have completed.
            (_push_to_queue([context = handle.context, funcs]() { funcs(); }), ...);

            return handle;
        }

        static size_t thread_id() { return detail::g_thread_id; }
        size_t thread_count() const { return num_threads; }

    private:
        void _job_loop()
        {
            while (!kill_signal)
            {
                while (true)
                {
                    boost::context::continuation continuation;
                    {
                        std::scoped_lock guard(_get_worker_state().queued_continuation_mutex);
                        if (!_get_worker_state().queued_continuation)
                        {
                            break;
                        }
                        
                        continuation = std::move(_get_worker_state().queued_continuation);
                    }

                    //log("resuming continuation");
                    continuation.resume();
                }

                if (detail::job j; _get_worker_state().private_queue.try_pop(j) || job_queue.try_pop(j))
                {
                    j();
                }
            }
        }

        // Yields to a fiber that runs the job loop. When the context expires, the current fiber will be queued for resumption.
        // Returns the continuation for whatever fiber eventually yields control back. Note that this may not be the same fiber we yielded to.
        void _yield_to_job_loop(
            std::shared_ptr<resume_context>& context)
        {
            auto& workerState = _get_worker_state();

            boost::context::continuation continuation;

            if (workerState.free_continuations.empty())
            {
                continuation = boost::context::callcc(
                    [this, context = std::move(context)](boost::context::continuation&& continuation) mutable
                {
                    log("new fiber");
                    context->continuation = std::move(continuation);
                    context.reset();
                    _job_loop();
                    return boost::context::continuation();
                });
            }
            else
            {
                boost::context::continuation reuseContinuation = std::move(workerState.free_continuations.back());
                workerState.free_continuations.pop_back();
                workerState.reuse_context = std::move(context);
                continuation = reuseContinuation.resume_with([this](boost::context::continuation&& continuation) mutable
                {
                    log("reusing fiber");
                    auto& context = _get_worker_state().reuse_context;
                    context->continuation = std::move(continuation);
                    context.reset();
                    return boost::context::continuation();
                });
            }

            // When control is returned, store the fiber that yielded back to us for later reuse.
            // This avoids allocating a stack every time we async.
            _get_worker_state().free_continuations.emplace_back(std::move(continuation));
        }

        worker_state& _get_worker_state() { return worker_states[detail::g_thread_id]; }

        template<typename Func>
        void _push_to_queue(Func&& func)
        {
            bool success = job_queue.try_push(std::forward<Func>(func));
            assert(success);
        }

        size_t num_threads;
        std::unique_ptr<worker_state[]> worker_states;
        std::vector<std::jthread> worker_threads;
        detail::mpmc_ring_buffer<detail::job, 256> job_queue;

        std::atomic_bool kill_signal = false;
    };
}

#endif // !NOVA_H
