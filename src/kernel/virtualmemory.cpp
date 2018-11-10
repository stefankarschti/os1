#include "virtualmemory.h"
#include "memory.h"

VirtualMemory::VirtualMemory(PageFrameContainer &frames)
	: frames_(frames), initialized_(false), pag4_(~0ull)
{
}

bool VirtualMemory::Initialize(uint64_t address, uint64_t num_pages)
{
	if(initialized_)
		return false;
	bool result = false;

	// Allocate PML4
	result = frames_.Allocate(pag4_);
	if(!result) return result;
	memsetq((void*)pag4_, 0, 4096);
	initialized_ = true;

	// create page tables
	uint64_t virtual_pointer = address;
	uint64_t end = address + (num_pages << 12);
	while(virtual_pointer < end)
	{
		uint64_t *pag = (uint64_t*)pag4_;
		uint64_t bits = 39;

		while(bits >= 12)
		{
			uint64_t idx = (virtual_pointer >> bits) & 0x1FF;
			uint64_t pag_next = pag[idx] & ~(0xFFFull);
			if(0 == (pag_next & PAGE_PRESENT))
			{
				// Allocate next level
				result = frames_.Allocate(pag_next); // TODO: check failure
				if(!result) return result;
				memsetq((void*)pag_next, 0, 4096);
				pag[idx] = (pag_next & ~(0xFFFull)) | PAGE_PRESENT | PAGE_WRITE;
			}

			bits -= 9;
			pag = (uint64_t*)pag_next;
		}

		// next page
		virtual_pointer += 0x1000;
	}

	return result;
}

void VirtualMemory::Free()
{
	if(initialized_)
	{
		// some allocation has happened (at least partial)
		// pag4_ is valid
		InternalFree((uint64_t*)pag4_, 4);
		initialized_ = false;
		frames_.Free(pag4_);
		pag4_ = ~0ull;
	}
}

void VirtualMemory::InternalFree(uint64_t *pag, int level)
{
	for(uint64_t idx = 0; idx < 512; ++idx)
	{
		if(pag[idx] & PAGE_PRESENT)
		{
			uint64_t address = pag[idx] & ~(0xFFFull);
			if(level > 1)
				InternalFree((uint64_t*)address, level - 1);
			frames_.Free(address);
		}
	}
}
