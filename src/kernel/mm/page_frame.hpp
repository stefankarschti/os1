// Bitmap physical page-frame allocator. The allocator is initialized from the
// normalized BootInfo memory map and remains the source of physical pages for VM,
// process tables, kernel stacks, and early drivers.
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <span>

#include "handoff/boot_info.hpp"
#include "console/terminal.hpp"

/**
 * @brief The PageFrameContainer class
 * Manages the physical memory page frames
 */
class PageFrameContainer
{
public:
	PageFrameContainer();
	// initialize the allocator from bootloader-provided memory regions and a fixed
	// bitmap storage range.
	bool initialize(std::span<const BootMemoryRegion> memory_regions, uint64_t bitmap_address, uint64_t bitmap_limit);
	// Return total usable memory bytes reported by the boot memory map.
	uint64_t memory_size() { return memory_size_; }
	// Return one byte past the highest physical address tracked by the bitmap.
	uint64_t memory_end() { return memory_end_address_; }
	// Return the number of physical pages represented by the bitmap.
	uint64_t page_count() { return page_count_; }
	// Return the number of pages currently marked free.
	uint64_t free_page_count() { return free_page_count_; }

	// allocate one free physical page and return its physical address.
	bool allocate(uint64_t &address);

	// allocate a contiguous block of physical pages and return its base address.
	bool allocate(uint64_t &address, unsigned page_count);

	/**
	 * @brief Reserve a physical range so later allocators do not hand it out.
	 * Boot modules stay pinned this way until the kernel grows a richer boot-
	 * time object-lifetime model.
	 */
	bool reserve_range(uint64_t address, uint64_t length);

	// free one aligned physical page back to the bitmap.
	bool free(uint64_t address);

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

	// Mark a bitmap bit free without changing counters.
	void set_free(uint64_t ipage);
	// Mark a bitmap bit busy without changing counters.
	void set_busy(uint64_t ipage);
	// Return true when a bitmap bit currently represents a free page.
	bool is_free(uint64_t ipage);
};

