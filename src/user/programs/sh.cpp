#include <os1/syscall.h>

#include <stddef.h>
#include <stdint.h>

namespace
{
constexpr size_t kShellLineBytes = 128;
constexpr size_t kShellMaxTokens = 16;

size_t StringLength(const char *text)
{
	size_t length = 0;
	while((nullptr != text) && text[length])
	{
		++length;
	}
	return length;
}

void WriteBytes(const char *data, size_t length)
{
	if((nullptr == data) || (0 == length))
	{
		return;
	}
	os1_write(1, data, length);
}

void WriteString(const char *text)
{
	WriteBytes(text, StringLength(text));
}

void WriteChar(char c)
{
	WriteBytes(&c, 1);
}

bool IsSpace(char c)
{
	return (' ' == c) || ('\t' == c) || ('\n' == c) || ('\r' == c);
}

bool StringsEqual(const char *lhs, const char *rhs)
{
	if((nullptr == lhs) || (nullptr == rhs))
	{
		return false;
	}
	for(size_t index = 0;; ++index)
	{
		if(lhs[index] != rhs[index])
		{
			return false;
		}
		if(0 == lhs[index])
		{
			return true;
		}
	}
}

void WriteUnsigned(uint64_t value)
{
	char digits[21];
	size_t index = 0;
	do
	{
		digits[index++] = (char)('0' + (value % 10));
		value /= 10;
	}
	while(value > 0);

	while(index > 0)
	{
		WriteChar(digits[--index]);
	}
}

size_t NormalizeLine(char *buffer, long count)
{
	size_t length = (count > 0) ? (size_t)count : 0;
	if(length >= kShellLineBytes)
	{
		length = kShellLineBytes - 1;
	}
	while((length > 0) && (('\n' == buffer[length - 1]) || ('\r' == buffer[length - 1])))
	{
		--length;
	}
	buffer[length] = 0;
	return length;
}

size_t Tokenize(char *buffer, char *tokens[kShellMaxTokens])
{
	size_t count = 0;
	size_t index = 0;
	while(buffer[index])
	{
		while(buffer[index] && IsSpace(buffer[index]))
		{
			buffer[index++] = 0;
		}
		if(0 == buffer[index])
		{
			break;
		}
		if(count == kShellMaxTokens)
		{
			return count;
		}
		tokens[count++] = buffer + index;
		while(buffer[index] && !IsSpace(buffer[index]))
		{
			++index;
		}
	}
	return count;
}

void WritePrompt()
{
	WriteString("os1> ");
}

void RunHelp()
{
	WriteString("help echo pid exit\n");
}

void RunEcho(size_t argc, char *argv[kShellMaxTokens])
{
	WriteString("echo:");
	for(size_t i = 1; i < argc; ++i)
	{
		WriteChar(' ');
		WriteString(argv[i]);
	}
	WriteChar('\n');
}

void RunPid()
{
	WriteString("pid: ");
	const long pid = os1_getpid();
	if(pid < 0)
	{
		WriteString("error\n");
		return;
	}
	WriteUnsigned((uint64_t)pid);
	WriteChar('\n');
}

void RunUnknown(const char *command)
{
	WriteString("unknown command: ");
	WriteString(command);
	WriteChar('\n');
}
}

int main(void)
{
	char line[kShellLineBytes];
	char *tokens[kShellMaxTokens];

	WriteString("shell prompt ready\n");
	for(;;)
	{
		WritePrompt();
		const long count = os1_read(0, line, sizeof(line));
		if(count <= 0)
		{
			WriteString("shell read failed\n");
			os1_yield();
			continue;
		}

		for(size_t i = 0; i < kShellMaxTokens; ++i)
		{
			tokens[i] = nullptr;
		}
		NormalizeLine(line, count);
		const size_t argc = Tokenize(line, tokens);
		if(0 == argc)
		{
			continue;
		}

		if(StringsEqual(tokens[0], "help"))
		{
			RunHelp();
		}
		else if(StringsEqual(tokens[0], "echo"))
		{
			RunEcho(argc, tokens);
		}
		else if(StringsEqual(tokens[0], "pid"))
		{
			RunPid();
		}
		else if(StringsEqual(tokens[0], "exit"))
		{
			return 0;
		}
		else
		{
			RunUnknown(tokens[0]);
		}
	}
}
