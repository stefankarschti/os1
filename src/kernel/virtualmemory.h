#ifndef VIRTUALMEMORY_H
#define VIRTUALMEMORY_H

#include <stdint.h>
#include <stddef.h>
#include "pageframe.h"

/**
 * @brief The VirtualMemory class
 * Manages the VM page structure
 * Set CR3
 * Shrink / Expand / Share
 * uses PageFrameContainer
 */
class VirtualMemory
{
public:
	static const uint64_t PAGE_PRESENT	= 1 << 0;
	static const uint64_t PAGE_WRITE	= 1 << 1;

	VirtualMemory(PageFrameContainer &frames);

	/**
	 * @brief Allocate a new range to the virtual memory, or identity map range
	 * @param start_address : must be aligned to page boundary
	 * @param num_pages : must be greater than 0
	 * @param identity_map : identity map VM rather than alloc frames
	 * @return true if successful
	 */
	bool Allocate(uint64_t start_address, uint64_t num_pages, bool identity_map);

	/**
	 * @brief Free
	 * @param start_address
	 * @param num_pages
	 * @return
	 */
	bool Free(uint64_t start_address, uint64_t num_pages);

	/**
	 * @brief Free
	 * @return
	 */
	bool Free();

	/**
	 * @brief Root
	 * @return PML4 pointer
	 */
	uint64_t Root();

	/**
	 * @brief Activate the page tables
	 * @return true if successful
	 */
	bool Activate();

private:
	PageFrameContainer &frames_;
	bool initialized_;
	uint64_t root_;

	void ForceFreeTable(uint64_t* pag, int level);
	bool AllocEntry(uint64_t &entry, bool clear);
	bool FreeEntry(uint64_t &entry, bool is_table);
};

#endif // VIRTUALMEMORY_H
