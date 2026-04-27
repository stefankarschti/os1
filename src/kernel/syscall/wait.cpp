#include "syscall/wait.hpp"

#include "mm/user_copy.hpp"

namespace
{
Process* FindChildProcess(Process* parent, uint64_t pid, bool zombie_only)
{
    if(nullptr == parent)
    {
        return nullptr;
    }

    for(size_t i = 0; i < kMaxProcesses; ++i)
    {
        Process* candidate = processTable + i;
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

bool ProcessHasAnyThreads(const Process* process)
{
    if(nullptr == process)
    {
        return false;
    }

    for(size_t i = 0; i < kMaxThreads; ++i)
    {
        if((threadTable[i].process == process) && (ThreadState::Free != threadTable[i].state))
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

    Process* child = FindChildProcess(thread->process, pid, true);
    if(nullptr != child)
    {
        if(ProcessHasAnyThreads(child))
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

    if(nullptr != FindChildProcess(thread->process, pid, false))
    {
        return false;
    }

    return true;
}

void wake_child_waiters(PageFrameContainer& frames)
{
    for(size_t i = 0; i < kMaxThreads; ++i)
    {
        Thread* thread = threadTable + i;
        if((ThreadState::Blocked != thread->state) ||
           (ThreadWaitReason::ChildExit != thread->wait_reason))
        {
            continue;
        }

        long result = -1;
        if(!try_complete_wait_pid(
               frames, thread, thread->wait_length, thread->wait_address, result))
        {
            continue;
        }

        clear_thread_wait(thread);
        thread->frame.rax = static_cast<uint64_t>(result);
        mark_thread_ready(thread);
    }
}