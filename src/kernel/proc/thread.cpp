// Thread-table implementation. This file owns thread IDs, kernel stacks, saved
// TrapFrame initialization, wait-state transitions, and CPU-local current-thread
// publication.
#include "proc/thread.h"

#include "arch/x86_64/cpu/cpu.h"
#include "debug/debug.h"
#include "handoff/memory_layout.h"
#include "util/memory.h"

namespace
{
constexpr size_t kThreadTablePageCount =
	(kMaxThreads * sizeof(Thread) + kPageSize - 1) / kPageSize;

uint64_t g_next_tid = 1;
Thread *g_idle_thread = nullptr;

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

bool InitializeThreadTable(PageFrameContainer &frames)
{
	g_next_tid = 1;
	g_idle_thread = nullptr;

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

	memset(threadTable, 0, kThreadTablePageCount * kPageSize);
	for(size_t i = 0; i < kMaxThreads; ++i)
	{
		threadTable[i].state = ThreadState::Free;
	}
	return true;
}
}

Thread *threadTable = nullptr;

bool initTasks(PageFrameContainer &frames)
{
	if(!InitializeProcessTable(frames) || !InitializeThreadTable(frames))
	{
		return false;
	}
	setCurrentThread(nullptr);
	return true;
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
	// Kernel threads enter a normal C++ function through the scheduler return
	// path, not a direct `call`, so reserve one dummy return slot at a SysV
	// function-entry-aligned stack position. The 16 bytes above it stay available
	// for the synthetic kernel `iretq` frame used by the scheduler.
	*((uint64_t*)(thread->kernel_stack_top - 3 * sizeof(uint64_t))) = 0;
	thread->frame.rip = (uint64_t)entry;
	thread->frame.cs = kKernelCodeSegment;
	thread->frame.rflags = 0x202;
	thread->frame.rsp = thread->kernel_stack_top - 3 * sizeof(uint64_t);
	thread->frame.ss = kKernelDataSegment;

	if(nullptr == g_idle_thread)
	{
		// The idle thread enables interrupts explicitly once it reaches its
		// steady-state `sti; hlt` loop. Starting it with IF clear avoids taking a
		// timer interrupt in the middle of its first console write.
		thread->frame.rflags = 0x2;
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

void blockCurrentThread(ThreadWaitReason reason, uint64_t wait_address, uint64_t wait_length)
{
	Thread *thread = currentThread();
	if((nullptr == thread) || (ThreadState::Dying == thread->state) || (ThreadState::Free == thread->state))
	{
		return;
	}

	thread->wait_reason = reason;
	thread->wait_address = wait_address;
	thread->wait_length = wait_length;
	thread->state = ThreadState::Blocked;
	if(thread->process && (ProcessState::Dying != thread->process->state))
	{
		thread->process->state = ProcessState::Ready;
	}
	relinkRunnableThreads();
}

void clearThreadWait(Thread *thread)
{
	if(nullptr == thread)
	{
		return;
	}

	thread->wait_reason = ThreadWaitReason::None;
	thread->wait_address = 0;
	thread->wait_length = 0;
}

Thread *firstBlockedThread(ThreadWaitReason reason)
{
	for(size_t i = 0; i < kMaxThreads; ++i)
	{
		if((ThreadState::Blocked == threadTable[i].state)
			&& (reason == threadTable[i].wait_reason))
		{
			return threadTable + i;
		}
	}
	return nullptr;
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
	clearThreadWait(thread);
	if(thread->process)
	{
		thread->process->exit_status = exit_status;
		thread->process->state = thread->process->parent
			? ProcessState::Zombie
			: ProcessState::Dying;
	}
	relinkRunnableThreads();
}