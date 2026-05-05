// Kmem-backed thread-registry implementation. This file owns thread IDs,
// kernel stacks, saved TrapFrame initialization, wait-state transitions, and
// CPU-local current-thread publication.
#include "proc/thread.hpp"

#include "arch/x86_64/cpu/cpu.hpp"
#include "debug/debug.hpp"
#include "handoff/memory_layout.h"
#include "mm/kmem.hpp"
#include "sync/smp.hpp"
#include "util/memory.h"

namespace
{
constexpr char kThreadCacheName[] = "thread";

extern "C" void kernel_thread_start();
}  // namespace

OS1_BSP_ONLY Spinlock g_thread_registry_lock{"thread-registry"};

namespace
{

// BSP-only for now: TID allocation, idle-thread publication, and thread registry
// mutation have no SMP lock until APs leave the parked idle loop.
OS1_BSP_ONLY uint64_t g_next_tid = 1;
OS1_BSP_ONLY Thread* g_idle_thread = nullptr;
OS1_BSP_ONLY KmemCache* g_thread_cache = nullptr;
OS1_BSP_ONLY Thread* g_thread_head = nullptr;
OS1_BSP_ONLY Thread* g_thread_tail = nullptr;

Thread* allocate_thread_record()
{
    if(nullptr == g_thread_cache)
    {
        return nullptr;
    }
    return static_cast<Thread*>(kmem_cache_alloc(g_thread_cache, KmallocFlags::Zero));
}

void link_thread(Thread* thread)
{
    if(nullptr == thread)
    {
        return;
    }

    thread->registry_next = nullptr;
    if(nullptr != g_thread_tail)
    {
        g_thread_tail->registry_next = thread;
    }
    else
    {
        g_thread_head = thread;
    }
    g_thread_tail = thread;
}

void unlink_thread(Thread* thread)
{
    if((nullptr == thread) || (nullptr == g_thread_head))
    {
        return;
    }

    Thread* previous = nullptr;
    for(Thread* candidate = g_thread_head; nullptr != candidate; candidate = candidate->registry_next)
    {
        if(candidate != thread)
        {
            previous = candidate;
            continue;
        }

        Thread* next = candidate->registry_next;
        if(nullptr != previous)
        {
            previous->registry_next = next;
        }
        else
        {
            g_thread_head = next;
        }
        if(g_thread_tail == candidate)
        {
            g_thread_tail = previous;
        }
        candidate->registry_next = nullptr;
        return;
    }
}

void release_thread_record(Thread* thread)
{
    if((nullptr == thread) || (nullptr == g_thread_cache))
    {
        return;
    }

    memset(thread, 0, sizeof(Thread));
    thread->state = ThreadState::Free;
    kmem_cache_free(g_thread_cache, thread);
}

bool initialize_thread_table(PageFrameContainer& frames)
{
    KASSERT_ON_BSP();
    (void)frames;
    g_next_tid = 1;
    g_idle_thread = nullptr;
    g_thread_head = nullptr;
    g_thread_tail = nullptr;

    g_thread_cache = kmem_cache_create(kThreadCacheName, sizeof(Thread), alignof(Thread));
    if(nullptr == g_thread_cache)
    {
        debug("thread cache creation failed")();
        return false;
    }
    return true;
}
}  // namespace

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

Thread* first_thread(void)
{
    return g_thread_head;
}

Thread* next_thread(const Thread* thread)
{
    return (nullptr != thread) ? thread->registry_next : nullptr;
}

void relink_runnable_threads()
{
    KASSERT_ON_BSP();

    Thread* first = nullptr;
    Thread* last = nullptr;
    for(Thread* thread = first_thread(); nullptr != thread; thread = next_thread(thread))
    {
        thread->next = nullptr;
        if((ThreadState::Ready == thread->state) || (ThreadState::Running == thread->state))
        {
            if(nullptr == first)
            {
                first = thread;
            }
            if(last)
            {
                last->next = thread;
            }
            last = thread;
        }
    }
    if(last)
    {
        last->next = first;
    }
}

void clear_thread(Thread* thread)
{
    if(nullptr == thread)
    {
        return;
    }

    if(thread == g_idle_thread)
    {
        g_idle_thread = nullptr;
    }
    unlink_thread(thread);
    release_thread_record(thread);
}

Thread* create_kernel_thread(Process* process, void (*entry)(void), PageFrameContainer& frames)
{
    KASSERT_ON_BSP();
    if((nullptr == process) || (nullptr == entry))
    {
        return nullptr;
    }

    Thread* thread = allocate_thread_record();
    if(nullptr == thread)
    {
        return nullptr;
    }

    uint64_t stack_base = 0;
    if(!frames.allocate(stack_base, kKernelThreadStackPages))
    {
        release_thread_record(thread);
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

    link_thread(thread);
    relink_runnable_threads();
    return thread;
}

Thread* create_user_thread(Process* process,
                           uint64_t user_rip,
                           uint64_t user_rsp,
                           PageFrameContainer& frames,
                           bool start_ready)
{
    KASSERT_ON_BSP();
    if(nullptr == process)
    {
        return nullptr;
    }

    Thread* thread = allocate_thread_record();
    if(nullptr == thread)
    {
        return nullptr;
    }

    uint64_t stack_base = 0;
    if(!frames.allocate(stack_base, kKernelThreadStackPages))
    {
        release_thread_record(thread);
        return nullptr;
    }
    uint8_t* stack_memory = kernel_physical_pointer<uint8_t>(stack_base);
    memset(stack_memory, 0, kKernelThreadStackPages * kPageSize);
    const uint64_t stack_top = (uint64_t)(stack_memory + kKernelThreadStackPages * kPageSize);

    thread->tid = g_next_tid++;
    thread->process = process;
    thread->state = start_ready ? ThreadState::Ready : ThreadState::Blocked;
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

    link_thread(thread);
    if(start_ready)
    {
        relink_runnable_threads();
    }
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
    for(Thread* thread = first_thread(); nullptr != thread; thread = next_thread(thread))
    {
        if((ThreadState::Blocked == thread->state) && (reason == thread->wait.reason))
        {
            return thread;
        }
    }
    return nullptr;
}

void wake_block_io_waiters(uint64_t completion_flag)
{
    if(0 == completion_flag)
    {
        return;
    }

    for(Thread* thread = first_thread(); nullptr != thread; thread = next_thread(thread))
    {
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
