#include "task.h"

#include "cpu.h"
#include "debug.h"
#include "memory.h"
#include "memory_layout.h"
#include "virtualmemory.h"

namespace
{
constexpr size_t kProcessTablePageCount =
	(kMaxProcesses * sizeof(Process) + kPageSize - 1) / kPageSize;
constexpr size_t kThreadTablePageCount =
	(kMaxThreads * sizeof(Thread) + kPageSize - 1) / kPageSize;

uint64_t g_next_pid = 1;
uint64_t g_next_tid = 1;
Process *g_kernel_process = nullptr;
Thread *g_idle_thread = nullptr;

Process *nextFreeProcess()
{
	for(size_t i = 0; i < kMaxProcesses; ++i)
	{
		if(ProcessState::Free == processTable[i].state)
		{
			return processTable + i;
		}
	}
	return nullptr;
}

Thread *nextFreeThread()
{
	for(size_t i = 0; i < kMaxThreads; ++i)
	{
		if(ThreadState::Free == threadTable[i].state)
		{
			return threadTable + i;
		}
	}
	return nullptr;
}

void relinkRunnableThreads()
{
	Thread *first = nullptr;
	Thread *last = nullptr;
	for(size_t i = 0; i < kMaxThreads; ++i)
	{
		threadTable[i].next = nullptr;
		if((ThreadState::Ready == threadTable[i].state)
			|| (ThreadState::Running == threadTable[i].state))
		{
			if(nullptr == first)
			{
				first = threadTable + i;
			}
			if(last)
			{
				last->next = threadTable + i;
			}
			last = threadTable + i;
		}
	}
	if(last)
	{
		last->next = first;
	}
}

void clearThread(Thread *thread)
{
	if(thread)
	{
		memset(thread, 0, sizeof(Thread));
		thread->state = ThreadState::Free;
	}
}

void clearProcess(Process *process)
{
	if(process)
	{
		memset(process, 0, sizeof(Process));
		process->state = ProcessState::Free;
	}
}

bool processHasThreads(Process *process)
{
	if(nullptr == process)
	{
		return false;
	}

	for(size_t i = 0; i < kMaxThreads; ++i)
	{
		if((threadTable[i].process == process)
			&& (ThreadState::Free != threadTable[i].state))
		{
			return true;
		}
	}
	return false;
}

void fillProcessName(Process *process, const char *name)
{
	if(nullptr == process)
	{
		return;
	}
	if(nullptr == name)
	{
		process->name[0] = 0;
		return;
	}
	size_t index = 0;
	while((index + 1) < sizeof(process->name) && name[index])
	{
		process->name[index] = name[index];
		++index;
	}
	process->name[index] = 0;
}
}

Process *processTable = nullptr;
Thread *threadTable = nullptr;

bool initTasks(PageFrameContainer &frames)
{
	g_next_pid = 1;
	g_next_tid = 1;

	if(nullptr == processTable)
	{
		uint64_t process_table_address = 0;
		if(!frames.Allocate(process_table_address, kProcessTablePageCount))
		{
			debug("process table allocation failed")();
			return false;
		}
		processTable = (Process*)process_table_address;
		debug("process table allocated at 0x")(process_table_address, 16)();
	}

	if(nullptr == threadTable)
	{
		uint64_t thread_table_address = 0;
		if(!frames.Allocate(thread_table_address, kThreadTablePageCount))
		{
			debug("thread table allocation failed")();
			return false;
		}
		threadTable = (Thread*)thread_table_address;
		debug("thread table allocated at 0x")(thread_table_address, 16)();
	}

	memset(processTable, 0, kProcessTablePageCount * kPageSize);
	memset(threadTable, 0, kThreadTablePageCount * kPageSize);
	for(size_t i = 0; i < kMaxProcesses; ++i)
	{
		processTable[i].state = ProcessState::Free;
	}
	for(size_t i = 0; i < kMaxThreads; ++i)
	{
		threadTable[i].state = ThreadState::Free;
	}

	g_kernel_process = nullptr;
	g_idle_thread = nullptr;
	setCurrentThread(nullptr);
	return true;
}

Process *createKernelProcess(uint64_t kernel_cr3)
{
	Process *process = nextFreeProcess();
	if(nullptr == process)
	{
		return nullptr;
	}

	process->pid = g_next_pid++;
	process->state = ProcessState::Ready;
	process->address_space.cr3 = kernel_cr3;
	process->exit_status = 0;
	fillProcessName(process, "kernel");
	g_kernel_process = process;
	return process;
}

Process *createUserProcess(const char *name, uint64_t cr3)
{
	Process *process = nextFreeProcess();
	if(nullptr == process)
	{
		return nullptr;
	}

	process->pid = g_next_pid++;
	process->state = ProcessState::Ready;
	process->address_space.cr3 = cr3;
	process->exit_status = 0;
	fillProcessName(process, name);
	return process;
}

Thread *createKernelThread(Process *process, void (*entry)(void), PageFrameContainer &frames)
{
	Thread *thread = nextFreeThread();
	if((nullptr == thread) || (nullptr == process) || (nullptr == entry))
	{
		return nullptr;
	}

	uint64_t stack_base = 0;
	if(!frames.Allocate(stack_base, kKernelThreadStackPages))
	{
		return nullptr;
	}
	memset((void*)stack_base, 0, kKernelThreadStackPages * kPageSize);

	thread->tid = g_next_tid++;
	thread->process = process;
	thread->state = ThreadState::Ready;
	thread->user_mode = false;
	thread->address_space_cr3 = process->address_space.cr3;
	thread->kernel_stack_base = stack_base;
	thread->kernel_stack_top = stack_base + kKernelThreadStackPages * kPageSize;
	thread->exit_status = 0;
	thread->frame = {};
	// Kernel threads enter a normal C++ function through `iretq`, not a `call`,
	// so reserve one dummy return slot to preserve the usual SysV stack shape at
	// function entry.
	*((uint64_t*)(thread->kernel_stack_top - sizeof(uint64_t))) = 0;
	thread->frame.rip = (uint64_t)entry;
	thread->frame.cs = kKernelCodeSegment;
	thread->frame.rflags = 0x202;
	thread->frame.rsp = thread->kernel_stack_top - sizeof(uint64_t);
	thread->frame.ss = kKernelDataSegment;

	if(nullptr == g_idle_thread)
	{
		g_idle_thread = thread;
	}

	relinkRunnableThreads();
	return thread;
}

Thread *createUserThread(Process *process, uint64_t user_rip, uint64_t user_rsp, PageFrameContainer &frames)
{
	Thread *thread = nextFreeThread();
	if(nullptr == thread)
	{
		return nullptr;
	}

	uint64_t stack_base = 0;
	if(!frames.Allocate(stack_base, kKernelThreadStackPages))
	{
		return nullptr;
	}
	memset((void*)stack_base, 0, kKernelThreadStackPages * kPageSize);

	thread->tid = g_next_tid++;
	thread->process = process;
	thread->state = ThreadState::Ready;
	thread->user_mode = true;
	thread->address_space_cr3 = process->address_space.cr3;
	thread->kernel_stack_base = stack_base;
	thread->kernel_stack_top = stack_base + kKernelThreadStackPages * kPageSize;
	thread->exit_status = 0;
	thread->frame = {};
	thread->frame.rip = user_rip;
	thread->frame.cs = kUserCodeSegment;
	thread->frame.rflags = 0x202;
	thread->frame.rsp = user_rsp;
	thread->frame.ss = kUserDataSegment;

	relinkRunnableThreads();
	return thread;
}

Thread *currentThread(void)
{
	return cpu_cur()->current_thread;
}

Thread *idleThread(void)
{
	return g_idle_thread;
}

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
		if(candidate == g_idle_thread)
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

void setCurrentThread(Thread *thread)
{
	cpu_cur()->current_thread = thread;
	if(thread)
	{
		thread->state = ThreadState::Running;
		if(thread->process)
		{
			thread->process->state = ProcessState::Running;
		}
		cpu_set_kernel_stack(thread->kernel_stack_top);
	}
}

void markThreadReady(Thread *thread)
{
	if((nullptr != thread)
		&& (ThreadState::Dying != thread->state)
		&& (ThreadState::Free != thread->state))
	{
		thread->state = ThreadState::Ready;
		if(thread->process && (ProcessState::Dying != thread->process->state))
		{
			thread->process->state = ProcessState::Ready;
		}
	}
}

void markCurrentThreadDying(int exit_status)
{
	Thread *thread = currentThread();
	if(nullptr == thread)
	{
		return;
	}

	thread->exit_status = exit_status;
	thread->state = ThreadState::Dying;
	if(thread->process)
	{
		thread->process->exit_status = exit_status;
		thread->process->state = ProcessState::Dying;
	}
	relinkRunnableThreads();
}

void reapDeadThreads(PageFrameContainer &frames)
{
	Thread *active = currentThread();
	for(size_t i = 0; i < kMaxThreads; ++i)
	{
		Thread *thread = threadTable + i;
		if((thread == active) || (ThreadState::Dying != thread->state))
		{
			continue;
		}

		// Thread teardown is deferred until some *other* thread enters the
		// kernel. That keeps us from freeing the current kernel stack out from
		// under the CPU while it is still executing on it.
		for(size_t page = 0; page < kKernelThreadStackPages; ++page)
		{
			frames.Free(thread->kernel_stack_base + page * kPageSize);
		}

		Process *owner = thread->process;
		clearThread(thread);

		if(owner && !processHasThreads(owner))
		{
			if((owner != g_kernel_process) && (owner->address_space.cr3 != 0))
			{
				VirtualMemory vm(frames, owner->address_space.cr3);
				vm.DestroyUserSlot(kUserPml4Index);
				uint64_t *pml4 = (uint64_t*)owner->address_space.cr3;
				pml4[0] = 0;
				frames.Free(owner->address_space.cr3);
			}
			clearProcess(owner);
		}
	}

	relinkRunnableThreads();
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
