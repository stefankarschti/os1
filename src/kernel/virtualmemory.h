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
	bool Initialize(uint64_t address, uint64_t num_pages); // single mode

	// TODO: add VM range management
	size_t Size();

private:
	PageFrameContainer &frames_;
};

#endif // VIRTUALMEMORY_H
