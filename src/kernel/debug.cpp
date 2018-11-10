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
	outb(PORT + 0, 0x03);    // Set divisor to 3 (lo byte) 38400 baud
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
	itoa(value, temp, base, minimum_digits);
	Write(temp);
}

void Debug::WriteIntLn(uint64_t value, int base, int minimum_digits)
{
	WriteInt(value, base, minimum_digits);
	Write('\n');
}
