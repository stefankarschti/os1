#ifndef PAGEFRAME_H
#define PAGEFRAME_H

#include "stdint.h"
#include "stddef.h"
#include "sysinfo.h"

/**
 * @brief The PageFrameContainer class
 * Manages the physical memory page frames
 */
class PageFrameContainer
{
public:
	PageFrameContainer();
	bool Initialize(system_info *info);
	uint64_t MemorySize() { return memory_size_; }
	uint64_t MemoryEnd() { return memory_end_address_; }
	uint64_t PageCount() { return page_count_; }

private:
	/**
	 * @brief Usable memory size in bytes
	 */
	uint64_t memory_size_ = 0;

	/**
	 * @brief Maximum valid memory address + 1
	 */
	uint64_t memory_end_address_ = 0;

	/**
	 * @brief page_count_
	 */
	uint64_t page_count_ = 0;

	/**
	 * @brief bitmap_ contains 1 bit for every page:
	 *		0 = page NOT free / NOT available
	 *		1 = page free & available
	 *	array padded to 8 bytes
	 *	so when you need to get the first available page, do:
	 *		cld
	 *		mov rsi, bitmap_
	 *		mov rdi, bitmap_end_
	 *	continue:
	 *		cmp rsi, rdi
	 *		jae done
	 *		lodsq
	 *		or rax, rax
	 *		jz continue
	 *		; FOUND
	 *		; one of the 64 pages IS FREE!
	 *	done:
	 *		; NOT FOUND
	 */
	uint64_t *bitmap_ = (uint64_t*)(0x0);
	/**
	 * @brief bitmap_size_ number of qwords
	 */
	uint64_t bitmap_size_ = 0;
};

#endif // PAGEFRAME_H