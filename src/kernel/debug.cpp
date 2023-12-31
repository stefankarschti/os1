#include "debug.h"
#include "memory.h"
#include <stdlib.h>

Debug debug;

Debug::Debug()
{
	InitSerial();
}

void Debug::InitSerial()
{
	outb(PORT + 1, 0x00);    // Disable all interrupts
	outb(PORT + 3, 0x80);    // Enable DLAB (set baud rate divisor)
	outb(PORT + 0, 0x01);    // Set divisor to 1 (lo byte) 115200 baud
	outb(PORT + 1, 0x00);    //                  (hi byte)
	outb(PORT + 3, 0x03);    // 8 bits, no parity, one stop bit
	outb(PORT + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
	outb(PORT + 4, 0x0B);    // IRQs enabled, RTS/DSR set
}

int Debug::Busy()
{
   return !(inb(PORT + 5) & 0x20);
}

void Debug::Write(const char c)
{
   while(Busy());
   outb(PORT, c);
}

void Debug::Write(const char *str)
{
	while(*str)
	{
		Write(*str++);
	}
}

void Debug::WriteLn(const char *str)
{
	Write(str);
	Write('\n');
}

void Debug::WriteInt(uint64_t value, int base, int minimum_digits)
{
	char temp[256];
	utoa(value, temp, base, minimum_digits);
	Write(temp);
}

void Debug::WriteIntLn(uint64_t value, int base, int minimum_digits)
{
	WriteInt(value, base, minimum_digits);
	Write('\n');
}

Debug &Debug::operator()()
{
	Write('\n');
	return *this;
}

Debug &Debug::operator()(const char *str)
{
	Write(str);
	return *this;
}

Debug &Debug::operator()(uint64_t value, int base, int minimum_digits)
{
	char temp[256];
	utoa(value, temp, base, minimum_digits);
	return (*this)(temp);
}

Debug &Debug::s(const char *str)
{
	Write(str);
	return *this;
}

Debug &Debug::u(uint64_t value, int base, int minimum_digits)
{
	char temp[256];
	utoa(value, temp, base, minimum_digits);
	return (*this)(temp);
}

Debug &Debug::nl()
{
	Write('\n');
	return *this;
}

void DebugMemory(uint64_t begin, uint64_t end)
{
	uint8_t* p = (uint8_t*)begin;
	uint8_t* e = (uint8_t*)end;
	// debug memory zone
	while(p < e)
	{
		debug((uint64_t)p, 16, 16)(":");
		for(int i = 0; i < 32; ++i)
		{
			if(0 == i % 8) debug(" ");
			debug(p[i], 16, 2);
		}
		debug(" ");
		char *s = (char*)p;
		for(int i = 0; i < 32; ++i)
		{
			if(s[i] >= 32 && s[i] < 0x7F)
				debug.Write(s[i]);
			else
				debug.Write('.');
		}
		debug.nl();
		p += 32;
	}
}
