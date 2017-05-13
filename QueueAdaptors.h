#pragma once

#include "concurrentqueue.h"

template<typename T>
class MoodycamelAdaptor {
public:
	bool Pop(T& item) {
		return m_queue.try_dequeue(item);
	}

	void Push(T& item) {
		m_queue.enqueue(item);
	}

	template<unsigned N>
	void Push(std::array<T, N> && items) {
		m_queue.enqueue_bulk(items.begin(), items.size());
	}

	void Push(std::vector<T> && items) {
		m_queue.enqueue_bulk(items.begin(), items.size());
	}
private:
	moodycamel::ConcurrentQueue<T> m_queue;
};

template<typename T>
class mpmc_bounded_queue
{
public:
	mpmc_bounded_queue(size_t buffer_size)
		: buffer_(new cell_t[buffer_size])
		, buffer_mask_(buffer_size - 1)
	{
		assert((buffer_size >= 2) &&
			((buffer_size & (buffer_size - 1)) == 0));
		for (size_t i = 0; i != buffer_size; i += 1)
			buffer_[i].sequence_.store(i, std::memory_order_relaxed);
		enqueue_pos_.store(0, std::memory_order_relaxed);
		dequeue_pos_.store(0, std::memory_order_relaxed);
	}

	~mpmc_bounded_queue()
	{
		delete[] buffer_;
	}

	bool enqueue(T const& data)
	{
		cell_t* cell;
		size_t pos = enqueue_pos_.load(std::memory_order_relaxed);
		for (;;)
		{
			cell = &buffer_[pos & buffer_mask_];
			size_t seq =
				cell->sequence_.load(std::memory_order_acquire);
			intptr_t dif = (intptr_t)seq - (intptr_t)pos;
			if (dif == 0)
			{
				if (enqueue_pos_.compare_exchange_weak
				(pos, pos + 1, std::memory_order_relaxed))
					break;
			}
			else if (dif < 0)
				return false;
			else
				pos = enqueue_pos_.load(std::memory_order_relaxed);
		}
		cell->data_ = data;
		cell->sequence_.store(pos + 1, std::memory_order_release);
		return true;
	}

	bool dequeue(T& data)
	{
		cell_t* cell;
		size_t pos = dequeue_pos_.load(std::memory_order_relaxed);
		for (;;)
		{
			cell = &buffer_[pos & buffer_mask_];
			size_t seq =
				cell->sequence_.load(std::memory_order_acquire);
			intptr_t dif = (intptr_t)seq - (intptr_t)(pos + 1);
			if (dif == 0)
			{
				if (dequeue_pos_.compare_exchange_weak
				(pos, pos + 1, std::memory_order_relaxed))
					break;
			}
			else if (dif < 0)
				return false;
			else
				pos = dequeue_pos_.load(std::memory_order_relaxed);
		}
		data = std::move(cell->data_);
		cell->sequence_.store
		(pos + buffer_mask_ + 1, std::memory_order_release);
		return true;
	}

private:
	struct cell_t
	{
		std::atomic<size_t>   sequence_;
		T                     data_;
	};

	static size_t const     cacheline_size = 64;
	typedef char            cacheline_pad_t[cacheline_size];

	cacheline_pad_t         pad0_;
	cell_t* const           buffer_;
	size_t const            buffer_mask_;
	cacheline_pad_t         pad1_;
	std::atomic<size_t>     enqueue_pos_;
	cacheline_pad_t         pad2_;
	std::atomic<size_t>     dequeue_pos_;
	cacheline_pad_t         pad3_;

	mpmc_bounded_queue(mpmc_bounded_queue const&);
	void operator = (mpmc_bounded_queue const&);
};

#define MPMC_BUFFER_SIZE 32768

template<typename T>
class MPMCBoundedAdaptor {
public:
	MPMCBoundedAdaptor()
		: m_queue(MPMC_BUFFER_SIZE) {
	}
	bool Pop(T& item) {
		return m_queue.dequeue(item);
	}

	void Push(T& item) {
		m_queue.enqueue(item);
	}

	template<unsigned N>
	void Push(std::array<T, N> && items) {
		for (T & t : items)
			Push(t);
	}

	void Push(std::vector<T> && items) {
		for (T & t : items)
			Push(t);
	}
private:
	mpmc_bounded_queue<T> m_queue;
};

#define NOVA_QUEUE_TYPE MoodycamelAdaptor