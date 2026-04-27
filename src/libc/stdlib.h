#pragma once

#include <stddef.h>
#include <stdint.h>

char* utoa(uint64_t value, char* str, int base = 10, int minimum_digits = 1);

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
