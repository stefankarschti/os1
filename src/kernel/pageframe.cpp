#include "pageframe.h"
#include "memory.h"
#include "debug.h"

/**
 * @brief PageFrameContainer::Initialize
 * @param info Pointer to system information
 * @return true for success
 */
bool PageFrameContainer::Initialize(SystemInformation *info)
{
	bool result = false;
	memory_size_ = 0;
	memory_end_address_ = 0;
	for(size_t i = 0; i < info->num_memory_blocks; i++)
	{
		MemoryBlock &b = info->memory_blocks[i];
		if(1 == b.type)
		{
			// add to usable memory size
			memory_size_ += b.length;
		}
		if(memory_end_address_ < b.start + b.length)
		{
			memory_end_address_ = b.start + b.length;
		}
	}

	// set up page frame bitmap
	debug.Write("bitmap_ is ");
	debug.WriteIntLn((uint64_t)bitmap_, 16);
	bitmap_ = (uint64_t*)(0x1C000);
	if(memory_size_ > 0 && memory_end_address_ > 0)
	{
		page_count_ = memory_end_address_ >> 12;
		bitmap_size_ = ((page_count_ + 7) / 8 + 7) / 8; // number of qwords, round up

		// TODO: correct check fit into memory
		if(bitmap_ + bitmap_size_ <= (uint64_t*)0x9FC00)
		{
			// set all pages to '0' = not available
			// this takes care of any memory gaps
			memsetq(bitmap_, 0, bitmap_size_ * 8);
			result = true;
		}
	}

	// paint pages in bitmap according to availability
	if(result)
	{
		for(size_t i = 0; i < info->num_memory_blocks; i++)
		{
			MemoryBlock &b = info->memory_blocks[i];
			if(1 == b.type)
			{
				// check page start aligned
				if(b.start & 0xFFF)
				{
					result = false;
					break;
				}

				uint64_t start_page = b.start >> 12;
				uint64_t end_page = (b.start + b.length) >> 12; // exclusive end. if page end is not aligned, the partial last page is lost memory

				// set start_page:end_page bits to 1
				uint64_t ifirst = start_page / 64;
				uint64_t ilast = end_page / 64;

				// deal with whole qwords
				if(ifirst + 1 < ilast)
				{
					memsetq(bitmap_ + ifirst + 1, 0xFFFFFFFFFFFFFFFF, 8 * (ilast - ifirst - 1));
				}

				// deal with first qword
				bitmap_[ifirst] |= 0xFFFFFFFFFFFFFFFF << (start_page % 64);

				// deal with last qword
				bitmap_[ilast] |= 0xFFFFFFFFFFFFFFFF >> (64 - (end_page % 64));
			}
		}
	}

	// count free memory pages
	if(result)
	{
		uint64_t num_pages = 0;
		uint64_t ipage = 0;
		auto bitmap_end = bitmap_ + bitmap_size_;
		auto p = bitmap_;
		while(p < bitmap_end)
		{
			if(*p)
			{
				uint64_t val = *p;
				// at least 1 page free
				int bit_count = 64;
				while(bit_count-- && ipage < page_count_)
				{
					if(val & 1)
						num_pages++;
					val >>= 1;
					ipage++;
				}
			}
			p++;
		}
		free_page_count_ = num_pages;
	}

	// mark bitmap_ pages as occupied
	if(result)
	{
		uint64_t p = (uint64_t)bitmap_;
		uint64_t bitmap_end = (uint64_t)(bitmap_ + bitmap_size_);
		do
		{
			// set page occupied
			SetBusy(p);

			// next
			p += 0x1000;
		}
		while(p < bitmap_end);
	}

	// mark kernel pages occupied
	if(result)
	{
		uint64_t p = 0x0;
		uint64_t end = 0x200000; // 2M
		while(p < end)
		{
			// set page occupied
			SetBusy(p);

			// next
			p += 0x1000;
		}
	}

	return result;
}

bool PageFrameContainer::Allocate(uint64_t &address)
{
	// TODO: check for successful initialization
	// TODO: mark last first free qword to speed up next search
	auto bitmap_end = bitmap_ + bitmap_size_;
	auto p = bitmap_;
	uint64_t ipage = 0;
	while(p < bitmap_end)
	{
		if(*p)
		{
			uint64_t val = *p;
			// at least 1 page free
			int bit_count = 64;
			while(bit_count-- && ipage < page_count_)
			{
				if(val & 1)
				{
					// this page is free
					address = ipage << 12;

					// mark as occupied
					uint64_t mask = 1ull << (ipage % 64);
					mask = ~mask;
					*p &= mask;

					// return this one
					return true;
				}
				val >>= 1;
				ipage++;
			}
		}
		p++;
	}
	return false;
}

bool PageFrameContainer::Free(uint64_t address)
{
	// TODO: check address align
	// TODO: check against unavailable memory pages
	bool result = false;
	uint64_t ipage = address >> 12;
	if(ipage < page_count_)
	{
		auto p = bitmap_ + ipage / 64;

		// check if occupied
		uint64_t mask = 1ull << (ipage % 64);
		if(0 == (*p & mask))
		{
			*p |= mask; // mark as free
			result = true;
		}
	}
	return result;
}

void PageFrameContainer::SetFree(uint64_t address)
{
	uint64_t ipage = address >> 12;
	bitmap_[ipage / 64] |= (1ull << (ipage % 64));
}

void PageFrameContainer::SetBusy(uint64_t address)
{
	uint64_t ipage = address >> 12;
	bitmap_[ipage / 64] &= ~(1ull << (ipage % 64));
}
