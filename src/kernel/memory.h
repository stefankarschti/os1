#ifndef _MEMORY_H_
#define _MEMORY_H_

#include <stdint.h>
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

inline int isprint(char c)
{
	return (c >= ' ');
}

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief memset
 * @param ptr
 * @param value
 * @param num number of bytes
 */
void memset(void *ptr, uint8_t value, uint64_t num);

/**
 * @brief memsetw
 * @param ptr
 * @param value
 * @param num number of bytes
 */
void memsetw(void* ptr, uint16_t value, uint64_t num);

/**
 * @brief memsetd
 * @param ptr
 * @param value
 * @param num number of bytes
 */
void memsetd(void* ptr, uint32_t value, uint64_t num);

/**
 * @brief memsetq
 * @param ptr
 * @param value
 * @param num number of bytes
 */
void memsetq(void* ptr, uint64_t value, uint64_t num);

void memcpy(void *dest, void *src, uint64_t len);

#ifdef __cplusplus
}
#endif

#endif

