// Simple fixed-table runnable selection. This file owns scheduler policy that
// scans threadTable; object lifetime remains in proc/thread.cpp and proc/reaper.cpp.
#include "proc/thread.h"

Thread *nextRunnableThread(Thread *after)
{
	relinkRunnableThreads();
	auto is_runnable = [](const Thread *thread) -> bool
	{
		return (nullptr != thread)
			&& ((ThreadState::Ready == thread->state)
				|| (ThreadState::Running == thread->state));
	};

	size_t start_index = 0;
	if((nullptr != after) && (after >= threadTable) && (after < (threadTable + kMaxThreads)))
	{
		start_index = (size_t)(after - threadTable + 1) % kMaxThreads;
	}

	Thread *idle_candidate = nullptr;
	for(size_t i = 0; i < kMaxThreads; ++i)
	{
		Thread *candidate = threadTable + ((start_index + i) % kMaxThreads);
		if(!is_runnable(candidate))
		{
			continue;
		}
		if(candidate == idleThread())
		{
			if(nullptr == idle_candidate)
			{
				idle_candidate = candidate;
			}
			continue;
		}
		return candidate;
	}

	return idle_candidate;
}

size_t runnableThreadCount(void)
{
	size_t count = 0;
	for(size_t i = 0; i < kMaxThreads; ++i)
	{
		if((ThreadState::Ready == threadTable[i].state)
			|| (ThreadState::Running == threadTable[i].state))
		{
			++count;
		}
	}
	return count;
}

Thread *firstRunnableUserThread(void)
{
	for(size_t i = 0; i < kMaxThreads; ++i)
	{
		if(threadTable[i].user_mode
			&& ((ThreadState::Ready == threadTable[i].state)
				|| (ThreadState::Running == threadTable[i].state)))
		{
			return threadTable + i;
		}
	}
	return nullptr;
}