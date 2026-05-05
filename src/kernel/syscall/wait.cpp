#include "syscall/wait.hpp"

#include "mm/user_copy.hpp"

namespace
{
Process* find_child_process_locked(Process* parent, uint64_t pid, bool zombie_only)
{
    if(nullptr == parent)
    {
        return nullptr;
    }

    for(Process* candidate = first_process(); nullptr != candidate; candidate = next_process(candidate))
    {
        if((candidate->parent != parent) || (ProcessState::Free == candidate->state) ||
           (ProcessState::Dying == candidate->state))
        {
            continue;
        }
        if(zombie_only && (ProcessState::Zombie != candidate->state))
        {
            continue;
        }
        if((0 == pid) || (candidate->pid == pid))
        {
            return candidate;
        }
    }
    return nullptr;
}

bool process_has_any_threads_locked(const Process* process)
{
    if(nullptr == process)
    {
        return false;
    }

    for(Thread* thread = first_thread(); nullptr != thread; thread = next_thread(thread))
    {
        if((thread->process == process) && (ThreadState::Free != thread->state))
        {
            return true;
        }
    }
    return false;
}
}  // namespace

bool try_complete_wait_pid(PageFrameContainer& frames,
                           Thread* thread,
                           uint64_t pid,
                           uint64_t user_status_pointer,
                           long& result)
{
    result = -1;
    if((nullptr == thread) || (nullptr == thread->process) || ((pid >> 63) != 0))
    {
        return true;
    }

    Process* child = nullptr;
    uint64_t child_pid = 0;
    int exit_status = 0;
    {
        IrqSpinGuard process_guard(g_process_table_lock);
        child = find_child_process_locked(thread->process, pid, true);
        {
            IrqSpinGuard thread_guard(g_thread_registry_lock);
            if((nullptr != child) && process_has_any_threads_locked(child))
            {
                return false;
            }
        }

        if(nullptr != child)
        {
            child_pid = child->pid;
            exit_status = child->exit_status;
        }
    }
    if(nullptr != child)
    {
        if((0 != user_status_pointer) &&
           !copy_to_user(frames, thread, user_status_pointer, &exit_status, sizeof(exit_status)))
        {
            return true;
        }

        result = static_cast<long>(child_pid);
        if(!reap_process(child, frames))
        {
            result = -1;
        }
        return true;
    }

    {
        IrqSpinGuard process_guard(g_process_table_lock);
        if(nullptr != find_child_process_locked(thread->process, pid, false))
        {
            return false;
        }
    }

    return true;
}

void wake_child_waiters(PageFrameContainer& frames)
{
    (void)frames;
}

void wake_child_waiters(PageFrameContainer& frames, Process* parent)
{
    if(nullptr == parent)
    {
        return;
    }

    Thread* pending = wait_queue_dequeue_all(parent->child_exit_waiters);
    Thread* still_waiting_head = nullptr;
    Thread* still_waiting_tail = nullptr;
    while(nullptr != pending)
    {
        Thread* thread = pending;
        pending = pending->wait_link;
        thread->wait_link = nullptr;

        long result = -1;
        if(try_complete_wait_pid(frames,
                                 thread,
                                 thread->wait.child_exit.pid,
                                 thread->wait.child_exit.user_status_pointer,
                                 result))
        {
            thread->frame.rax = static_cast<uint64_t>(result);
            wake_blocked_thread(thread);
            continue;
        }

        if(nullptr == still_waiting_head)
        {
            still_waiting_head = thread;
        }
        else
        {
            still_waiting_tail->wait_link = thread;
        }
        still_waiting_tail = thread;
    }

    if(nullptr != still_waiting_head)
    {
        IrqSpinGuard guard(parent->child_exit_waiters.lock);
        for(Thread* thread = still_waiting_head; nullptr != thread;)
        {
            Thread* next = thread->wait_link;
            thread->wait_link = nullptr;
            wait_queue_enqueue_locked(parent->child_exit_waiters, thread);
            thread = next;
        }
    }
}
