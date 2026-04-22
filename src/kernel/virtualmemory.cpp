#include "virtualmemory.h"

#include "debug.h"
#include "memory.h"
#include "memory_layout.h"

namespace
{
constexpr uint64_t kPageMask = ~(kPageSize - 1);
// Long-mode page-table entries carry the NX bit in the high flag range, so the
// physical-address mask must exclude it explicitly instead of just clearing the
// low page-offset bits.
constexpr uint64_t kEntryAddressMask = 0x000FFFFFFFFFF000ull;

[[nodiscard]] inline uint64_t PageIndex(uint64_t virtual_address, unsigned shift)
{
	return (virtual_address >> shift) & 0x1FFull;
}
}

VirtualMemory::VirtualMemory(PageFrameContainer &frames, uint64_t existing_root)
	: frames_(frames),
	  initialized_(existing_root != ~0ull),
	  root_(existing_root)
{
}

void VirtualMemory::Attach(uint64_t root)
{
	root_ = root;
	initialized_ = (root != ~0ull);
}

bool VirtualMemory::EnsureRoot(void)
{
	if(initialized_)
	{
		return true;
	}

	if(!frames_.Allocate(root_))
	{
		return false;
	}

	memsetq((void*)root_, 0, kPageSize);
	initialized_ = true;
	debug("root alloc 0x")(root_, 16)();
	return true;
}

uint64_t VirtualMemory::FlagsToEntry(PageFlags flags)
{
	return static_cast<uint64_t>(flags);
}

uint64_t VirtualMemory::TableEntryFlags(bool user_visible)
{
	uint64_t entry = static_cast<uint64_t>(PageFlags::Present | PageFlags::Write);
	if(user_visible)
	{
		entry |= static_cast<uint64_t>(PageFlags::User);
	}
	return entry;
}

bool VirtualMemory::EnsureTableEntry(uint64_t &entry, bool user_visible)
{
	if(0 == entry)
	{
		uint64_t new_page = 0;
		if(!frames_.Allocate(new_page))
		{
			return false;
		}
		memsetq((void*)new_page, 0, kPageSize);
		entry = (new_page & kEntryAddressMask) | TableEntryFlags(user_visible);
		return true;
	}

	if(user_visible)
	{
		entry |= static_cast<uint64_t>(PageFlags::User);
	}

	return true;
}

bool VirtualMemory::WalkToLeaf(uint64_t virtual_address, bool create, bool user_visible, uint64_t **leaf_entry)
{
	if((nullptr == leaf_entry) || !EnsureRoot())
	{
		return false;
	}

	uint64_t *pml4 = (uint64_t*)root_;
	uint64_t *pml3 = nullptr;
	uint64_t *pml2 = nullptr;
	uint64_t *pml1 = nullptr;

	uint64_t &pml4e = pml4[PageIndex(virtual_address, 39)];
	if(create)
	{
		if(!EnsureTableEntry(pml4e, user_visible))
		{
			return false;
		}
	}
	else if(0 == pml4e)
	{
		return false;
	}
	pml3 = (uint64_t*)(pml4e & kEntryAddressMask);

	uint64_t &pml3e = pml3[PageIndex(virtual_address, 30)];
	if(create)
	{
		if(!EnsureTableEntry(pml3e, user_visible))
		{
			return false;
		}
	}
	else if(0 == pml3e)
	{
		return false;
	}
	pml2 = (uint64_t*)(pml3e & kEntryAddressMask);

	uint64_t &pml2e = pml2[PageIndex(virtual_address, 21)];
	if(create)
	{
		if(!EnsureTableEntry(pml2e, user_visible))
		{
			return false;
		}
	}
	else if(0 == pml2e)
	{
		return false;
	}
	pml1 = (uint64_t*)(pml2e & kEntryAddressMask);

	*leaf_entry = &pml1[PageIndex(virtual_address, 12)];
	return true;
}

bool VirtualMemory::Allocate(uint64_t start_address, uint64_t num_pages, bool identity_map)
{
	if(identity_map)
	{
		return MapPhysical(start_address, start_address, num_pages,
			PageFlags::Present | PageFlags::Write);
	}

	return AllocateAndMap(start_address, num_pages,
		PageFlags::Present | PageFlags::Write);
}

bool VirtualMemory::MapPhysical(uint64_t virtual_address, uint64_t physical_address, uint64_t num_pages, PageFlags flags)
{
	if((0 == num_pages) || (virtual_address & (kPageSize - 1)) || (physical_address & (kPageSize - 1)))
	{
		return false;
	}

	const bool user_visible = (PageFlags::User == (flags & PageFlags::User));
	for(uint64_t i = 0; i < num_pages; ++i)
	{
		uint64_t *leaf_entry = nullptr;
		if(!WalkToLeaf(virtual_address + i * kPageSize, true, user_visible, &leaf_entry))
		{
			return false;
		}
		*leaf_entry = ((physical_address + i * kPageSize) & kEntryAddressMask)
			| FlagsToEntry(flags | PageFlags::Present);
	}

	return true;
}

bool VirtualMemory::AllocateAndMap(uint64_t virtual_address, uint64_t num_pages, PageFlags flags, uint64_t *first_physical)
{
	if((0 == num_pages) || (virtual_address & (kPageSize - 1)))
	{
		return false;
	}

	uint64_t first_page = 0;
	for(uint64_t i = 0; i < num_pages; ++i)
	{
		uint64_t physical_page = 0;
		if(!frames_.Allocate(physical_page))
		{
			return false;
		}
		if(0 == i)
		{
			first_page = physical_page;
		}
		memsetq((void*)physical_page, 0, kPageSize);
		if(!MapPhysical(virtual_address + i * kPageSize, physical_page, 1, flags | PageFlags::Present))
		{
			return false;
		}
	}

	if(first_physical)
	{
		*first_physical = first_page;
	}

	return true;
}

bool VirtualMemory::Protect(uint64_t virtual_address, uint64_t num_pages, PageFlags flags)
{
	if((0 == num_pages) || (virtual_address & (kPageSize - 1)))
	{
		return false;
	}

	for(uint64_t i = 0; i < num_pages; ++i)
	{
		uint64_t *leaf_entry = nullptr;
		if(!WalkToLeaf(virtual_address + i * kPageSize, false, false, &leaf_entry) || (nullptr == leaf_entry) || (0 == *leaf_entry))
		{
			return false;
		}
		const uint64_t physical_page = *leaf_entry & kEntryAddressMask;
		*leaf_entry = physical_page | FlagsToEntry(flags | PageFlags::Present);
	}

	return true;
}

bool VirtualMemory::Translate(uint64_t virtual_address, uint64_t &physical_address, uint64_t &flags) const
{
	if(!initialized_)
	{
		return false;
	}

	const uint64_t *pml4 = (const uint64_t*)root_;
	const uint64_t pml4e = pml4[PageIndex(virtual_address, 39)];
	if(0 == pml4e)
	{
		return false;
	}
	const uint64_t *pml3 = (const uint64_t*)(pml4e & kEntryAddressMask);
	const uint64_t pml3e = pml3[PageIndex(virtual_address, 30)];
	if(0 == pml3e)
	{
		return false;
	}
	const uint64_t *pml2 = (const uint64_t*)(pml3e & kEntryAddressMask);
	const uint64_t pml2e = pml2[PageIndex(virtual_address, 21)];
	if(0 == pml2e)
	{
		return false;
	}
	const uint64_t *pml1 = (const uint64_t*)(pml2e & kEntryAddressMask);
	const uint64_t pml1e = pml1[PageIndex(virtual_address, 12)];
	if(0 == pml1e)
	{
		return false;
	}

	flags = pml1e & ~kEntryAddressMask;
	physical_address = (pml1e & kEntryAddressMask) | (virtual_address & ~kPageMask);
	return true;
}

bool VirtualMemory::CloneKernelPml4Entry(uint64_t slot, uint64_t source_root)
{
	if(!EnsureRoot())
	{
		return false;
	}
	if(slot >= 512)
	{
		return false;
	}
	uint64_t *target = (uint64_t*)root_;
	const uint64_t *source = (const uint64_t*)source_root;
	target[slot] = source[slot];
	return true;
}

void VirtualMemory::DestroyTable(uint64_t &entry, int level, bool free_leaf_pages)
{
	if(0 == entry)
	{
		return;
	}

	uint64_t *table = (uint64_t*)(entry & kEntryAddressMask);
	if(level > 1)
	{
		for(int i = 0; i < 512; ++i)
		{
			if(table[i])
			{
				DestroyTable(table[i], level - 1, free_leaf_pages);
			}
		}
	}
	else if(free_leaf_pages)
	{
		for(int i = 0; i < 512; ++i)
		{
			if(table[i])
			{
				frames_.Free(table[i] & kEntryAddressMask);
				table[i] = 0;
			}
		}
	}

	frames_.Free(entry & kEntryAddressMask);
	entry = 0;
}

bool VirtualMemory::DestroyUserSlot(uint64_t slot)
{
	if(!initialized_ || (slot >= 512))
	{
		return false;
	}

	uint64_t *pml4 = (uint64_t*)root_;
	DestroyTable(pml4[slot], 4, true);
	return true;
}

bool VirtualMemory::Free(uint64_t start_address, uint64_t num_pages)
{
	if((0 == num_pages) || (start_address & (kPageSize - 1)))
	{
		return false;
	}

	for(uint64_t i = 0; i < num_pages; ++i)
	{
		uint64_t *leaf_entry = nullptr;
		if(!WalkToLeaf(start_address + i * kPageSize, false, false, &leaf_entry) || (nullptr == leaf_entry) || (0 == *leaf_entry))
		{
			return false;
		}
		frames_.Free(*leaf_entry & kEntryAddressMask);
		*leaf_entry = 0;
	}

	return true;
}

bool VirtualMemory::Free()
{
	if(initialized_)
	{
		uint64_t *pml4 = (uint64_t*)root_;
		for(int i = 0; i < 512; ++i)
		{
			if(pml4[i])
			{
				DestroyTable(pml4[i], 4, true);
			}
		}
		frames_.Free(root_);
		root_ = ~0ull;
		initialized_ = false;
	}
	return true;
}

uint64_t VirtualMemory::Root()
{
	if(!initialized_)
	{
		debug("VirtualMemory::Root() warning: VirtualMemory not initialized")();
	}
	return root_;
}

bool VirtualMemory::Activate()
{
	if(!initialized_)
	{
		return false;
	}
	asm volatile("mov %0, %%cr3" : : "r"(root_));
	return true;
}
