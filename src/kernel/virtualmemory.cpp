#include "virtualmemory.h"
#include "memory.h"
#include "debug.h"
#include <stdlib.h>

VirtualMemory::VirtualMemory(PageFrameContainer &frames)
	: frames_(frames), initialized_(false), root_(~0ull)
{
}

bool VirtualMemory::Allocate(uint64_t start_address, uint64_t num_pages)
{
	if(!num_pages)
	{
		debug("allocate VM 0x")(start_address, 16)(" with 0 pages")();
		return false;
	}
	if(start_address & 0xFFF)
	{
		debug("allocate VM 0x")(start_address, 16)();
		debug("address not aligned")();
		return false;
	}

	// Allocate PML4
	if(!initialized_)
	{
		if(!frames_.Allocate(root_)) return false;
		memsetq((void*)root_, 0, 4096);
		initialized_ = true;
		debug("root alloc 0x")(root_, 16)();
	}

	// create page tables
	uint64_t end_address = start_address + (num_pages << 12);
	debug("allocate VM 0x")(start_address, 16)(" to 0x")(end_address, 16)();
	uint64_t *pag4 = (uint64_t*)root_;

	// level 4
	for(auto vp = start_address; vp < end_address; vp += (1ull << 39))
	{
		uint64_t idx4 = (vp >> 39) & 0x1FF;
		uint64_t mem = (vp >> 39) << 39;
		uint64_t len = 1ull << 39;
		debug(idx4)(" 0x")(mem, 16)(" 0x")(mem + len, 16);
		if(!AllocEntry(pag4[idx4], true)) return false;
		debug();
	}

	// level 3
	for(auto vp = start_address; vp < end_address; vp += (1ull << 30))
	{
		uint64_t idx4 = (vp >> 39) & 0x1FF;
		uint64_t idx3 = (vp >> 30) & 0x1FF;
		uint64_t mem = (vp >> 30) << 30;
		uint64_t len = 1ull << 30;
		debug(idx4)("/")(idx3)(" 0x")(mem, 16)(" 0x")(mem + len, 16);
		uint64_t *pag3 = (uint64_t*)(pag4[idx4] & ~(0xFFFull));
		if(!AllocEntry(pag3[idx3], true)) return false;
		debug();
	}

	// level 2
	for(auto vp = start_address; vp < end_address; vp += (1ull << 21))
	{
		uint64_t idx4 = (vp >> 39) & 0x1FF;
		uint64_t idx3 = (vp >> 30) & 0x1FF;
		uint64_t idx2 = (vp >> 21) & 0x1FF;
		uint64_t mem = (vp >> 21) << 21;
		uint64_t len = 1ull << 21;
		debug(idx4)("/")(idx3)("/")(idx2)(" 0x")(mem, 16)(" 0x")(mem + len, 16);
		uint64_t *pag3 = (uint64_t*)(pag4[idx4] & ~(0xFFFull));
		uint64_t *pag2 = (uint64_t*)(pag3[idx3] & ~(0xFFFull));
		if(!AllocEntry(pag2[idx2], true)) return false;
		debug();
	}

	// level 1
	for(auto vp = start_address; vp < end_address; vp += (1ull << 12))
	{
		uint64_t idx4 = (vp >> 39) & 0x1FF;
		uint64_t idx3 = (vp >> 30) & 0x1FF;
		uint64_t idx2 = (vp >> 21) & 0x1FF;
		uint64_t idx1 = (vp >> 12) & 0x1FF;
		uint64_t mem = (vp >> 12) << 12;
		uint64_t len = 1ull << 12;
		debug(idx4)("/")(idx3)("/")(idx2)("/")(idx1)(" 0x")(mem, 16)(" 0x")(mem + len, 16);
		uint64_t *pag3 = (uint64_t*)(pag4[idx4] & ~(0xFFFull));
		uint64_t *pag2 = (uint64_t*)(pag3[idx3] & ~(0xFFFull));
		uint64_t *pag1 = (uint64_t*)(pag2[idx2] & ~(0xFFFull));
		if(!AllocEntry(pag1[idx1], false)) return false;
		debug();
	}

	return true;
}

bool VirtualMemory::Free(uint64_t start_address, uint64_t num_pages)
{
	if(!num_pages)
	{
		debug("free VM 0x")(start_address, 16)(" with 0 pages")();
		return false;
	}
	if(start_address & 0xFFF)
	{
		debug("free VM 0x")(start_address, 16)();
		debug("address not aligned")();
		return false;
	}

	if(initialized_)
	{
		// free range
		uint64_t end_address = start_address + (num_pages << 12);
		debug("free VM 0x")(start_address, 16)(" to 0x")(end_address, 16)();
		uint64_t *pag4 = (uint64_t*)root_;

		// level 1
		for(auto vp = start_address; vp < end_address; vp += (1ull << 12))
		{
			uint64_t idx4 = (vp >> 39) & 0x1FF;
			uint64_t idx3 = (vp >> 30) & 0x1FF;
			uint64_t idx2 = (vp >> 21) & 0x1FF;
			uint64_t idx1 = (vp >> 12) & 0x1FF;
			uint64_t mem = (vp >> 12) << 12;
			uint64_t len = 1ull << 12;
			debug(idx4)("/")(idx3)("/")(idx2)("/")(idx1)(" 0x")(mem, 16)(" 0x")(mem + len, 16);
			uint64_t *pag3 = (uint64_t*)(pag4[idx4] & ~(0xFFFull));
			uint64_t *pag2 = (uint64_t*)(pag3[idx3] & ~(0xFFFull));
			uint64_t *pag1 = (uint64_t*)(pag2[idx2] & ~(0xFFFull));
			if(!FreeEntry(pag1[idx1], false)) return false;
			debug();
		}

		// level 2
		for(auto vp = start_address; vp < end_address; vp += (1ull << 21))
		{
			uint64_t idx4 = (vp >> 39) & 0x1FF;
			uint64_t idx3 = (vp >> 30) & 0x1FF;
			uint64_t idx2 = (vp >> 21) & 0x1FF;
			uint64_t mem = (vp >> 21) << 21;
			uint64_t len = 1ull << 21;
			debug(idx4)("/")(idx3)("/")(idx2)(" 0x")(mem, 16)(" 0x")(mem + len, 16);
			uint64_t *pag3 = (uint64_t*)(pag4[idx4] & ~(0xFFFull));
			uint64_t *pag2 = (uint64_t*)(pag3[idx3] & ~(0xFFFull));
			if(!FreeEntry(pag2[idx2], true)) return false;
			debug();
		}

		// level 3
		for(auto vp = start_address; vp < end_address; vp += (1ull << 30))
		{
			uint64_t idx4 = (vp >> 39) & 0x1FF;
			uint64_t idx3 = (vp >> 30) & 0x1FF;
			uint64_t mem = (vp >> 30) << 30;
			uint64_t len = 1ull << 30;
			debug(idx4)("/")(idx3)(" 0x")(mem, 16)(" 0x")(mem + len, 16);
			uint64_t *pag3 = (uint64_t*)(pag4[idx4] & ~(0xFFFull));
			if(!FreeEntry(pag3[idx3], true)) return false;
			debug();
		}

		// level 4
		for(auto vp = start_address; vp < end_address; vp += (1ull << 39))
		{
			uint64_t idx4 = (vp >> 39) & 0x1FF;
			uint64_t mem = (vp >> 39) << 39;
			uint64_t len = 1ull << 39;
			debug(idx4)(" 0x")(mem, 16)(" 0x")(mem + len, 16);
			if(!FreeEntry(pag4[idx4], true)) return false;
			debug();
		}

		debug("root");
		FreeEntry(root_, true);
		if(0 == root_)
		{
			initialized_ = false;
			root_ = ~0ull;
		}
		else
		{
			debug("=0x")(root_, 16);
		}
		debug();
	}

	return true;
}

bool VirtualMemory::Free()
{
	if(initialized_)
	{
		// some allocation has happened (at least partial)
		// pag4_ is valid
		ForceFreeTable((uint64_t*)root_, 4);
		initialized_ = false;
		frames_.Free(root_);
		root_ = ~0ull;
	}
	return true;
}

void VirtualMemory::ForceFreeTable(uint64_t *pag, int level)
{
	for(uint64_t idx = 0; idx < 512; ++idx)
	{
		if(pag[idx])
		{
			uint64_t address = pag[idx] & ~(0xFFFull);
			if(level > 1)
				ForceFreeTable((uint64_t*)address, level - 1);
			frames_.Free(address);
		}
	}
}

bool VirtualMemory::AllocEntry(uint64_t &entry, bool clear)
{
	if(0 == (entry))
	{
		uint64_t new_pag;
		if(!frames_.Allocate(new_pag))	return false;
		debug(" alloc 0x")(new_pag, 16);
		if(clear)
		{
			debug(" clear");
			memsetq((void*)new_pag, 0, 4096);
		}
		entry = (new_pag & ~(0xFFFull)) | PAGE_PRESENT | PAGE_WRITE;
	}
	return true;
}

bool VirtualMemory::FreeEntry(uint64_t &entry, bool is_table)
{
	if(entry)
	{
		uint64_t address = entry & ~(0xFFFull);
		bool ok_to_delete = true;
		if(is_table)
		{
			// check for any remaining child
			uint64_t *pag = (uint64_t*)address;
			for(int i = 0; i < 512; ++i)
			{
				if(pag[i])
				{
					ok_to_delete = false;
					break;
				}
			}
		}

		if(ok_to_delete)
		{
			debug(" free 0x")(address, 16);
			if(!frames_.Free(address)) return false;
			entry = 0;
		}
	}
	return true;
}
