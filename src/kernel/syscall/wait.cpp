#include "syscall/wait.hpp"

#include "mm/user_copy.hpp"

namespace
{
Process* find_child_process(Process* parent, uint64_t pid, bool zombie_only)
{
    if(nullptr == parent)
    {
        return nullptr;
    }

    for(Process* candidate = first_process(); nullptr != candidate; candidate = next_process(candidate))
    {
        if((candidate->parent != parent) || (ProcessState::Free == candidate->state))
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

bool process_has_any_threads(const Process* process)
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

    Process* child = find_child_process(thread->process, pid, true);
    if(nullptr != child)
    {
        if(process_has_any_threads(child))
        {
            return false;
        }

        const int exit_status = child->exit_status;
        if((0 != user_status_pointer) &&
           !copy_to_user(frames, thread, user_status_pointer, &exit_status, sizeof(exit_status)))
        {
            return true;
        }

        result = static_cast<long>(child->pid);
        if(!reap_process(child, frames))
        {
            result = -1;
        }
        return true;
    }

    if(nullptr != find_child_process(thread->process, pid, false))
    {
        return false;
    }

    return true;
}

void wake_child_waiters(PageFrameContainer& frames)
{
    for(Thread* thread = first_thread(); nullptr != thread; thread = next_thread(thread))
    {
        if((ThreadState::Blocked != thread->state) ||
           (ThreadWaitReason::ChildExit != thread->wait.reason))
        {
            continue;
        }

        long result = -1;
        if(!try_complete_wait_pid(frames,
                                  thread,
                                  thread->wait.child_exit.pid,
                                  thread->wait.child_exit.user_status_pointer,
                                  result))
        {
            continue;
        }

        thread->frame.rax = static_cast<uint64_t>(result);
        wake_blocked_thread(thread);
    }
}
