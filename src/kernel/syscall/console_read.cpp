#include "syscall/console_read.h"

#include "console/console_input.h"
#include "mm/user_copy.h"

bool TryCompleteConsoleRead(PageFrameContainer &frames,
		Thread *thread,
		uint64_t user_buffer,
		size_t length,
		long &result)
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
	if(!ConsoleInputHasLine())
	{
		return false;
	}

	char line[kConsoleInputMaxLineBytes];
	size_t line_length = 0;
	if(!ConsoleInputPopLine(line, sizeof(line), line_length))
	{
		return false;
	}
	if((line_length > length) || !CopyToUser(frames, thread, user_buffer, line, line_length))
	{
		return true;
	}

	result = (long)line_length;
	return true;
}


void WakeConsoleReaders(PageFrameContainer &frames)
{
	while(ConsoleInputHasLine())
	{
		Thread *thread = firstBlockedThread(ThreadWaitReason::ConsoleRead);
		if(nullptr == thread)
		{
			return;
		}

		long result = -1;
		if(!TryCompleteConsoleRead(frames, thread, thread->wait_address, (size_t)thread->wait_length, result))
		{
			return;
		}

		clearThreadWait(thread);
		thread->frame.rax = (uint64_t)result;
		markThreadReady(thread);
	}
}