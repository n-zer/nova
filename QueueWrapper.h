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
		while (!m_globalQueue.Pop(item)) {
			if (counter++ > SPIN_COUNT) {
				counter = 0;
				SleepConditionVariableCS(&s_cv, &s_cs.cs, INFINITE);
			}
		}
	}

	void PopMain(T& item) {
		unsigned counter = 0;
		while (!m_globalQueue.Pop(item) && !m_mainQueue.Pop(item)) {
			if (counter++ > SPIN_COUNT) {
				counter = 0;
				SleepConditionVariableCS(&s_mainCV, &s_cs.cs, INFINITE);
			}
		}
	}

	template<bool ToMain, std::enable_if_t<!ToMain, int> = 0>
	void Push(T&& item) {
		m_globalQueue.Push(std::forward<T>(item));
		WakeConditionVariable(&s_cv);
		WakeConditionVariable(&s_mainCV);
	}

	template<bool ToMain, typename Collection, std::enable_if_t<!ToMain, int> = 0>
	void Push(Collection && items) {
		m_globalQueue.Push(std::forward<decltype(items)>(items));
		WakeAllConditionVariable(&s_cv);
		WakeConditionVariable(&s_mainCV);
	}

	template<bool ToMain, std::enable_if_t<ToMain, int> = 0>
	void Push(T&& item) {
		m_mainQueue.Push(std::forward<T>(item));
		WakeConditionVariable(&s_mainCV);
	}

	template<bool ToMain, typename Collection, std::enable_if_t<ToMain, int> = 0>
	void Push(Collection && items) {
		m_mainQueue.Push(std::forward<decltype(items)>(items));
		WakeConditionVariable(&s_mainCV);
	}

private:
	NOVA_QUEUE_TYPE<T> m_globalQueue;
	NOVA_QUEUE_TYPE<T> m_mainQueue;
	static CONDITION_VARIABLE s_cv;
	static CONDITION_VARIABLE s_mainCV;
	static thread_local CriticalWrapper s_cs;
	static thread_local CriticalLock s_cl;
};

template<typename T>
CONDITION_VARIABLE QueueWrapper<T>::s_cv;

template<typename T>
CONDITION_VARIABLE QueueWrapper<T>::s_mainCV;

template<typename T>
thread_local CriticalWrapper QueueWrapper<T>::s_cs;

template<typename T>
thread_local CriticalLock QueueWrapper<T>::s_cl(QueueWrapper<T>::s_cs);
