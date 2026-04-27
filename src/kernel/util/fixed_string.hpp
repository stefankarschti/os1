// Fixed-size string copy helper for kernel records that must remain fully
// nul-padded for stable observability and deterministic table state.
#pragma once

#include <stddef.h>

// copy `source` into a fixed buffer and pad all remaining bytes with zeroes.
inline void copy_fixed_string(char* destination, size_t destination_size, const char* source)
{
    if((nullptr == destination) || (0 == destination_size))
    {
        return;
    }

    size_t index = 0;
    if(nullptr != source)
    {
        while(((index + 1) < destination_size) && source[index])
        {
            destination[index] = source[index];
            ++index;
        }
    }
    while(index < destination_size)
    {
        destination[index++] = 0;
    }
}
