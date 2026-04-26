#include "mm/user_copy.h"

#include "memory.h"
#include "memory_layout.h"

namespace
{
[[nodiscard]] bool IsCanonicalVirtualAddress(uint64_t address)
{
	const uint64_t upper_bits = address >> 48;
	return (0 == upper_bits) || (0xFFFFull == upper_bits);
}


[[nodiscard]] bool IsUserAddressRange(uint64_t address, size_t length)
{
	if(0 == length)
	{
		return true;
	}
	if((0 == address) || !IsCanonicalVirtualAddress(address))
	{
		return false;
	}

	const uint64_t last_byte_offset = static_cast<uint64_t>(length - 1);
	if(address > (~0ull - last_byte_offset))
	{
		return false;
	}

	const uint64_t end_address = address + last_byte_offset;
	if(!IsCanonicalVirtualAddress(end_address))
	{
		return false;
	}

	return (address >= kUserSpaceBase) && (end_address < kUserStackTop);
}

[[nodiscard]] bool HasRequiredUserFlags(uint64_t flags, bool require_write)
{
	const PageFlags page_flags = static_cast<PageFlags>(flags);
	if((page_flags & PageFlags::User) != PageFlags::User)
	{
		return false;
	}
	if(require_write && ((page_flags & PageFlags::Write) != PageFlags::Write))
	{
		return false;
	}
	return true;
}
}

bool CopyIntoAddressSpace(VirtualMemory &vm, uint64_t virtual_address, const uint8_t *source, uint64_t length)
{
	uint64_t copied = 0;
	while(copied < length)
	{
		uint64_t physical = 0;
		uint64_t flags = 0;
		if(!vm.Translate(virtual_address + copied, physical, flags))
		{
			return false;
		}

		const uint64_t page_offset = (virtual_address + copied) & (kPageSize - 1);
		const uint64_t chunk = ((length - copied) < (kPageSize - page_offset))
			? (length - copied)
			: (kPageSize - page_offset);
		memcpy((void*)physical, source + copied, chunk);
		copied += chunk;
	}
	return true;
}

bool CopyToUser(PageFrameContainer &frames, const Thread *thread, uint64_t user_pointer, const void *source, size_t length)
{
	if((nullptr == thread) || (nullptr == source))
	{
		return false;
	}
	if(!IsUserAddressRange(user_pointer, length))
	{
		return false;
	}

	VirtualMemory vm(frames, thread->address_space_cr3);
	const uint8_t *src = (const uint8_t*)source;
	size_t copied = 0;
	while(copied < length)
	{
		uint64_t physical = 0;
		uint64_t flags = 0;
		if(!vm.Translate(user_pointer + copied, physical, flags))
		{
			return false;
		}
		if(!HasRequiredUserFlags(flags, true))
		{
			return false;
		}
		const size_t page_offset = (user_pointer + copied) & (kPageSize - 1);
		const size_t chunk = ((length - copied) < (kPageSize - page_offset))
			? (length - copied)
			: (kPageSize - page_offset);
		memcpy((void*)physical, src + copied, chunk);
		copied += chunk;
	}
	return true;
}

bool CopyFromUser(PageFrameContainer &frames, const Thread *thread, uint64_t user_pointer, void *destination, size_t length)
{
	if((nullptr == thread) || (nullptr == destination))
	{
		return false;
	}
	if(!IsUserAddressRange(user_pointer, length))
	{
		return false;
	}

	VirtualMemory vm(frames, thread->address_space_cr3);
	uint8_t *dest = (uint8_t*)destination;
	size_t copied = 0;
	while(copied < length)
	{
		uint64_t physical = 0;
		uint64_t flags = 0;
		if(!vm.Translate(user_pointer + copied, physical, flags))
		{
			return false;
		}
		if(!HasRequiredUserFlags(flags, false))
		{
			return false;
		}
		const size_t page_offset = (user_pointer + copied) & (kPageSize - 1);
		const size_t chunk = ((length - copied) < (kPageSize - page_offset))
			? (length - copied)
			: (kPageSize - page_offset);
		memcpy(dest + copied, (const void*)physical, chunk);
		copied += chunk;
	}
	return true;
}

bool CopyUserString(PageFrameContainer &frames,
		const Thread *thread,
		uint64_t user_pointer,
		char *destination,
		size_t destination_size)
{
	if((nullptr == thread) || (nullptr == destination) || (0 == destination_size) || (0 == user_pointer))
	{
		return false;
	}

	for(size_t index = 0; index < destination_size; ++index)
	{
		char ch = 0;
		if(!CopyFromUser(frames, thread, user_pointer + index, &ch, sizeof(ch)))
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