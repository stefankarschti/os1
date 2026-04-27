// Console stream implementation shared by kernel status messages and syscall
// write plumbing.
#include "console/console.h"

#include "core/kernel_state.h"
#include "debug/debug.h"

void WriteConsoleBytes(const char *data, size_t length)
{
	if(nullptr == data)
	{
		return;
	}

	for(size_t i = 0; i < length; ++i)
	{
		debug.Write(data[i]);
		if(active_terminal)
		{
			active_terminal->Write(data[i]);
		}
	}
}

void WriteConsoleLine(const char *text)
{
	if(nullptr == text)
	{
		return;
	}

	debug.WriteLn(text);
	if(active_terminal)
	{
		active_terminal->WriteLn(text);
	}
}