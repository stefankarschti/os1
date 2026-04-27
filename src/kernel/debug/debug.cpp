#include "debug/debug.hpp"

#include <stdlib.h>

#include "arch/x86_64/cpu/io_port.hpp"
#include "util/memory.h"

Debug debug;

Debug::Debug()
{
    init_serial();
}

void Debug::init_serial()
{
    outb(PORT + 1, 0x00);  // Disable all interrupts
    outb(PORT + 3, 0x80);  // Enable DLAB (set baud rate divisor)
    outb(PORT + 0, 0x01);  // Set divisor to 1 (lo byte) 115200 baud
    outb(PORT + 1, 0x00);  //                  (hi byte)
    outb(PORT + 3, 0x03);  // 8 bits, no parity, one stop bit
    outb(PORT + 2, 0xC7);  // Enable FIFO, clear them, with 14-byte threshold
    outb(PORT + 4, 0x0B);  // IRQs enabled, RTS/DSR set
}

int Debug::busy()
{
    return !(inb(PORT + 5) & 0x20);
}

void Debug::write(const char c)
{
    while(busy())
        ;
    outb(PORT, c);
}

void Debug::write(const char* str)
{
    while(*str)
    {
        write(*str++);
    }
}

void Debug::write_line(const char* str)
{
    write(str);
    write('\n');
}

void Debug::write_int(uint64_t value, int base, int minimum_digits)
{
    char temp[256];
    utoa(value, temp, base, minimum_digits);
    write(temp);
}

void Debug::write_int_line(uint64_t value, int base, int minimum_digits)
{
    write_int(value, base, minimum_digits);
    write('\n');
}

Debug& Debug::operator()()
{
    write('\n');
    return *this;
}

Debug& Debug::operator()(const char* str)
{
    write(str);
    return *this;
}

Debug& Debug::operator()(uint64_t value, int base, int minimum_digits)
{
    char temp[256];
    utoa(value, temp, base, minimum_digits);
    return (*this)(temp);
}

Debug& Debug::s(const char* str)
{
    write(str);
    return *this;
}

Debug& Debug::u(uint64_t value, int base, int minimum_digits)
{
    char temp[256];
    utoa(value, temp, base, minimum_digits);
    return (*this)(temp);
}

Debug& Debug::nl()
{
    write('\n');
    return *this;
}

void debug_memory(uint64_t begin, uint64_t end)
{
    uint8_t* p = (uint8_t*)begin;
    uint8_t* e = (uint8_t*)end;
    // debug memory zone
    while(p < e)
    {
        debug((uint64_t)p, 16, 16)(":");
        for(int i = 0; i < 32; ++i)
        {
            if(0 == i % 8)
                debug(" ");
            debug(p[i], 16, 2);
        }
        debug(" ");
        char* s = (char*)p;
        for(int i = 0; i < 32; ++i)
        {
            if(s[i] >= 32 && s[i] < 0x7F)
                debug.write(s[i]);
            else
                debug.write('.');
        }
        debug.nl();
        p += 32;
    }
}
