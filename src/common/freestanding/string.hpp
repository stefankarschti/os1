// Small freestanding byte/string helpers shared by early boot code and kernel
// code that cannot rely on the hosted C library.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace freestanding
{
inline void copy_bytes(void* destination, const void* source, size_t length)
{
    auto* dest = static_cast<uint8_t*>(destination);
    const auto* src = static_cast<const uint8_t*>(source);
    for(size_t i = 0; i < length; ++i)
    {
        dest[i] = src[i];
    }
}

inline void zero_bytes(void* destination, size_t length)
{
    auto* dest = static_cast<uint8_t*>(destination);
    for(size_t i = 0; i < length; ++i)
    {
        dest[i] = 0;
    }
}

[[nodiscard]] inline size_t string_length(const char* text)
{
    size_t length = 0;
    if(nullptr == text)
    {
        return 0;
    }
    while(text[length])
    {
        ++length;
    }
    return length;
}

inline void copy_string(char* destination, size_t capacity, const char* source)
{
    if((nullptr == destination) || (0 == capacity))
    {
        return;
    }

    destination[0] = 0;
    if(nullptr == source)
    {
        return;
    }

    const size_t source_length = string_length(source);
    const size_t copy_length = (source_length < (capacity - 1)) ? source_length : (capacity - 1);
    copy_bytes(destination, source, copy_length);
    destination[copy_length] = 0;
}

inline void append_string(char* destination, size_t capacity, const char* suffix)
{
    if((nullptr == destination) || (nullptr == suffix) || (0 == capacity))
    {
        return;
    }

    size_t destination_length = string_length(destination);
    while((destination_length + 1) < capacity && *suffix)
    {
        destination[destination_length++] = *suffix++;
    }
    destination[destination_length] = 0;
}

[[nodiscard]] inline bool strings_equal(const char* left, const char* right)
{
    if((nullptr == left) || (nullptr == right))
    {
        return false;
    }

    size_t index = 0;
    while(left[index] && right[index])
    {
        if(left[index] != right[index])
        {
            return false;
        }
        ++index;
    }

    return left[index] == right[index];
}

[[nodiscard]] inline bool path_ends_with(const char* path, const char* suffix)
{
    if((nullptr == path) || (nullptr == suffix))
    {
        return false;
    }

    const size_t path_length = string_length(path);
    const size_t suffix_length = string_length(suffix);
    if(path_length < suffix_length)
    {
        return false;
    }

    return strings_equal(path + path_length - suffix_length, suffix);
}
}  // namespace freestanding