#include "pageframe.h"
#include "memory.h"
#include "debug.h"

PageFrameContainer::PageFrameContainer()
	:initialized_(false)
{
}

bool PageFrameContainer::Initialize(SystemInformation &info, uint64_t bitmap_address, uint64_t bitmap_limit)
{
	if(initialized_) return false;
	bool result = false;
	memory_size_ = 0;
	memory_end_address_ = 0;
	for(size_t i = 0; i < info.num_memory_blocks; i++)
	{
		MemoryBlock &b = info.memory_blocks[i];
		debug.WriteInt(b.start, 16, 16); debug.Write(' ');
		debug.WriteInt(b.length, 16, 16); debug.Write(' ');
		debug.WriteIntLn(b.type);
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
	bitmap_ = (uint64_t*)(bitmap_address);
	bitmap_limit_ = bitmap_limit;
	debug("bitmap_ 0x")((uint64_t)bitmap_, 16)(" limit ")(bitmap_limit_)();

	if(memory_size_ > 0 && memory_end_address_ > 0)
	{
		page_count_ = memory_end_address_ >> 12;
		bitmap_size_ = ((page_count_ + 7) / 8 + 7) / 8; // number of qwords, round up

		// TODO: correct check fit into memory
		if(bitmap_size_ <= bitmap_limit_)
		{
			// set all pages to '0' = not available
			// this takes care of any memory gaps
			memsetq(bitmap_, 0, bitmap_size_ * 8);
			result = true;
		}
		else
		{
			debug("bitmap limit exceeded: ")(bitmap_size_)();
			result = false;
		}
	}

	// paint pages in bitmap according to availability
	if(result)
	{
		for(size_t i = 0; i < info.num_memory_blocks; i++)
		{
			MemoryBlock &b = info.memory_blocks[i];
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
		uint64_t bitmap_end = (uint64_t)(bitmap_ + bitmap_limit_);
		for(uint64_t vp = (uint64_t)bitmap_; vp < bitmap_end; vp += 0x1000)
		{
			SetBusy(vp >> 12);
		}
	}

	// mark kernel pages occupied
	if(result)
	{
		// mark kernel low data pages as busy
		for(uint64_t vp = 0; vp < 0x20000; vp += 0x1000)
		{
			SetBusy(vp >> 12);
		}

		// mark kernel code & stack as busy
		for(uint64_t vp = 0x100000; vp < 0x160000; vp += 0x1000)
		{
			SetBusy(vp >> 12);
		}
	}

	// debug page frames
	if(false)
	{
		auto bitmap_end = bitmap_ + bitmap_size_;
		for(auto p = bitmap_; p < bitmap_end; p++)
		{
			debug.WriteInt((p - bitmap_) * 64);
			debug.Write(' ');
			debug.WriteIntLn(*p, 2, 64);
		}
	}

	if(result)
		initialized_ = true;

	return result;
}

bool PageFrameContainer::Allocate(uint64_t &address)
{
	// check for successful initialization
	if(!initialized_)
		return false;
	// TODO: mark last first free qword to speed up next search
	auto bitmap_end = bitmap_ + bitmap_size_;
	auto p = bitmap_;
	uint64_t ipage = 0;
	while(p < bitmap_end)
	{
		if(*p)
		{
			// at least 1 page free
			uint64_t val = *p;
			ipage = (p - bitmap_) * 64;
			while(val)
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

bool PageFrameContainer::Allocate(uint64_t &address, unsigned count)
{
	// check for successful initialization
	if(!initialized_)
		return false;
	if(count == 0)
		return false;

	debug("allocate ")(count)(" pages")();

	int64_t last_free = -1;
	for(int64_t i = 0; i < page_count_; i++)
	{
		if(IsFree(i))
		{
			if(last_free < 0) last_free = i;
			if(i - last_free + 1 == count)
			{
				debug("return ")(last_free)(" to ")(i)();
				address = last_free << 12;
				for(auto j = last_free; j <= i; ++j)
					SetBusy(j);
				return true;
			}
		}
		else
			last_free = -1;
	}
	return false;
}

bool PageFrameContainer::Free(uint64_t address)
{
	if(!initialized_) return false;
	// check address align
	if(address & 0xFFF)
	{
		debug("free frame 0x")(address, 16)();
		debug("address not aligned")();
		return false;
	}

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

void PageFrameContainer::SetFree(uint64_t ipage)
{
	bitmap_[ipage / 64] |= (1ull << (ipage % 64));
}

void PageFrameContainer::SetBusy(uint64_t ipage)
{
	bitmap_[ipage / 64] &= ~(1ull << (ipage % 64));
}

bool PageFrameContainer::IsFree(uint64_t ipage)
{
	return (bitmap_[ipage / 64] & (1ull << (ipage % 64))) != 0;
}
