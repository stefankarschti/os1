#include "syscall/console_read.hpp"

#include "console/console_input.hpp"
#include "mm/user_copy.hpp"
#include "sync/wait_queue.hpp"

bool try_complete_console_read(
    PageFrameContainer& frames, Thread* thread, uint64_t user_buffer, size_t length, long& result)
{
    result = -1;
    if(nullptr == thread)
    {
        return true;
    }
    if((0 == user_buffer) || (0 == length))
    {
        return true;
    }
    if(!console_input_has_line())
    {
        return false;
    }

    char line[kConsoleInputMaxLineBytes];
    size_t line_length = 0;
    if(!console_input_pop_line(line, sizeof(line), line_length))
    {
        return false;
    }
    if((line_length > length) || !copy_to_user(frames, thread, user_buffer, line, line_length))
    {
        return true;
    }

    result = (long)line_length;
    return true;
}

void wake_console_readers(PageFrameContainer& frames)
{
    while(console_input_has_line())
    {
        Thread* thread = wait_queue_dequeue(console_input_read_wait_queue());
        if(nullptr == thread)
        {
            return;
        }

        long result = -1;
        if(!try_complete_console_read(frames,
                                      thread,
                                      thread->wait.console_read.user_buffer,
                                      (size_t)thread->wait.console_read.length,
                                      result))
        {
            wait_queue_enqueue(console_input_read_wait_queue(), thread);
            return;
        }

        thread->frame.rax = (uint64_t)result;
        wake_blocked_thread(thread);
    }
}
