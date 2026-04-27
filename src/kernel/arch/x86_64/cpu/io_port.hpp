// Narrow x86_64 port-I/O helpers. Generic kernel code should include this only
// when it is deliberately touching legacy I/O ports or PCI configuration ports.
#pragma once

#include <stdint.h>

// Read one byte from an x86 I/O port.
static inline uint8_t inb(int port)
{
    uint8_t data;
    __asm __volatile("inb %w1,%0" : "=a"(data) : "d"(port));
    return data;
}

// Read `cnt` bytes from an x86 I/O port into memory.
static inline void insb(int port, void* addr, int cnt)
{
    __asm __volatile("cld\n\trepne\n\tinsb"
                     : "=D"(addr), "=c"(cnt)
                     : "d"(port), "0"(addr), "1"(cnt)
                     : "memory", "cc");
}

// Read one 16-bit word from an x86 I/O port.
static inline uint16_t inw(int port)
{
    uint16_t data;
    __asm __volatile("inw %w1,%0" : "=a"(data) : "d"(port));
    return data;
}

// Read `cnt` 16-bit words from an x86 I/O port into memory.
static inline void insw(int port, void* addr, int cnt)
{
    __asm __volatile("cld\n\trepne\n\tinsw"
                     : "=D"(addr), "=c"(cnt)
                     : "d"(port), "0"(addr), "1"(cnt)
                     : "memory", "cc");
}

// Read one 32-bit doubleword from an x86 I/O port.
static inline uint32_t inl(int port)
{
    uint32_t data;
    __asm __volatile("inl %w1,%0" : "=a"(data) : "d"(port));
    return data;
}

// Read `cnt` 32-bit doublewords from an x86 I/O port into memory.
static inline void insl(int port, void* addr, int cnt)
{
    __asm __volatile("cld\n\trepne\n\tinsl"
                     : "=D"(addr), "=c"(cnt)
                     : "d"(port), "0"(addr), "1"(cnt)
                     : "memory", "cc");
}

// write one byte to an x86 I/O port.
static inline void outb(int port, uint8_t data)
{
    __asm __volatile("outb %0,%w1" : : "a"(data), "d"(port));
}

// write `cnt` bytes from memory to an x86 I/O port.
static inline void outsb(int port, const void* addr, int cnt)
{
    __asm __volatile("cld\n\trepne\n\toutsb"
                     : "=S"(addr), "=c"(cnt)
                     : "d"(port), "0"(addr), "1"(cnt)
                     : "cc");
}

// write one 16-bit word to an x86 I/O port.
static inline void outw(int port, uint16_t data)
{
    __asm __volatile("outw %0,%w1" : : "a"(data), "d"(port));
}

// write `cnt` 16-bit words from memory to an x86 I/O port.
static inline void outsw(int port, const void* addr, int cnt)
{
    __asm __volatile("cld\n\trepne\n\toutsw"
                     : "=S"(addr), "=c"(cnt)
                     : "d"(port), "0"(addr), "1"(cnt)
                     : "cc");
}

// write one 32-bit doubleword to an x86 I/O port.
static inline void outl(int port, uint32_t data)
{
    __asm __volatile("outl %0,%w1" : : "a"(data), "d"(port));
}

// write `cnt` 32-bit doublewords from memory to an x86 I/O port.
static inline void outsl(int port, const void* addr, int cnt)
{
    __asm __volatile("cld\n\trepne\n\toutsl"
                     : "=S"(addr), "=c"(cnt)
                     : "d"(port), "0"(addr), "1"(cnt)
                     : "cc");
}
