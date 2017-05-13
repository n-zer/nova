#pragma once
#include <array>
#include <vector>
#include <Windows.h>
#include "QueueAdaptors.h"

#define SPIN_COUNT 10000

#ifndef NOVA_QUEUE_TYPE
#define NOVA_QUEUE_TYPE MoodycamelAdaptor
#endif // !NOVA_QUEUE_TYPE


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
		while (!m_queue.Pop(item)) {
			if (counter++ > SPIN_COUNT) {
				counter = 0;
				SleepConditionVariableCS(&s_cv, &s_cs.cs, INFINITE);
			}
		}
	}

	void Push(T& item) {
		m_queue.Push(item);
		WakeConditionVariable(&s_cv);
	}

	template<typename Collection>
	void Push(Collection && items) {
		m_queue.Push(std::forward<decltype(items)>(items));
		WakeAllConditionVariable(&s_cv);
	}

private:
	NOVA_QUEUE_TYPE<T> m_queue;
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
