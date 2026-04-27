// Register-level syscall switch extracted from the kernel trap file.
#include "syscall/dispatch.hpp"

#include <stddef.h>
#include <stdint.h>

#include "arch/x86_64/cpu/control_regs.hpp"
#include "console/console.hpp"
#include "core/kernel_state.hpp"
#include "proc/thread.hpp"
#include "sched/scheduler.hpp"
#include "syscall/abi.hpp"
#include "syscall/console_read.hpp"
#include "syscall/observe.hpp"
#include "syscall/process.hpp"
#include "syscall/wait.hpp"

Thread* handle_syscall(TrapFrame* frame)
{
    Thread* thread = current_thread();
    if(nullptr == thread)
    {
        return nullptr;
    }

    switch(frame->rax)
    {
        case os1_sys_write:
            frame->rax = (uint64_t)sys_write(
                ProcessSyscallContext{
                    .frames = &page_frames,
                    .kernel_root_cr3 = g_kernel_root_cr3,
                    .write_console_bytes = write_console_bytes,
                    .write_cr3 = write_cr3,
                },
                (int)frame->rdi,
                frame->rsi,
                (size_t)frame->rdx);
            return thread;
        case os1_sys_read: {
            long read_result = -1;
            if((int)frame->rdi != 0)
            {
                frame->rax = (uint64_t)-1;
                return thread;
            }
            if(try_complete_console_read(
                   page_frames, thread, frame->rsi, (size_t)frame->rdx, read_result))
            {
                frame->rax = (uint64_t)read_result;
                return thread;
            }

            block_current_thread(ThreadWaitReason::ConsoleRead, frame->rsi, frame->rdx);
            return schedule_next(false);
        }
        case os1_sys_exit:
            mark_current_thread_dying((int)frame->rdi);
            return schedule_next(false);
        case os1_sys_yield:
            return schedule_next(true);
        case os1_sys_getpid:
            frame->rax = thread->process ? thread->process->pid : 0;
            return thread;
        case os1_sys_observe:
            frame->rax = static_cast<uint64_t>(sys_observe(
                ObserveContext{
                    .boot_info = g_boot_info,
                    .text_display = g_text_display,
                    .timer_ticks = g_timer_ticks,
                    .frames = &page_frames,
                },
                frame->rdi,
                frame->rsi,
                static_cast<size_t>(frame->rdx)));
            return thread;
        case os1_sys_spawn:
            frame->rax = static_cast<uint64_t>(sys_spawn(
                ProcessSyscallContext{
                    .frames = &page_frames,
                    .kernel_root_cr3 = g_kernel_root_cr3,
                    .write_console_bytes = write_console_bytes,
                    .write_cr3 = write_cr3,
                },
                frame->rdi));
            return thread;
        case os1_sys_waitpid: {
            long wait_result = -1;
            if(try_complete_wait_pid(page_frames, thread, frame->rdi, frame->rsi, wait_result))
            {
                frame->rax = static_cast<uint64_t>(wait_result);
                return thread;
            }

            block_current_thread(ThreadWaitReason::ChildExit, frame->rsi, frame->rdi);
            return schedule_next(false);
        }
        case os1_sys_exec:
            frame->rax = static_cast<uint64_t>(sys_exec(
                ProcessSyscallContext{
                    .frames = &page_frames,
                    .kernel_root_cr3 = g_kernel_root_cr3,
                    .write_console_bytes = write_console_bytes,
                    .write_cr3 = write_cr3,
                },
                frame->rdi));
            return thread;
        default:
            frame->rax = (uint64_t)-1;
            return thread;
    }
}