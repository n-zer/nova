#pragma once

#ifndef NOVA_H
#define NOVA_H

#include <vector>
#include <deque>
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
#include <syncstream>

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
    }

    size_t thread_id() { return detail::g_thread_id; }

    template<size_t Capacity = 256>
    class thread_pool
    {
    public:
        thread_pool(
            size_t numThreads = std::thread::hardware_concurrency())
        {
            assert(numThreads > 0);

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
                        work_until([this]() { return kill_signal.load(); });
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
                    context->get().work_until([this]() { return context.use_count() == 1; });
                }
            }
        private:
            friend class thread_pool;
            task(thread_pool& threadPool)
                : context(std::make_shared<std::reference_wrapper<thread_pool>>(threadPool))
            {
            }

            std::shared_ptr<std::reference_wrapper<thread_pool>> context;
        };

        template<typename... Funcs>
        void async(task& handle, Funcs&&... funcs)
        {
            // Attach the resume context to all queued jobs so this fiber resumes when all jobs have completed.
            (_push_to_queue([context = handle.context, funcs]() { funcs(); }), ...);
        }

        template<typename... Funcs>
        task async(Funcs&&... funcs)
        {
            task handle(*this);
            async(handle, std::forward<Funcs>(funcs)...);
            return handle;
        }

        size_t thread_count() const { return worker_threads.size() + 1; }

        template<typename StopFunc>
        void work_until(
            StopFunc&& stopFunc)
        {
            while (!stopFunc())
            {
                detail::job j;
                if (job_queue.try_pop(j))
                {
                    j();
                }
            }
        }

    private:
        template<typename Func>
        void _push_to_queue(Func&& func)
        {
            while (!job_queue.try_push(std::forward<Func>(func)))
            {
                if (detail::job j; job_queue.try_pop(j))
                {
                    j();
                }
            }
        }

        std::vector<std::jthread> worker_threads;
        detail::mpmc_ring_buffer<detail::job, Capacity> job_queue;

        std::atomic_bool kill_signal = false;
    };
}

#endif // !NOVA_H
