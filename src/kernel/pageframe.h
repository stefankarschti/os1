#ifndef PAGEFRAME_H
#define PAGEFRAME_H

#include <stdint.h>
#include <stddef.h>
#include "sysinfo.h"
#include "terminal.h"

/**
 * @brief The PageFrameContainer class
 * Manages the physical memory page frames
 */
class PageFrameContainer
{
public:
	PageFrameContainer();
	/**
	 * @brief Initialize
	 * @param info : system information
	 * @param bitmap_address : base address
	 * @param bitmap_limit : limit number of u64 in bitmap
	 * @return
	 */
	bool Initialize(SystemInformation &info, uint64_t bitmap_address, uint64_t bitmap_limit);
	uint64_t MemorySize() { return memory_size_; }
	uint64_t MemoryEnd() { return memory_end_address_; }
	uint64_t PageCount() { return page_count_; }
	uint64_t FreePageCount() { return free_page_count_; }

	/**
	 * @brief Allocate one page
	 * @param [out] address : page address
	 * @return bool true if success
	 */
	bool Allocate(uint64_t &address);

	/**
	 * @brief Allocate block of pages
	 * @param [out] address : block address
	 * @param page_count : number of pages
	 * @return
	 */
	bool Allocate(uint64_t &address, unsigned page_count);

	/**
	 * @brief Free one page
	 * @param address Address of the page
	 * @return bool True if success
	 */
	bool Free(uint64_t address);

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
	uint64_t free_page_count_ = 0;

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
	uint64_t *bitmap_;
	/**
	 * @brief bitmap_size_ number of qwords
	 */
	uint64_t bitmap_size_ = 0;
	uint64_t bitmap_limit_ = 0;
	bool initialized_ = false;

	// direct manipulators - restricted access
	void SetFree(uint64_t ipage);
	void SetBusy(uint64_t ipage);
	bool IsFree(uint64_t ipage);
};

#endif // PAGEFRAME_H
