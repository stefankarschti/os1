// Bitmap page-frame allocator implementation. Boot memory-map regions seed the
// bitmap; later subsystems allocate physical pages through this single owner.
#include "mm/page_frame.hpp"

#include "debug/debug.hpp"
#include "handoff/memory_layout.h"
#include "util/memory.h"

PageFrameContainer::PageFrameContainer() : initialized_(false) {}

bool PageFrameContainer::initialize(std::span<const BootMemoryRegion> memory_regions,
                                    uint64_t bitmap_address,
                                    uint64_t bitmap_limit)
{
    if(initialized_)
        return false;
    bool result = false;
    memory_size_ = 0;
    memory_end_address_ = 0;
    debug("num_memory_blocks = ")(memory_regions.size())();
    for(size_t i = 0; i < memory_regions.size(); i++)
    {
        const BootMemoryRegion& b = memory_regions[i];
        debug.write_int(b.physical_start, 16, 16);
        debug.write(' ');
        debug.write_int(b.length, 16, 16);
        debug.write(' ');
        debug.write_int_line(static_cast<uint64_t>(b.type));
        if(boot_memory_region_is_usable(b))
        {
            // add to usable memory size
            memory_size_ += b.length;
            if(memory_end_address_ < b.physical_start + b.length)
            {
                memory_end_address_ = b.physical_start + b.length;
            }
        }
    }

    // set up page frame bitmap
    bitmap_physical_address_ = bitmap_address;
    bitmap_ = kernel_physical_pointer<uint64_t>(bitmap_physical_address_);
    bitmap_limit_ = bitmap_limit;
    debug("bitmap_ 0x")((uint64_t)bitmap_, 16)(" limit ")(bitmap_limit_)();

    if(memory_size_ > 0 && memory_end_address_ > 0)
    {
        page_count_ = memory_end_address_ >> 12;
        bitmap_size_ = ((page_count_ + 7) / 8 + 7) / 8;  // number of qwords, round up

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
    else
    {
        debug("!(memory_size_ > 0 && memory_end_address_ > 0)")();
        result = false;
    }

    // paint pages in bitmap according to availability
    if(result)
    {
        for(size_t i = 0; i < memory_regions.size(); i++)
        {
            const BootMemoryRegion& b = memory_regions[i];
            if(boot_memory_region_is_usable(b))
            {
                // check page start aligned
                if(b.physical_start & (kPageSize - 1))
                {
                    result = false;
                    break;
                }

                const uint64_t start_page = b.physical_start >> 12;
                const uint64_t end_page = (b.physical_start + b.length) >>
                                          12;  // exclusive end. if page end is not aligned, the
                                               // partial last page is lost memory
                if(end_page <= start_page)
                {
                    continue;
                }

                // Set the range of usable pages free in the bitmap. This boot-time
                // path keeps the logic explicit so the loader's memory map is easy to
                // audit while the rest of the VM system is still mostly identity-mapped.
                const uint64_t ifirst = start_page / 64;
                const uint64_t ilast = (end_page - 1) / 64;

                if(ifirst == ilast)
                {
                    const uint64_t first_bit = start_page % 64;
                    const uint64_t last_bit = (end_page - 1) % 64;
                    const uint64_t bit_count = last_bit - first_bit + 1;
                    const uint64_t mask = (64 == bit_count)
                                              ? 0xFFFFFFFFFFFFFFFFull
                                              : (((1ull << bit_count) - 1) << first_bit);
                    bitmap_[ifirst] |= mask;
                    continue;
                }

                bitmap_[ifirst] |= 0xFFFFFFFFFFFFFFFFull << (start_page % 64);

                if(ifirst + 1 < ilast)
                {
                    memsetq(bitmap_ + ifirst + 1, 0xFFFFFFFFFFFFFFFFull, 8 * (ilast - ifirst - 1));
                }

                const uint64_t last_bits = end_page % 64;
                bitmap_[ilast] |= (0 == last_bits) ? 0xFFFFFFFFFFFFFFFFull
                                                   : (0xFFFFFFFFFFFFFFFFull >> (64 - last_bits));
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
        for(uint64_t vp = (uint64_t)bitmap_; vp < bitmap_end; vp += kPageSize)
        {
            set_busy(vp >> 12);
        }
    }

    // mark kernel pages occupied
    if(result)
    {
        // mark kernel low data pages as busy
        for(uint64_t vp = 0; vp < kEarlyReservedPhysicalEnd; vp += kPageSize)
        {
            set_busy(vp >> 12);
        }

        // mark kernel code & stack as busy
        for(uint64_t vp = kKernelReservedPhysicalStart; vp < kKernelReservedPhysicalEnd;
            vp += kPageSize)
        {
            set_busy(vp >> 12);
        }
    }

    // debug page frames
    if(false)
    {
        auto bitmap_end = bitmap_ + bitmap_size_;
        for(auto p = bitmap_; p < bitmap_end; p++)
        {
            debug.write_int((p - bitmap_) * 64);
            debug.write(' ');
            debug.write_int_line(*p, 2, 64);
        }
    }

    if(result)
        initialized_ = true;

    return result;
}

void PageFrameContainer::enable_direct_map_access()
{
    if(0 != bitmap_physical_address_)
    {
        bitmap_ = kernel_physical_pointer<uint64_t>(bitmap_physical_address_);
    }
}

bool PageFrameContainer::allocate(uint64_t& address)
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
                    if(free_page_count_ > 0)
                    {
                        --free_page_count_;
                    }

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

bool PageFrameContainer::allocate(uint64_t& address, unsigned count)
{
    // check for successful initialization
    if(!initialized_)
        return false;
    if(count == 0)
        return false;

    //	debug("allocate ")(count)(" pages")();

    uint64_t run_start = page_count_;
    for(uint64_t i = 0; i < page_count_; i++)
    {
        if(is_free(i))
        {
            if(run_start == page_count_)
                run_start = i;
            if((i - run_start + 1) == count)
            {
                //				debug("return ")(run_start)(" to ")(i)();
                address = run_start << 12;
                for(uint64_t j = run_start; j <= i; ++j)
                {
                    set_busy(j);
                }
                if(free_page_count_ >= count)
                {
                    free_page_count_ -= count;
                }
                return true;
            }
        }
        else
            run_start = page_count_;
    }
    return false;
}

bool PageFrameContainer::free(uint64_t address)
{
    if(!initialized_)
        return false;
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
            *p |= mask;  // mark as free
            ++free_page_count_;
            result = true;
        }
    }
    return result;
}

bool PageFrameContainer::reserve_range(uint64_t address, uint64_t length)
{
    if(!initialized_ || (0 == length))
    {
        return false;
    }

    const uint64_t start_page = address >> 12;
    const uint64_t end_page = (address + length + kPageSize - 1) >> 12;
    if(end_page <= start_page)
    {
        return true;
    }

    for(uint64_t ipage = start_page; ipage < end_page; ++ipage)
    {
        if(ipage >= page_count_)
        {
            return false;
        }
        if(is_free(ipage))
        {
            set_busy(ipage);
            if(free_page_count_ > 0)
            {
                --free_page_count_;
            }
        }
    }

    return true;
}

void PageFrameContainer::set_free(uint64_t ipage)
{
    bitmap_[ipage / 64] |= (1ull << (ipage % 64));
}

void PageFrameContainer::set_busy(uint64_t ipage)
{
    bitmap_[ipage / 64] &= ~(1ull << (ipage % 64));
}

bool PageFrameContainer::is_free(uint64_t ipage)
{
    return (bitmap_[ipage / 64] & (1ull << (ipage % 64))) != 0;
}
