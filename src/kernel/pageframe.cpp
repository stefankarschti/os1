#include "pageframe.h"
#include "memory.h"

PageFrameContainer::PageFrameContainer()
{

}

/**
 * @brief PageFrameContainer::Initialize
 * @param info Pointer to system information
 * @return true for success
 */
bool PageFrameContainer::Initialize(system_info *info)
{
	bool result = false;
	memory_size_ = 0;
	memory_end_address_ = 0;
	for(size_t i = 0; i < info->num_memory_blocks; i++)
	{
		memory_block &b = info->memory_blocks[i];
		if(1 == b.type)
		{
			memory_size_ += b.length;
			if(memory_end_address_ < b.start + b.length)
			{
				memory_end_address_ = b.start + b.length;
			}
		}
	}

	// set up page frame bitmap
	if(memory_size_ > 0 && memory_end_address_ > 0)
	{
		page_count_ = memory_end_address_ >> 12;
		bitmap_size_ = ((page_count_ + 7) / 8 + 7) / 8; // number of qwords, round up

		// check fit into memory
		if(bitmap_ + bitmap_size_ <= (uint64_t*)0xA000)
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
			memory_block &b = info->memory_blocks[i];
			if(1 == b.type)
			{
				// check page start aligned
				if(b.start & 0xFFF)
				{
					result = false;
					break;
				}
				// check page end aligned
				if((b.start + b.length) & 0xFFF)
				{
					result = false;
					break;
				}

				uint64_t start_page = b.start >> 12;
				uint64_t end_page = (b.start + b.length) >> 12; // exclusive end!

				// set start_page:end_page bits to 1
				uint64_t ifirst = start_page / 64;
				uint64_t ilast = end_page / 64;

				// deal with whole qwords
				if(ifirst + 1 <= ilast)
				{
					memsetq(bitmap_ + ifirst + 1, 0xFFFFFFFFFFFFFFFF, 8 * (ilast - ifirst - 1));
				}

				// deal with first qword
				bitmap_[ifirst] &= 0xFFFFFFFFFFFFFFFF << (start_page % 64);

				// deal with last qword
				bitmap_[ilast] &= 0xFFFFFFFFFFFFFFFF >> (64 - (end_page % 64));
			}
		}
	}

	return result;
}
