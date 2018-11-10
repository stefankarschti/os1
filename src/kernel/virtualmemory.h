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
	bool Initialize(uint64_t address, uint64_t num_pages); // single mode
	void Free();

	// TODO: add VM range management
	uint64_t Size();

private:
	PageFrameContainer &frames_;
	bool initialized_;
	uint64_t pag4_;

	void InternalFree(uint64_t* pag, int level);
};

#endif // VIRTUALMEMORY_H
