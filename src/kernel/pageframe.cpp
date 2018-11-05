#include "pageframe.h"
#include "memory.h"

/**
 * @brief PageFrameContainer::Initialize
 * @param info Pointer to system information
 * @return true for success
 */
bool PageFrameContainer::Initialize(SystemInformation *info, Terminal& debug)
{
	bool result = false;
	memory_size_ = 0;
	memory_end_address_ = 0;
	debug.WriteInt(info->num_memory_blocks); debug.WriteLn(" memory blocks");
	for(size_t i = 0; i < info->num_memory_blocks; i++)
	{
		MemoryBlock &b = info->memory_blocks[i];
		debug.Write("(1) "); debug.WriteInt(b.start, 16, 16); debug.Write(' '); debug.WriteInt(b.length, 16, 16); debug.Write(' '); debug.WriteIntLn(b.type);
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
	bitmap_ = (uint64_t*)(0x1C000);
	debug.Write("bitmap "); debug.WriteIntLn((uint64_t)bitmap_, 16, 16);
	if(memory_size_ > 0 && memory_end_address_ > 0)
	{
		page_count_ = memory_end_address_ >> 12;
		bitmap_size_ = ((page_count_ + 7) / 8 + 7) / 8; // number of qwords, round up

		debug.Write("bitmap size "); debug.WriteIntLn(bitmap_size_);
		// TODO: check fit into memory
//		if(bitmap_ + bitmap_size_ <= (uint64_t*)0xA000)
		{
			// set all pages to '0' = not available
			// this takes care of any memory gaps
			debug.WriteLn("set all to zero");
			memsetq(bitmap_, 0, bitmap_size_ * 8);

			result = true;
		}
	}

	// paint pages in bitmap according to availability
	if(result)
	{
		debug.WriteInt(info->num_memory_blocks); debug.WriteLn(" memory blocks");
		for(size_t i = 0; i < info->num_memory_blocks; i++)
		{
			MemoryBlock &b = info->memory_blocks[i];
			debug.Write("(2) "); debug.WriteInt(b.start, 16, 16); debug.Write(' '); debug.WriteInt(b.length, 16, 16); debug.Write(' '); debug.WriteIntLn(b.type);
			if(1 == b.type)
			{
				// check page start aligned
				if(b.start & 0xFFF)
				{
					debug.WriteLn("break1");
					result = false;
					break;
				}

				uint64_t start_page = b.start >> 12;
				uint64_t end_page = (b.start + b.length) >> 12; // exclusive end. if page end is not aligned, the partial last page is lost memory

				// set start_page:end_page bits to 1
				uint64_t ifirst = start_page / 64;
				uint64_t ilast = end_page / 64;

				debug.WriteInt(ifirst); debug.Write(' '); debug.WriteIntLn(ilast);

				// deal with whole qwords
				if(ifirst + 1 < ilast)
				{
					debug.Write("set "); debug.WriteInt(ifirst + 1); debug.Write(' '); debug.WriteIntLn(ilast);
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
			uint64_t val = *p;
			debug.WriteInt(val, 2, 64);
			debug.Write(' ');

			if(*p)
			{
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

	return result;
}
