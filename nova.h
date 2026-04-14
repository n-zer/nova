#pragma once

#ifndef NOVA_H
#define NOVA_H

#include <vector>
#include <thread>
#include <atomic>
#include <cassert>
#include <functional>

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

            Node buffer[Size];
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

    template<size_t Capacity = 128>
    class thread_pool
    {
        struct work_context
        {
            work_context(thread_pool& threadPool) : thread_pool(threadPool) {}
            thread_pool& thread_pool;
            std::atomic<uint32_t> task_count{};
            std::atomic<uint32_t> job_count{};
        };

        struct job_context
        {
            job_context(work_context& workContext) : work_context(&workContext)
            {
                work_context->job_count.fetch_add(1);
            }

            ~job_context()
            {
                if (work_context
                    && work_context->job_count.fetch_sub(1) == 0
                    && work_context->task_count.load() == 0)
                {
                    delete work_context;
                }
            }

            job_context(job_context&& other)
            {
                *this = std::move(other);
            }

            job_context& operator=(job_context&& other)
            {
                work_context = other.work_context;
                other.work_context = nullptr;
                return *this;
            }

            work_context* work_context;
        };

        struct task_context
        {
            task_context(work_context& workContext) : work_context(&workContext)
            {
                work_context->task_count.fetch_add(1);
            }

            ~task_context()
            {
                if (work_context
                    && work_context->job_count.load() == 0
                    && work_context->task_count.fetch_sub(1) == 0)
                {
                    delete work_context;
                }
            }

            task_context(const task_context& other)
            {
                *this = other;
            }

            task_context& operator=(const task_context& other)
            {
                work_context = other.work_context;
                work_context->task_count.fetch_add(1);
                return *this;
            }

            task_context(task_context&& other)
            {
                *this = std::move(other);
            }

            task_context& operator=(task_context&& other)
            {
                work_context = other.work_context;
                other.work_context = nullptr;
                return *this;
            }

            work_context* work_context;
        };

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
                        [this, n](std::stop_token s)
                    {
                        detail::g_thread_id = n;
                        work_until([s]() { return s.stop_requested(); });
                    });
                }
            }
        }

        class task
        {
        public:
            task(const task&) = default;
            task& operator=(const task&) = default;
            task(task&&) = default;
            task& operator=(task&&) = default;

            void wait()
            {
                if (context.work_context && context.work_context->job_count.load())
                {
                    context.work_context->thread_pool.work_until(
                        [&workContext = *context.work_context]() { return workContext.job_count.load() == 0; });
                }
            }

        private:
            friend class thread_pool;

            task(work_context& context)
                : context(context)
            {
            }

            task_context context;
        };

        template<typename... Funcs>
        void async(task& handle, Funcs&&... funcs)
        {
            (_push_to_queue([context = job_context(*handle.context.work_context), funcs]() { funcs(); }), ...);
        }

        template<typename... Funcs>
        task async(Funcs&&... funcs)
        {
            auto* workContext = new work_context(*this);
            task handle(*workContext);
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
    };
}

#endif // !NOVA_H
