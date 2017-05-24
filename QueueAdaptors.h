#pragma once

#include "concurrentqueue.h"

template<typename T>
class MoodycamelAdaptor {
public:
	bool Pop(T& item) {
		return m_queueWrapper.try_dequeue(item);
	}

	void Push(T&& item) {
		m_queueWrapper.enqueue(std::forward<T>(item));
	}

	template<std::size_t N>
	void Push(std::array<T, N> && items) {
		m_queueWrapper.enqueue_bulk(std::make_move_iterator(std::begin(items)), items.size());
	}

	void Push(std::vector<T> && items) {
		m_queueWrapper.enqueue_bulk(std::make_move_iterator(std::begin(items)), items.size());
	}
private:
	moodycamel::ConcurrentQueue<T> m_queueWrapper;
};

#define NOVA_QUEUE_TYPE MoodycamelAdaptor