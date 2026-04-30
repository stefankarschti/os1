// Thread-table implementation. This file owns thread IDs, kernel stacks, saved
// TrapFrame initialization, wait-state transitions, and CPU-local current-thread
// publication.
#include "proc/thread.hpp"

#include "arch/x86_64/cpu/cpu.hpp"
#include "debug/debug.hpp"
#include "handoff/memory_layout.h"
#include "sync/smp.hpp"
#include "util/memory.h"

namespace
{
constexpr size_t kThreadTablePageCount = (kMaxThreads * sizeof(Thread) + kPageSize - 1) / kPageSize;

extern "C" void kernel_thread_start();

// BSP-only for now: TID allocation, idle-thread publication, and thread table
// mutation have no SMP lock until APs leave the parked idle loop.
OS1_BSP_ONLY uint64_t g_next_tid = 1;
OS1_BSP_ONLY Thread* g_idle_thread = nullptr;

Thread* next_free_thread()
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

bool initialize_thread_table(PageFrameContainer& frames)
{
    KASSERT_ON_BSP();
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
        threadTable = kernel_physical_pointer<Thread>(thread_table_address);
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

OS1_BSP_ONLY Thread* threadTable = nullptr;

bool init_tasks(PageFrameContainer& frames)
{
    KASSERT_ON_BSP();
    if(!initialize_process_table(frames) || !initialize_thread_table(frames))
    {
        return false;
    }
    set_current_thread(nullptr);
    return true;
}

void relink_runnable_threads()
{
    KASSERT_ON_BSP();
    if(nullptr == threadTable)
    {
        return;
    }

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
    KASSERT_ON_BSP();
    Thread* thread = next_free_thread();
    if((nullptr == thread) || (nullptr == process) || (nullptr == entry))
    {
        return nullptr;
    }

    uint64_t stack_base = 0;
    if(!frames.allocate(stack_base, kKernelThreadStackPages))
    {
        return nullptr;
    }
    uint8_t* stack_memory = kernel_physical_pointer<uint8_t>(stack_base);
    memset(stack_memory, 0, kKernelThreadStackPages * kPageSize);
    const uint64_t stack_top = (uint64_t)(stack_memory + kKernelThreadStackPages * kPageSize);

    thread->tid = g_next_tid++;
    thread->process = process;
    thread->state = ThreadState::Ready;
    thread->user_mode = false;
    thread->address_space_cr3 = process->address_space.cr3;
    thread->kernel_stack_base = stack_base;
    thread->kernel_stack_top = stack_top;
    thread->exit_status = 0;
    thread->frame = {};
    // Kernel threads resume through `iretq` into a small trampoline that aligns
    // the stack before calling the C++ entry point carried in RDI.
    *reinterpret_cast<uint64_t*>(thread->kernel_stack_top - 3 * sizeof(uint64_t)) = 0;
    thread->frame.rip = (uint64_t)kernel_thread_start;
    thread->frame.rdi = (uint64_t)entry;
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
    KASSERT_ON_BSP();
    Thread* thread = next_free_thread();
    if(nullptr == thread)
    {
        return nullptr;
    }

    uint64_t stack_base = 0;
    if(!frames.allocate(stack_base, kKernelThreadStackPages))
    {
        return nullptr;
    }
    uint8_t* stack_memory = kernel_physical_pointer<uint8_t>(stack_base);
    memset(stack_memory, 0, kKernelThreadStackPages * kPageSize);
    const uint64_t stack_top = (uint64_t)(stack_memory + kKernelThreadStackPages * kPageSize);

    thread->tid = g_next_tid++;
    thread->process = process;
    thread->state = ThreadState::Ready;
    thread->user_mode = true;
    thread->address_space_cr3 = process->address_space.cr3;
    thread->kernel_stack_base = stack_base;
    thread->kernel_stack_top = stack_top;
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
    KASSERT_ON_BSP();
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

static void block_current_thread(ThreadWaitState wait)
{
    KASSERT_ON_BSP();
    Thread* thread = current_thread();
    if((nullptr == thread) || (ThreadState::Dying == thread->state) ||
       (ThreadState::Free == thread->state))
    {
        return;
    }

    thread->wait = wait;
    thread->state = ThreadState::Blocked;
    if(thread->process && (ProcessState::Dying != thread->process->state))
    {
        thread->process->state = ProcessState::Ready;
    }
    relink_runnable_threads();
}

void block_current_thread_on_console_read(uint64_t user_buffer, uint64_t length)
{
    ThreadWaitState wait{};
    wait.reason = ThreadWaitReason::ConsoleRead;
    wait.console_read = ConsoleReadWaitState{
        .user_buffer = user_buffer,
        .length = length,
    };
    block_current_thread(wait);
}

void block_current_thread_on_child_exit(uint64_t user_status_pointer, uint64_t pid)
{
    ThreadWaitState wait{};
    wait.reason = ThreadWaitReason::ChildExit;
    wait.child_exit = ChildExitWaitState{
        .user_status_pointer = user_status_pointer,
        .pid = pid,
    };
    block_current_thread(wait);
}

void block_current_thread_on_block_io(uint64_t completion_flag)
{
    ThreadWaitState wait{};
    wait.reason = ThreadWaitReason::BlockIo;
    wait.block_io = BlockIoWaitState{
        .completion_flag = completion_flag,
    };
    block_current_thread(wait);
}

void clear_thread_wait(Thread* thread)
{
    if(nullptr == thread)
    {
        return;
    }

    thread->wait = ThreadWaitState{};
}

void wake_blocked_thread(Thread* thread)
{
    KASSERT_ON_BSP();
    if(nullptr == thread)
    {
        return;
    }

    clear_thread_wait(thread);
    if(thread == current_thread())
    {
        thread->state = ThreadState::Running;
        if(thread->process && (ProcessState::Dying != thread->process->state))
        {
            thread->process->state = ProcessState::Running;
        }
        return;
    }

    mark_thread_ready(thread);
}

Thread* first_blocked_thread(ThreadWaitReason reason)
{
    if(nullptr == threadTable)
    {
        return nullptr;
    }

    for(size_t i = 0; i < kMaxThreads; ++i)
    {
        if((ThreadState::Blocked == threadTable[i].state) && (reason == threadTable[i].wait.reason))
        {
            return threadTable + i;
        }
    }
    return nullptr;
}

void wake_block_io_waiters(uint64_t completion_flag)
{
    if((0 == completion_flag) || (nullptr == threadTable))
    {
        return;
    }

    for(size_t i = 0; i < kMaxThreads; ++i)
    {
        Thread* thread = threadTable + i;
        if((ThreadState::Blocked != thread->state) ||
           (ThreadWaitReason::BlockIo != thread->wait.reason) ||
           (completion_flag != thread->wait.block_io.completion_flag))
        {
            continue;
        }

        wake_blocked_thread(thread);
    }
}

void mark_current_thread_dying(int exit_status)
{
    KASSERT_ON_BSP();
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
