#pragma once

#include <Windows.h>

namespace Nova {

	class CriticalLock {
	public:
		CriticalLock(CRITICAL_SECTION & section) : m_section(section) {
			EnterCriticalSection(&m_section);
		}
		~CriticalLock() {
			LeaveCriticalSection(&m_section);
		}
	private:
		CRITICAL_SECTION & m_section;

		CriticalLock(const CriticalLock& other) = delete;
	};
}