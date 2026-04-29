#include <stdint.h>

extern "C"
{
void memsetw(void* ptr, uint16_t value, uint64_t num)
{
    auto* words = static_cast<uint16_t*>(ptr);
    for(uint64_t i = 0; i < (num / sizeof(uint16_t)); ++i)
    {
        words[i] = value;
    }
}

void memsetd(void* ptr, uint32_t value, uint64_t num)
{
    auto* dwords = static_cast<uint32_t*>(ptr);
    for(uint64_t i = 0; i < (num / sizeof(uint32_t)); ++i)
    {
        dwords[i] = value;
    }
}

void memsetq(void* ptr, uint64_t value, uint64_t num)
{
    auto* qwords = static_cast<uint64_t*>(ptr);
    for(uint64_t i = 0; i < (num / sizeof(uint64_t)); ++i)
    {
        qwords[i] = value;
    }
}
}
