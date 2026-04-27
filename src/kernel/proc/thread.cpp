// Thread-table implementation. This file owns thread IDs, kernel stacks, saved
// TrapFrame initialization, wait-state transitions, and CPU-local current-thread
// publication.
#include "proc/thread.hpp"

#include "arch/x86_64/cpu/cpu.hpp"
#include "debug/debug.hpp"
#include "handoff/memory_layout.h"
#include "util/memory.h"

namespace
{
constexpr size_t kThreadTablePageCount = (kMaxThreads * sizeof(Thread) + kPageSize - 1) / kPageSize;

uint64_t g_next_tid = 1;
Thread* g_idle_thread = nullptr;

Thread* nextFreeThread()
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

bool InitializeThreadTable(PageFrameContainer& frames)
{
    g_next_tid = 1;
    g_idle_thread = nullptr;

    if(nullptr == threadTable)
    {
        uint64_t thread_table_address = 0;
        if(!frames.allocate(thread_table_address, kThreadTablePageCount))
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
}  // namespace

Thread* threadTable = nullptr;

bool init_tasks(PageFrameContainer& frames)
{
    if(!initialize_process_table(frames) || !InitializeThreadTable(frames))
    {
        return false;
    }
    set_current_thread(nullptr);
    return true;
}

void relink_runnable_threads()
{
    Thread* first = nullptr;
    Thread* last = nullptr;
    for(size_t i = 0; i < kMaxThreads; ++i)
    {
        threadTable[i].next = nullptr;
        if((ThreadState::Ready == threadTable[i].state) ||
           (ThreadState::Running == threadTable[i].state))
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

void clear_thread(Thread* thread)
{
    if(thread)
    {
        memset(thread, 0, sizeof(Thread));
        thread->state = ThreadState::Free;
    }
}

Thread* create_kernel_thread(Process* process, void (*entry)(void), PageFrameContainer& frames)
{
    Thread* thread = nextFreeThread();
    if((nullptr == thread) || (nullptr == process) || (nullptr == entry))
    {
        return nullptr;
    }

    uint64_t stack_base = 0;
    if(!frames.allocate(stack_base, kKernelThreadStackPages))
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

    relink_runnable_threads();
    return thread;
}

Thread* create_user_thread(Process* process,
                           uint64_t user_rip,
                           uint64_t user_rsp,
                           PageFrameContainer& frames)
{
    Thread* thread = nextFreeThread();
    if(nullptr == thread)
    {
        return nullptr;
    }

    uint64_t stack_base = 0;
    if(!frames.allocate(stack_base, kKernelThreadStackPages))
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

    relink_runnable_threads();
    return thread;
}

Thread* current_thread(void)
{
    return cpu_cur()->current_thread;
}

Thread* idle_thread(void)
{
    return g_idle_thread;
}

void set_current_thread(Thread* thread)
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

void mark_thread_ready(Thread* thread)
{
    if((nullptr != thread) && (ThreadState::Dying != thread->state) &&
       (ThreadState::Free != thread->state))
    {
        thread->state = ThreadState::Ready;
        if(thread->process && (ProcessState::Dying != thread->process->state))
        {
            thread->process->state = ProcessState::Ready;
        }
    }
}

void block_current_thread(ThreadWaitReason reason, uint64_t wait_address, uint64_t wait_length)
{
    Thread* thread = current_thread();
    if((nullptr == thread) || (ThreadState::Dying == thread->state) ||
       (ThreadState::Free == thread->state))
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
    relink_runnable_threads();
}

void clear_thread_wait(Thread* thread)
{
    if(nullptr == thread)
    {
        return;
    }

    thread->wait_reason = ThreadWaitReason::None;
    thread->wait_address = 0;
    thread->wait_length = 0;
}

Thread* first_blocked_thread(ThreadWaitReason reason)
{
    for(size_t i = 0; i < kMaxThreads; ++i)
    {
        if((ThreadState::Blocked == threadTable[i].state) && (reason == threadTable[i].wait_reason))
        {
            return threadTable + i;
        }
    }
    return nullptr;
}

void mark_current_thread_dying(int exit_status)
{
    Thread* thread = current_thread();
    if(nullptr == thread)
    {
        return;
    }

    thread->exit_status = exit_status;
    thread->state = ThreadState::Dying;
    clear_thread_wait(thread);
    if(thread->process)
    {
        thread->process->exit_status = exit_status;
        thread->process->state =
            thread->process->parent ? ProcessState::Zombie : ProcessState::Dying;
    }
    relink_runnable_threads();
}