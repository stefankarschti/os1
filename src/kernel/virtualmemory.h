#ifndef VIRTUALMEMORY_H
#define VIRTUALMEMORY_H

#include <stdint.h>
#include <stddef.h>

#include "pageframe.h"

enum class PageFlags : uint64_t
{
	None = 0,
	Present = 1ull << 0,
	Write = 1ull << 1,
	User = 1ull << 2,
	NoExecute = 1ull << 63,
};

inline constexpr PageFlags operator|(PageFlags left, PageFlags right)
{
	return static_cast<PageFlags>(
		static_cast<uint64_t>(left) | static_cast<uint64_t>(right));
}

inline constexpr PageFlags operator&(PageFlags left, PageFlags right)
{
	return static_cast<PageFlags>(
		static_cast<uint64_t>(left) & static_cast<uint64_t>(right));
}

inline constexpr PageFlags &operator|=(PageFlags &left, PageFlags right)
{
	left = left | right;
	return left;
}

class VirtualMemory
{
public:
	static const uint64_t PAGE_PRESENT = 1ull << 0;
	static const uint64_t PAGE_WRITE = 1ull << 1;

	VirtualMemory(PageFrameContainer &frames, uint64_t existing_root = ~0ull);

	void Attach(uint64_t root);
	bool Allocate(uint64_t start_address, uint64_t num_pages, bool identity_map);
	bool MapPhysical(uint64_t virtual_address, uint64_t physical_address, uint64_t num_pages, PageFlags flags);
	bool AllocateAndMap(uint64_t virtual_address, uint64_t num_pages, PageFlags flags, uint64_t *first_physical = nullptr);
	bool Protect(uint64_t virtual_address, uint64_t num_pages, PageFlags flags);
	bool Translate(uint64_t virtual_address, uint64_t &physical_address, uint64_t &flags) const;
	bool CloneKernelPml4Entry(uint64_t slot, uint64_t source_root);
	bool DestroyUserSlot(uint64_t slot);
	bool Free(uint64_t start_address, uint64_t num_pages);
	bool Free();
	uint64_t Root();
	bool Activate();

private:
	PageFrameContainer &frames_;
	bool initialized_;
	uint64_t root_;

	bool EnsureRoot(void);
	bool EnsureTableEntry(uint64_t &entry, bool user_visible);
	bool WalkToLeaf(uint64_t virtual_address, bool create, bool user_visible, uint64_t **leaf_entry);
	void DestroyTable(uint64_t &entry, int level, bool free_leaf_pages);
	static uint64_t FlagsToEntry(PageFlags flags);
	static uint64_t TableEntryFlags(bool user_visible);
};

#endif // VIRTUALMEMORY_H
