#ifndef _STDLIB_H_
#define _STDLIB_H_

#include <stdint.h>
#include <stddef.h>

char* itoa(uint64_t value, char *str, int base, int minimum_digits = 1);

template<typename T>
T min(T a, T b)
{
	return a < b ? a : b;
}

template<typename T>
T max(T a, T b)
{
	return a < b ? b : a;
}

#endif

