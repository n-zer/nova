#pragma once

#ifndef NOVA_H
#define NOVA_H

#include <vector>
#include <thread>
#include <atomic>
#include <cassert>
#include <functional>
#include <numeric>
#include <semaphore>

namespace nova 
{
    namespace detail
    {
        template<typename T>
        class mpmc_ring_buffer
        {
            struct Node
            {
                T data;
                std::atomic<std::size_t> sequence;
            };

            std::vector<Node> buffer;
            alignas(64) std::atomic<std::size_t> enqueue_pos;
            alignas(64) std::atomic<std::size_t> dequeue_pos;

        public:
            mpmc_ring_buffer(size_t capacity)
                : buffer(capacity)
            {
                // Size must be a power of two
                assert((capacity & (capacity - 1)) == 0);
                for (std::size_t i = 0; i < capacity; ++i)
                {
                    buffer[i].sequence.store(i);
                }
                enqueue_pos.store(0);
                dequeue_pos.store(0);
            }

            size_t capacity() const { return buffer.size(); }

            bool try_push(
                T&& data)
            {
                while (true)
                {
                    auto pos = enqueue_pos.load(std::memory_order_relaxed);
                    auto* node = &buffer[pos & (capacity() - 1)];
                    auto seq = node->sequence.load(std::memory_order_acquire);
                    auto diff = (intptr_t)seq - (intptr_t)pos;

                    // Slot is ready for a new item
                    if (diff == 0)
                    {
                        if (enqueue_pos.compare_exchange_weak(pos, pos + 1, std::memory_order_acq_rel, std::memory_order_relaxed))
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
                }
            }

            bool try_pop(
                T& data)
            {
                while (true)
                {
                    auto pos = dequeue_pos.load(std::memory_order_relaxed);
                    auto* node = &buffer[pos & (capacity() - 1)];
                    auto seq = node->sequence.load(std::memory_order_acquire);
                    auto diff = (intptr_t)seq - (intptr_t)(pos + 1);

                    // Slot has data to be consumed
                    if (diff == 0)
                    {
                        if (dequeue_pos.compare_exchange_weak(pos, pos + 1, std::memory_order_acq_rel, std::memory_order_relaxed))
                        {
                            data = std::move(node->data);
                            node->sequence.store(pos + capacity(), std::memory_order_release);
                            return true;
                        }
                    }
                    // Buffer is empty
                    else if (diff < 0)
                    {
                        return false;
                    }
                    // Someone else grabbed this item
                }
            }
        };

        using job = std::move_only_function<void()>;
    }

    struct range
    {
        size_t start = 0;
        size_t end = 0;
        size_t grain = 0;

        size_t distance() const { return end - start; }
    };

    class thread_pool
    {
        struct work_context
        {
            work_context(thread_pool& threadPool) : thread_pool(threadPool) {}
            thread_pool& thread_pool;
            std::atomic<uint32_t> ref_count{};
            std::atomic<uint32_t> job_count{};
        };

        struct job_context
        {
            job_context(
                work_context& workContext)
                : work_context(&workContext)
            {
                work_context->job_count.fetch_add(1);
                work_context->ref_count.fetch_add(1);
            }

            ~job_context()
            {
                if (work_context
                    && work_context->job_count.fetch_sub(1) == 1)
                {
                    work_context->job_count.notify_all();
                }

                if (work_context 
                    && work_context->ref_count.fetch_sub(1) == 1)
                {
                    delete work_context;
                }
            }

            job_context(
                job_context&& other) noexcept
            {
                *this = std::move(other);
            }

            job_context& operator=(
                job_context&& other) noexcept
            {
                work_context = other.work_context;
                other.work_context = nullptr;
                return *this;
            }

            work_context* work_context;
        };

        struct task_context
        {
            task_context(
                work_context& workContext) : work_context(&workContext)
            {
                work_context->ref_count.fetch_add(1);
            }

            ~task_context()
            {
                if (work_context
                    && work_context->ref_count.fetch_sub(1) == 1)
                {
                    delete work_context;
                }
            }

            task_context(
                const task_context& other)
            {
                *this = other;
            }

            task_context& operator=(
                const task_context& other)
            {
                work_context = other.work_context;
                work_context->ref_count.fetch_add(1);
                return *this;
            }

            task_context(
                task_context&& other) noexcept
            {
                *this = std::move(other);
            }

            task_context& operator=(
                task_context&& other) noexcept
            {
                work_context = other.work_context;
                other.work_context = nullptr;
                return *this;
            }

            work_context* work_context;
        };

    public:
        struct params
        {
            size_t num_threads = std::thread::hardware_concurrency();
            // Must be power of two
            size_t queue_capacity = 1024;
        };
        thread_pool(
            const params& params = {})
            : job_queue(params.queue_capacity)
        {
            assert(params.num_threads > 0);

            if (params.num_threads > 1)
            {
                worker_threads.reserve(params.num_threads - 1);

                // Main thread is ID 0. Worker threads are IDs 1 and onward.
                for (size_t n = 1; n <= worker_threads.capacity(); ++n)
                {
                    worker_threads.emplace_back(
                        [this, n](std::stop_token s)
                    {
                        while (!s.stop_requested())
                        {
                            if (detail::job j; _pop(j))
                            {
                                j();
                            }
                        }
                    });
                }
            }
        }

        ~thread_pool()
        {
            job_semaphore.release(worker_threads.size() + 1);
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
                if (!context.work_context)
                {
                    return;
                }

                thread_pool& threadPool = context.work_context->thread_pool;
                while (true)
                {
                    auto jobCount = context.work_context->job_count.load();
                    if (!jobCount)
                    {
                        return;
                    }

                    if (detail::job j; threadPool._try_pop(j))
                    {
                        j();
                        continue;
                    }

                    context.work_context->job_count.wait(jobCount);
                }
            }

        private:
            friend class thread_pool;

            task(
                thread_pool& threadPool)
                : context(*new work_context(threadPool))
            {
            }

            task_context context;
        };

        template<typename... Func>
        void async(
            task& handle, 
            Func&&... func)
        {
            (_helpful_push(
                [context = job_context(*handle.context.work_context), f = std::forward<Func>(func)]() mutable { f(); }), ...);
        }

        template<typename... Func>
        task async(
            Func&&... func)
        {
            task handle(*this);
            async(handle, std::forward<Func>(func)...);
            return handle;
        }

        template<typename Func>
        void sync_all(
            Func&& func)
        {
            task handle(*this);
            for (size_t n = 0; n < thread_count(); ++n)
            {
                async(handle, std::ref(func));
            }
            handle.wait();
        }

        template<typename Func>
        void parallel_for(
            const range& range,
            Func&& func)
        {
            task handle(*this);
            auto grain = range.grain ? range.grain : (range.distance() + thread_count() - 1) / thread_count();
            for (size_t n = 0; n < range.end; n += grain)
            {
                _helpful_push(
                    [start = n, end = std::min(n + grain, range.end), context = job_context(*handle.context.work_context), &func]() mutable
                {
                    func(start, end);
                });
            }
            handle.wait();
        }

        size_t thread_count() const { return worker_threads.size() + 1; }

    private:
        void _helpful_push(
            detail::job&& job)
        {
            while (!_try_push(std::move(job)))
            {
                if (detail::job j; _try_pop(j))
                {
                    j();
                }
            }
        }

        bool _try_push(
            detail::job&& job)
        {
            if (job_queue.try_push(std::move(job)))
            {
                job_semaphore.release();
                return true;
            }

            return false;
        }

        bool _try_pop(
            detail::job& j)
        {
            if (!job_semaphore.try_acquire())
            {
                return false;
            }

            if (job_queue.try_pop(j))
            {
                return true;
            }

            job_semaphore.release();
            return false;
        }

        bool _pop(
            detail::job& j)
        {
            job_semaphore.acquire();
            if (job_queue.try_pop(j))
            {
                return true;
            }
            job_semaphore.release();
            return false;
        }

        detail::mpmc_ring_buffer<detail::job> job_queue;
        std::vector<std::jthread> worker_threads;
        std::counting_semaphore<> job_semaphore{ 0 };
    };
}

#endif // !NOVA_H
