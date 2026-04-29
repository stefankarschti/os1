// Audited syscall copy boundary. Every user pointer is canonicalized, range
// checked, translated, and permission checked before bytes cross into kernel code.
#include "mm/user_copy.hpp"

#include "handoff/memory_layout.h"
#include "mm/user_address.hpp"
#include "util/memory.h"

bool copy_into_address_space(VirtualMemory& vm,
                             uint64_t virtual_address,
                             const uint8_t* source,
                             uint64_t length)
{
    uint64_t copied = 0;
    while(copied < length)
    {
        uint64_t physical = 0;
        uint64_t flags = 0;
        if(!vm.translate(virtual_address + copied, physical, flags))
        {
            return false;
        }

        const uint64_t page_offset = (virtual_address + copied) & (kPageSize - 1);
        const uint64_t chunk = ((length - copied) < (kPageSize - page_offset))
                                   ? (length - copied)
                                   : (kPageSize - page_offset);
        memcpy(kernel_physical_pointer<void>(physical), source + copied, chunk);
        copied += chunk;
    }
    return true;
}

bool copy_to_user(PageFrameContainer& frames,
                  const Thread* thread,
                  uint64_t user_pointer,
                  const void* source,
                  size_t length)
{
    if((nullptr == thread) || (nullptr == source))
    {
        return false;
    }
    if(!user_address::is_user_address_range(user_pointer, length))
    {
        return false;
    }

    VirtualMemory vm(frames, thread->address_space_cr3);
    const uint8_t* src = (const uint8_t*)source;
    size_t copied = 0;
    while(copied < length)
    {
        uint64_t physical = 0;
        uint64_t flags = 0;
        if(!vm.translate(user_pointer + copied, physical, flags))
        {
            return false;
        }
        if(!user_address::has_required_user_flags(flags, true))
        {
            return false;
        }
        const size_t page_offset = (user_pointer + copied) & (kPageSize - 1);
        const size_t chunk = ((length - copied) < (kPageSize - page_offset))
                                 ? (length - copied)
                                 : (kPageSize - page_offset);
        memcpy(kernel_physical_pointer<void>(physical), src + copied, chunk);
        copied += chunk;
    }
    return true;
}

bool copy_from_user(PageFrameContainer& frames,
                    const Thread* thread,
                    uint64_t user_pointer,
                    void* destination,
                    size_t length)
{
    if((nullptr == thread) || (nullptr == destination))
    {
        return false;
    }
    if(!user_address::is_user_address_range(user_pointer, length))
    {
        return false;
    }

    VirtualMemory vm(frames, thread->address_space_cr3);
    uint8_t* dest = (uint8_t*)destination;
    size_t copied = 0;
    while(copied < length)
    {
        uint64_t physical = 0;
        uint64_t flags = 0;
        if(!vm.translate(user_pointer + copied, physical, flags))
        {
            return false;
        }
        if(!user_address::has_required_user_flags(flags, false))
        {
            return false;
        }
        const size_t page_offset = (user_pointer + copied) & (kPageSize - 1);
        const size_t chunk = ((length - copied) < (kPageSize - page_offset))
                                 ? (length - copied)
                                 : (kPageSize - page_offset);
        memcpy(dest + copied, kernel_physical_pointer<const void>(physical), chunk);
        copied += chunk;
    }
    return true;
}

bool copy_user_string(PageFrameContainer& frames,
                      const Thread* thread,
                      uint64_t user_pointer,
                      char* destination,
                      size_t destination_size)
{
    if((nullptr == thread) || (nullptr == destination) || (0 == destination_size) ||
       (0 == user_pointer))
    {
        return false;
    }

    for(size_t index = 0; index < destination_size; ++index)
    {
        char ch = 0;
        if(!copy_from_user(frames, thread, user_pointer + index, &ch, sizeof(ch)))
        {
            return false;
        }
        destination[index] = ch;
        if(0 == ch)
        {
            return true;
        }
    }

    destination[destination_size - 1] = 0;
    return false;
}
