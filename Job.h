#pragma once

struct Job {
	void (*m_task)(void*);
	void* m_data;
};
