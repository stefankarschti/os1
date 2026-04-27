// Canonical line-input implementation shared by serial and keyboard sources.
// It maintains a small completed-line queue for blocking read syscalls.
#include "console/console_input.h"

#include <stdlib.h>

#include "debug/debug.h"
#include "arch/x86_64/cpu/io_port.h"
#include "util/ctype.h"
#include "util/memory.h"
#include "console/terminal.h"

extern Terminal *active_terminal;

namespace
{
constexpr uint16_t kSerialPortBase = 0x3F8;
constexpr uint16_t kSerialLineStatusPort = kSerialPortBase + 5;
constexpr uint8_t kSerialDataReady = 0x01;
constexpr size_t kConsoleInputCompletedLineCapacity = 8;

char g_pending_line[kConsoleInputMaxLineBytes]{};
size_t g_pending_length = 0;
char g_completed_lines[kConsoleInputCompletedLineCapacity][kConsoleInputMaxLineBytes]{};
size_t g_completed_lengths[kConsoleInputCompletedLineCapacity]{};
size_t g_completed_head = 0;
size_t g_completed_tail = 0;
size_t g_completed_count = 0;

void EchoByte(char c)
{
	debug.Write(c);
	if(active_terminal)
	{
		active_terminal->Write(c);
	}
}

void EchoBackspace()
{
	debug.Write('\b');
	debug.Write(' ');
	debug.Write('\b');
	if(active_terminal)
	{
		active_terminal->Write('\b');
	}
}

void CommitPendingLine()
{
	if(g_completed_count == kConsoleInputCompletedLineCapacity)
	{
		debug("console input line dropped: queue full")();
		g_pending_length = 0;
		return;
	}

	memcpy(g_completed_lines[g_completed_tail], g_pending_line, g_pending_length);
	g_completed_lines[g_completed_tail][g_pending_length] = '\n';
	g_completed_lengths[g_completed_tail] = g_pending_length + 1;
	g_completed_tail = (g_completed_tail + 1) % kConsoleInputCompletedLineCapacity;
	++g_completed_count;
	g_pending_length = 0;
}

void HandleInputChar(char ascii)
{
	if('\r' == ascii)
	{
		ascii = '\n';
	}
	else if((0x7F == static_cast<unsigned char>(ascii)) || ('\b' == ascii))
	{
		ascii = '\b';
	}

	if('\b' == ascii)
	{
		if(g_pending_length > 0)
		{
			--g_pending_length;
			EchoBackspace();
		}
		return;
	}

	if('\n' == ascii)
	{
		EchoByte('\n');
		CommitPendingLine();
		return;
	}

	if(!isprint(ascii))
	{
		return;
	}

	if((g_pending_length + 1) >= kConsoleInputMaxLineBytes)
	{
		debug("console input line dropped: line too long")();
		return;
	}

	g_pending_line[g_pending_length++] = ascii;
	EchoByte(ascii);
}
}

void ConsoleInputInitialize()
{
	memset(g_pending_line, 0, sizeof(g_pending_line));
	memset(g_completed_lines, 0, sizeof(g_completed_lines));
	memset(g_completed_lengths, 0, sizeof(g_completed_lengths));
	g_pending_length = 0;
	g_completed_head = 0;
	g_completed_tail = 0;
	g_completed_count = 0;
}

void ConsoleInputOnKeyboardChar(char ascii)
{
	HandleInputChar(ascii);
}

void ConsoleInputPollSerial()
{
	while((inb(kSerialLineStatusPort) & kSerialDataReady) != 0)
	{
		HandleInputChar((char)inb(kSerialPortBase));
	}
}

bool ConsoleInputHasLine()
{
	return g_completed_count > 0;
}

bool ConsoleInputPopLine(char *buffer, size_t buffer_size, size_t &line_length)
{
	line_length = 0;
	if((nullptr == buffer) || (0 == buffer_size) || (0 == g_completed_count))
	{
		return false;
	}

	const size_t next_length = g_completed_lengths[g_completed_head];
	if(next_length > buffer_size)
	{
		return false;
	}

	memcpy(buffer, g_completed_lines[g_completed_head], next_length);
	memset(g_completed_lines[g_completed_head], 0, sizeof(g_completed_lines[g_completed_head]));
	g_completed_lengths[g_completed_head] = 0;
	g_completed_head = (g_completed_head + 1) % kConsoleInputCompletedLineCapacity;
	--g_completed_count;
	line_length = next_length;
	return true;
}
