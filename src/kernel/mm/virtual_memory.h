// Long-mode page-table manager for kernel identity mappings and per-process
// user address spaces.
#ifndef OS1_KERNEL_MM_VIRTUAL_MEMORY_H
#define OS1_KERNEL_MM_VIRTUAL_MEMORY_H

#include <stdint.h>
#include <stddef.h>

#include "mm/page_frame.h"

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

	// Construct a manager around a new or existing PML4 root.
	VirtualMemory(PageFrameContainer &frames, uint64_t existing_root = ~0ull);

	// Attach this wrapper to an externally owned page-table root.
	void Attach(uint64_t root);
	// Allocate or identity-map a contiguous virtual page range.
	bool Allocate(uint64_t start_address, uint64_t num_pages, bool identity_map);
	// Map existing physical pages into a virtual range with explicit flags.
	bool MapPhysical(uint64_t virtual_address, uint64_t physical_address, uint64_t num_pages, PageFlags flags);
	// Allocate physical pages, zero them, and map them into a virtual range.
	bool AllocateAndMap(uint64_t virtual_address, uint64_t num_pages, PageFlags flags, uint64_t *first_physical = nullptr);
	// Replace permissions on already-mapped pages.
	bool Protect(uint64_t virtual_address, uint64_t num_pages, PageFlags flags);
	// Translate one virtual address and report its physical address plus leaf flags.
	bool Translate(uint64_t virtual_address, uint64_t &physical_address, uint64_t &flags) const;
	// Clone a kernel PML4 slot from another root into this address space.
	bool CloneKernelPml4Entry(uint64_t slot, uint64_t source_root);
	// Destroy an entire PML4 slot and optionally all user leaf pages beneath it.
	bool DestroyUserSlot(uint64_t slot);
	// Free mapped physical pages in a virtual range.
	bool Free(uint64_t start_address, uint64_t num_pages);
	// Destroy this entire page table hierarchy.
	bool Free();
	// Return the active root physical address for this wrapper.
	uint64_t Root();
	// Load this root into CR3.
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

#endif // OS1_KERNEL_MM_VIRTUAL_MEMORY_H
