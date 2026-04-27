// Register-level syscall switch extracted from the kernel trap file.
#include "syscall/dispatch.h"

#include <stddef.h>
#include <stdint.h>

#include "arch/x86_64/cpu/control_regs.h"
#include "console/console.h"
#include "core/kernel_state.h"
#include "sched/scheduler.h"
#include "syscall/abi.h"
#include "syscall/console_read.h"
#include "syscall/observe.h"
#include "syscall/process.h"
#include "syscall/wait.h"
#include "proc/thread.h"

Thread *HandleSyscall(TrapFrame *frame)
{
	Thread *thread = currentThread();
	if(nullptr == thread)
	{
		return nullptr;
	}

	switch(frame->rax)
	{
	case SYS_write:
		frame->rax = (uint64_t)SysWrite(ProcessSyscallContext{
			.frames = &page_frames,
			.kernel_root_cr3 = g_kernel_root_cr3,
			.write_console_bytes = WriteConsoleBytes,
			.write_cr3 = WriteCr3,
		}, (int)frame->rdi, frame->rsi, (size_t)frame->rdx);
		return thread;
	case SYS_read:
	{
		long read_result = -1;
		if((int)frame->rdi != 0)
		{
			frame->rax = (uint64_t)-1;
			return thread;
		}
		if(TryCompleteConsoleRead(page_frames, thread, frame->rsi, (size_t)frame->rdx, read_result))
		{
			frame->rax = (uint64_t)read_result;
			return thread;
		}

		blockCurrentThread(ThreadWaitReason::ConsoleRead, frame->rsi, frame->rdx);
		return ScheduleNext(false);
	}
	case SYS_exit:
		markCurrentThreadDying((int)frame->rdi);
		return ScheduleNext(false);
	case SYS_yield:
		return ScheduleNext(true);
	case SYS_getpid:
		frame->rax = thread->process ? thread->process->pid : 0;
		return thread;
	case SYS_observe:
		frame->rax = static_cast<uint64_t>(SysObserve(ObserveContext{
			.boot_info = g_boot_info,
			.text_display = g_text_display,
			.timer_ticks = g_timer_ticks,
			.frames = &page_frames,
		}, frame->rdi, frame->rsi, static_cast<size_t>(frame->rdx)));
		return thread;
	case SYS_spawn:
		frame->rax = static_cast<uint64_t>(SysSpawn(ProcessSyscallContext{
			.frames = &page_frames,
			.kernel_root_cr3 = g_kernel_root_cr3,
			.write_console_bytes = WriteConsoleBytes,
			.write_cr3 = WriteCr3,
		}, frame->rdi));
		return thread;
	case SYS_waitpid:
	{
		long wait_result = -1;
		if(TryCompleteWaitPid(page_frames, thread, frame->rdi, frame->rsi, wait_result))
		{
			frame->rax = static_cast<uint64_t>(wait_result);
			return thread;
		}

		blockCurrentThread(ThreadWaitReason::ChildExit, frame->rsi, frame->rdi);
		return ScheduleNext(false);
	}
	case SYS_exec:
		frame->rax = static_cast<uint64_t>(SysExec(ProcessSyscallContext{
			.frames = &page_frames,
			.kernel_root_cr3 = g_kernel_root_cr3,
			.write_console_bytes = WriteConsoleBytes,
			.write_cr3 = WriteCr3,
		}, frame->rdi));
		return thread;
	default:
		frame->rax = (uint64_t)-1;
		return thread;
	}
}