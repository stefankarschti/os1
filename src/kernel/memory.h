#ifndef _MEMORY_H_
#define _MEMORY_H_

#include <stddef.h>

inline size_t strlen(const char* str) 
{
	size_t len = 0;
	while (str[len])
		len++;
	return len;
}
  
inline uint8_t inb(uint16_t port)
{
    uint8_t ret;
    asm volatile ( "inb %1, %0"
                   : "=a"(ret)
                   : "Nd"(port) );
    return ret;
}

inline void outb(uint16_t port, uint8_t val)
{
   asm volatile("outb %0, %1" : : "a"(val), "Nd"(port) );
}

#ifdef __cplusplus
extern "C" {
#endif

void memset(void *ptr, char value, uint64_t len);
void memsetw(void* ptr, uint16_t value, uint64_t num);
void memcpy(void *dest, void *src, uint64_t len);

#ifdef __cplusplus
}
#endif

#endif

