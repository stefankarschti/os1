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
	VirtualMemory(PageFrameContainer &frames);
	void SetBaseAddress(uint64_t address);
	bool Allocate(size_t num_pages);
	size_t Size();
};

#endif // VIRTUALMEMORY_H
