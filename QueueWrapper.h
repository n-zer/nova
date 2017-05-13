#pragma once
#include <array>
#include <vector>
#include <Windows.h>
#include "concurrentqueue.h"

#define SPIN_COUNT 10000

class CriticalLock{
public:
	CriticalLock(CRITICAL_SECTION& cs) : m_cs(cs)	{
		EnterCriticalSection(&m_cs);
	}

	~CriticalLock()	{
		LeaveCriticalSection(&m_cs);
	}

private:
	CriticalLock(const CriticalLock&) = delete;

private:
	CRITICAL_SECTION & m_cs;
};

struct CriticalWrapper {
	CriticalWrapper() {
		InitializeCriticalSection(&cs);
	}

	~CriticalWrapper() {
		DeleteCriticalSection(&cs);
	}

	CRITICAL_SECTION cs;
};

template <typename T>
class QueueWrapper {
public:
	QueueWrapper() {
		InitializeConditionVariable(&s_cv);
	}

	void Pop(T& item) {
		unsigned counter = 0;
		while (!m_queue.try_dequeue(item)) {
			if (counter++ > SPIN_COUNT) {
				counter = 0;
				SleepConditionVariableCS(&s_cv, &s_cs.cs, INFINITE);
			}
		}
	}

	void Push(T& item) {
		WakeConditionVariable(&s_cv);
		m_queue.enqueue(item);
	}

	template<unsigned N>
	void Push(std::array<T, N> && items) {
		WakeAllConditionVariable(&s_cv);
		m_queue.enqueue_bulk(items.begin(), items.size());
	}

	void Push(std::vector<T> && items) {
		WakeAllConditionVariable(&s_cv);
		m_queue.enqueue_bulk(items.begin(), items.size());
	}

private:
	moodycamel::ConcurrentQueue<T> m_queue;
	static CONDITION_VARIABLE s_cv;
	static thread_local CriticalWrapper s_cs;
	static thread_local CriticalLock s_cl;
};

template<typename T>
CONDITION_VARIABLE QueueWrapper<T>::s_cv;

template<typename T>
thread_local CriticalWrapper QueueWrapper<T>::s_cs;

template<typename T>
thread_local CriticalLock QueueWrapper<T>::s_cl(QueueWrapper<T>::s_cs);
