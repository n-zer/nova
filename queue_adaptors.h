#pragma once

#define NOVA_QUEUE_TYPE moodycamel_adaptor

#include "concurrentqueue.h"

template<typename T>
class moodycamel_adaptor {
public:
	bool pop(T& item) {
		return m_queueWrapper.try_dequeue(item);
	}

	void push(T&& item) {
		m_queueWrapper.enqueue(std::forward<T>(item));
	}

	template<std::size_t N>
	void push(std::array<T, N> && items) {
		m_queueWrapper.enqueue_bulk(std::make_move_iterator(std::begin(items)), items.size());
	}

	void push(std::vector<T> && items) {
		m_queueWrapper.enqueue_bulk(std::make_move_iterator(std::begin(items)), items.size());
	}
private:
	moodycamel::ConcurrentQueue<T> m_queueWrapper;
};