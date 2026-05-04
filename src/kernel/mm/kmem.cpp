// Direct-map-backed slab allocator for kernel small objects.
#include "mm/kmem.hpp"

#include "debug/debug.hpp"
#include "handoff/memory_layout.h"
#include "sync/smp.hpp"
#include "util/align.hpp"
#include "util/memory.h"

#if !defined(OS1_HOST_TEST)
#include "core/panic.hpp"
#endif

namespace
{
constexpr uint32_t kSlabMagic = 0x4B4D454Du;
constexpr uint32_t kLargeAllocationMagic = 0x4B4D454Cu;
constexpr size_t kKeepEmptySlabsPerCache = 1;
constexpr size_t kMinKmallocAlignment = 16;
constexpr size_t kMaxSlabObjectSize = 1024;

enum class AllocationKind : uint8_t
{
    None = 0,
    Slab,
    Large,
};

enum class SlabListKind : uint8_t
{
    None = 0,
    Empty,
    Partial,
    Full,
};

struct KmemCache;

struct SlabPageHeader
{
    uint32_t magic = 0;
    uint16_t object_size = 0;
    uint16_t object_count = 0;
    uint16_t free_count = 0;
    uint16_t first_object_offset = 0;
    uint8_t list_kind = static_cast<uint8_t>(SlabListKind::None);
    uint8_t reserved = 0;
    uint64_t physical_page = 0;
    KmemCache* cache = nullptr;
    void* free_list = nullptr;
    SlabPageHeader* prev = nullptr;
    SlabPageHeader* next = nullptr;
};

static_assert(sizeof(SlabPageHeader) < kPageSize);

struct LargeAllocationHeader
{
    uint32_t magic = 0;
    uint32_t page_count = 0;
    uint16_t payload_offset = 0;
    uint16_t reserved = 0;
    uint64_t physical_base = 0;
    size_t requested_size = 0;
    size_t usable_size = 0;
};

static_assert(sizeof(LargeAllocationHeader) < kPageSize);

struct KmemCache
{
    constexpr KmemCache(const char* cache_name, uint16_t size, uint16_t cache_alignment)
        : name(cache_name), object_size(size), alignment(cache_alignment), lock(cache_name)
    {
    }

    const char* name;
    uint16_t object_size;
    uint16_t alignment;
    Spinlock lock;
    SlabPageHeader* empty = nullptr;
    SlabPageHeader* partial = nullptr;
    SlabPageHeader* full = nullptr;
    uint32_t slab_count = 0;
    uint32_t live_objects = 0;
    uint32_t peak_live_objects = 0;
    uint64_t alloc_count = 0;
    uint64_t free_count = 0;
    uint64_t failed_alloc_count = 0;
    uint64_t slab_growth_count = 0;
    uint64_t slab_return_count = 0;
};

class CacheLockGuard
{
public:
    explicit CacheLockGuard(Spinlock& lock) : irq_guard_(), lock_(lock)
    {
        lock_.lock();
    }

    CacheLockGuard(const CacheLockGuard&) = delete;
    CacheLockGuard& operator=(const CacheLockGuard&) = delete;

    ~CacheLockGuard()
    {
        lock_.unlock();
    }

private:
    IrqGuard irq_guard_;
    Spinlock& lock_;
};

constexpr uint16_t cache_alignment_for_size(uint16_t object_size)
{
    if(object_size < kMinKmallocAlignment)
    {
        return kMinKmallocAlignment;
    }
    if(object_size > 64u)
    {
        return 64u;
    }
    return object_size;
}

[[nodiscard]] bool add_overflow_size(size_t left, size_t right, size_t& result)
{
    return __builtin_add_overflow(left, right, &result);
}

[[nodiscard]] bool multiply_overflow_size(size_t left, size_t right, size_t& result)
{
    return __builtin_mul_overflow(left, right, &result);
}

[[nodiscard]] uint64_t allocation_page_base(const void* ptr)
{
    return align_down(reinterpret_cast<uint64_t>(ptr), static_cast<uint64_t>(kPageSize));
}

[[nodiscard]] AllocationKind allocation_kind_from_magic(uint32_t magic)
{
    if(kSlabMagic == magic)
    {
        return AllocationKind::Slab;
    }
    if(kLargeAllocationMagic == magic)
    {
        return AllocationKind::Large;
    }
    return AllocationKind::None;
}

[[nodiscard]] uint16_t large_payload_offset()
{
    return static_cast<uint16_t>(
        align_up(sizeof(LargeAllocationHeader), static_cast<uint64_t>(kMinKmallocAlignment)));
}

[[nodiscard]] void* large_payload(LargeAllocationHeader& header)
{
    return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(&header) + header.payload_offset);
}

[[nodiscard]] const void* large_payload(const LargeAllocationHeader& header)
{
    return reinterpret_cast<const void*>(reinterpret_cast<uintptr_t>(&header) +
                                         header.payload_offset);
}

[[nodiscard]] bool large_layout_for_size(size_t requested_size,
                                         uint32_t& page_count,
                                         uint16_t& payload_offset,
                                         size_t& usable_size)
{
    payload_offset = large_payload_offset();

    size_t total_bytes = 0;
    if(add_overflow_size(requested_size, payload_offset, total_bytes))
    {
        return false;
    }

    size_t rounded_total = 0;
    if(add_overflow_size(total_bytes, kPageSize - 1u, rounded_total))
    {
        return false;
    }

    const size_t page_count_size = rounded_total / kPageSize;
    if((0u == page_count_size) || (page_count_size > static_cast<size_t>(~0u)))
    {
        return false;
    }

    page_count = static_cast<uint32_t>(page_count_size);
    usable_size = page_count_size * kPageSize - payload_offset;
    return usable_size >= requested_size;
}

OS1_BSP_ONLY PageFrameContainer* g_kmem_frames = nullptr;
OS1_BSP_ONLY bool g_kmem_ready = false;
OS1_BSP_ONLY KmemGlobalStats g_kmem_global_stats{};
OS1_BSP_ONLY KmemCache g_builtin_caches[] = {
    KmemCache{"kmalloc-16", 16, cache_alignment_for_size(16)},
    KmemCache{"kmalloc-32", 32, cache_alignment_for_size(32)},
    KmemCache{"kmalloc-64", 64, cache_alignment_for_size(64)},
    KmemCache{"kmalloc-128", 128, cache_alignment_for_size(128)},
    KmemCache{"kmalloc-256", 256, cache_alignment_for_size(256)},
    KmemCache{"kmalloc-512", 512, cache_alignment_for_size(512)},
    KmemCache{"kmalloc-1024", 1024, cache_alignment_for_size(1024)},
};

[[nodiscard]] constexpr size_t builtin_cache_count()
{
    return sizeof(g_builtin_caches) / sizeof(g_builtin_caches[0]);
}

void record_cache_failure(KmemCache& cache)
{
    CacheLockGuard guard(cache.lock);
    ++cache.failed_alloc_count;
}

[[nodiscard]] bool has_flag(KmallocFlags value, KmallocFlags flag)
{
    return 0u != (static_cast<uint32_t>(value) & static_cast<uint32_t>(flag));
}

[[noreturn]] void kmem_halt()
{
#if defined(OS1_HOST_TEST)
    __builtin_trap();
#else
    halt_forever();
#endif
}

[[noreturn]] void kmem_corruption(const char* reason,
                                 const void* ptr,
                                 uint64_t header_address = 0,
                                 uint64_t physical_base = 0,
                                 const char* cache_name = nullptr)
{
    ++g_kmem_global_stats.corruption_count;
    debug("kmem: corruption: ")(reason)(" ptr=0x")(reinterpret_cast<uint64_t>(ptr), 16, 16)();
    if(0u != header_address)
    {
        debug("kmem: header=0x")(header_address, 16, 16)();
    }
    if(0u != physical_base)
    {
        debug("kmem: phys=0x")(physical_base, 16, 16)();
    }
    if(nullptr != cache_name)
    {
        debug("kmem: cache=")(cache_name)();
    }
    kmem_halt();
}

[[nodiscard]] void* kmem_fail(const char* reason, size_t size, KmallocFlags flags)
{
    ++g_kmem_global_stats.failed_alloc_count;
    if(has_flag(flags, KmallocFlags::PanicOnFail))
    {
        debug("kmem: allocation failed: ")(reason)(" size=")(static_cast<uint64_t>(size))();
        kmem_halt();
    }
    return nullptr;
}

[[nodiscard]] SlabListKind slab_kind(const SlabPageHeader& slab)
{
    return static_cast<SlabListKind>(slab.list_kind);
}

[[nodiscard]] SlabPageHeader*& slab_list_head(KmemCache& cache, SlabListKind kind)
{
    switch(kind)
    {
        case SlabListKind::Empty:
            return cache.empty;
        case SlabListKind::Partial:
            return cache.partial;
        case SlabListKind::Full:
            return cache.full;
        case SlabListKind::None:
            kmem_corruption("invalid slab list", &cache, reinterpret_cast<uint64_t>(&cache), 0, cache.name);
    }

    kmem_corruption("invalid slab list", &cache, reinterpret_cast<uint64_t>(&cache), 0, cache.name);
}

void detach_slab(KmemCache& cache, SlabPageHeader* slab)
{
    const SlabListKind kind = slab_kind(*slab);
    if(SlabListKind::None == kind)
    {
        return;
    }

    SlabPageHeader*& head = slab_list_head(cache, kind);
    if(nullptr != slab->prev)
    {
        slab->prev->next = slab->next;
    }
    else
    {
        head = slab->next;
    }

    if(nullptr != slab->next)
    {
        slab->next->prev = slab->prev;
    }

    slab->prev = nullptr;
    slab->next = nullptr;
    slab->list_kind = static_cast<uint8_t>(SlabListKind::None);
}

void attach_slab(KmemCache& cache, SlabPageHeader* slab, SlabListKind kind)
{
    SlabPageHeader*& head = slab_list_head(cache, kind);
    slab->prev = nullptr;
    slab->next = head;
    if(nullptr != head)
    {
        head->prev = slab;
    }
    head = slab;
    slab->list_kind = static_cast<uint8_t>(kind);
}

void move_slab(KmemCache& cache, SlabPageHeader* slab, SlabListKind kind)
{
    if(slab_kind(*slab) == kind)
    {
        return;
    }

    detach_slab(cache, slab);
    attach_slab(cache, slab, kind);
}

[[nodiscard]] KmemCache* select_cache(size_t size)
{
    for(size_t index = 0; index < builtin_cache_count(); ++index)
    {
        if(size <= g_builtin_caches[index].object_size)
        {
            return &g_builtin_caches[index];
        }
    }
    return nullptr;
}

[[nodiscard]] SlabPageHeader* select_slab(KmemCache& cache)
{
    if(nullptr != cache.partial)
    {
        return cache.partial;
    }
    return cache.empty;
}

[[nodiscard]] SlabPageHeader* initialize_slab_page(KmemCache& cache, uint64_t physical_page)
{
    auto* slab = kernel_physical_pointer<SlabPageHeader>(physical_page);
    memset(slab, 0, kPageSize);

    const uint64_t first_object_offset = align_up(sizeof(SlabPageHeader), cache.alignment);
    const uint64_t object_count = (kPageSize - first_object_offset) / cache.object_size;
    if((first_object_offset >= kPageSize) || (0u == object_count))
    {
        return nullptr;
    }

    slab->magic = kSlabMagic;
    slab->object_size = cache.object_size;
    slab->object_count = static_cast<uint16_t>(object_count);
    slab->free_count = static_cast<uint16_t>(object_count);
    slab->first_object_offset = static_cast<uint16_t>(first_object_offset);
    slab->physical_page = physical_page;
    slab->cache = &cache;

    auto* object_base = reinterpret_cast<uint8_t*>(slab) + first_object_offset;
    void* free_list = nullptr;
    for(uint16_t index = slab->object_count; index > 0; --index)
    {
        void* object = object_base + static_cast<size_t>(index - 1u) * cache.object_size;
        *reinterpret_cast<void**>(object) = free_list;
        free_list = object;
    }
    slab->free_list = free_list;
    return slab;
}

[[nodiscard]] bool grow_cache(KmemCache& cache)
{
    if(nullptr == g_kmem_frames)
    {
        return false;
    }

    uint64_t physical_page = 0;
    if(!g_kmem_frames->allocate(physical_page))
    {
        return false;
    }

    SlabPageHeader* slab = initialize_slab_page(cache, physical_page);
    if(nullptr == slab)
    {
        (void)g_kmem_frames->free(physical_page);
        return false;
    }

    CacheLockGuard guard(cache.lock);
    attach_slab(cache, slab, SlabListKind::Empty);
    ++cache.slab_count;
    ++cache.slab_growth_count;
    ++g_kmem_global_stats.slab_page_count;
    return true;
}

[[nodiscard]] bool pointer_in_slab(const SlabPageHeader& slab, const void* ptr)
{
    const uintptr_t slab_address = reinterpret_cast<uintptr_t>(&slab);
    const uintptr_t object_address = reinterpret_cast<uintptr_t>(ptr);
    const uintptr_t first_object = slab_address + slab.first_object_offset;
    const uintptr_t slab_end = slab_address + kPageSize;
    if((object_address < first_object) || (object_address >= slab_end))
    {
        return false;
    }

    return ((object_address - first_object) % slab.object_size) == 0u;
}

[[nodiscard]] bool pointer_on_free_list(const SlabPageHeader& slab, const void* ptr)
{
    void* current = slab.free_list;
    size_t visited = 0;
    while((nullptr != current) && (visited < slab.object_count))
    {
        if(current == ptr)
        {
            return true;
        }
        current = *reinterpret_cast<void**>(current);
        ++visited;
    }
    return false;
}

[[nodiscard]] bool pointer_is_large_allocation(const LargeAllocationHeader& header, const void* ptr)
{
    return large_payload(header) == ptr;
}

void snapshot_list(const SlabPageHeader* head,
                   uint32_t& slab_count,
                   uint32_t& total_object_count,
                   uint32_t& free_object_count)
{
    while(nullptr != head)
    {
        ++slab_count;
        total_object_count += head->object_count;
        free_object_count += head->free_count;
        head = head->next;
    }
}

[[nodiscard]] bool allocation_usable_size_internal(const void* ptr, size_t& size)
{
    if((nullptr == ptr) || !g_kmem_ready)
    {
        return false;
    }

    const uint64_t page_base = allocation_page_base(ptr);
    const auto kind = allocation_kind_from_magic(*reinterpret_cast<const uint32_t*>(page_base));
    switch(kind)
    {
        case AllocationKind::Slab:
        {
            const auto* slab = reinterpret_cast<const SlabPageHeader*>(page_base);
            if((nullptr == slab->cache) || !pointer_in_slab(*slab, ptr))
            {
                return false;
            }
            size = slab->object_size;
            return true;
        }
        case AllocationKind::Large:
        {
            const auto* large = reinterpret_cast<const LargeAllocationHeader*>(page_base);
            if(!pointer_is_large_allocation(*large, ptr))
            {
                return false;
            }
            size = large->usable_size;
            return true;
        }
        case AllocationKind::None:
            return false;
    }

    return false;
}

[[nodiscard]] void* allocate_large(size_t size, KmallocFlags flags)
{
    if(has_flag(flags, KmallocFlags::NoGrow) || has_flag(flags, KmallocFlags::Atomic))
    {
        ++g_kmem_global_stats.failed_large_alloc_count;
        return kmem_fail("large allocations require growth", size, flags);
    }

    uint32_t page_count = 0;
    uint16_t payload_offset = 0;
    size_t usable_size = 0;
    if(!large_layout_for_size(size, page_count, payload_offset, usable_size))
    {
        ++g_kmem_global_stats.failed_large_alloc_count;
        return kmem_fail("large allocation overflow", size, flags);
    }

    uint64_t physical_base = 0;
    if(!g_kmem_frames->allocate(physical_base, static_cast<unsigned>(page_count)))
    {
        ++g_kmem_global_stats.failed_large_alloc_count;
        return kmem_fail("out of physical pages", size, flags);
    }

    auto* header = kernel_physical_pointer<LargeAllocationHeader>(physical_base);
    if(has_flag(flags, KmallocFlags::Zero))
    {
        memset(header, 0, static_cast<uint64_t>(page_count) * kPageSize);
    }
    else
    {
        memset(header, 0, sizeof(*header));
    }

    header->magic = kLargeAllocationMagic;
    header->page_count = page_count;
    header->payload_offset = payload_offset;
    header->physical_base = physical_base;
    header->requested_size = size;
    header->usable_size = usable_size;

    ++g_kmem_global_stats.live_large_allocation_count;
    g_kmem_global_stats.live_large_allocation_bytes += size;
    ++g_kmem_global_stats.alloc_count;
    return large_payload(*header);
}

void release_large_allocation(LargeAllocationHeader* header)
{
    if((nullptr == header) || (0u == header->page_count))
    {
        ++g_kmem_global_stats.invalid_free_count;
        kmem_corruption("invalid large allocation header",
                        header,
                        reinterpret_cast<uint64_t>(header),
                        (nullptr != header) ? header->physical_base : 0,
                        "large");
    }

    if((0u == g_kmem_global_stats.live_large_allocation_count) ||
       (g_kmem_global_stats.live_large_allocation_bytes < header->requested_size))
    {
        kmem_corruption("large allocation stats underflow",
                        header,
                        reinterpret_cast<uint64_t>(header),
                        header->physical_base,
                        "large");
    }

    --g_kmem_global_stats.live_large_allocation_count;
    g_kmem_global_stats.live_large_allocation_bytes -= header->requested_size;
    ++g_kmem_global_stats.free_count;

    const uint64_t physical_base = header->physical_base;
    const uint32_t page_count = header->page_count;
    memset(header, 0, sizeof(*header));
    for(uint32_t page = 0; page < page_count; ++page)
    {
        if(!g_kmem_frames->free(physical_base + static_cast<uint64_t>(page) * kPageSize))
        {
            kmem_corruption("large allocation page free failed",
                            reinterpret_cast<void*>(large_payload_offset()),
                            reinterpret_cast<uint64_t>(header),
                            physical_base,
                            "large");
        }
    }
}

[[nodiscard]] void* allocate_from_slab_locked(KmemCache& cache, SlabPageHeader* slab)
{
    if((nullptr == slab) || (nullptr == slab->free_list) || (0u == slab->free_count))
    {
        kmem_corruption("empty slab allocation",
                        slab,
                        reinterpret_cast<uint64_t>(slab),
                        (nullptr != slab) ? slab->physical_page : 0,
                        cache.name);
    }

    void* object = slab->free_list;
    slab->free_list = *reinterpret_cast<void**>(object);
    --slab->free_count;

    ++cache.alloc_count;
    ++cache.live_objects;
    if(cache.peak_live_objects < cache.live_objects)
    {
        cache.peak_live_objects = cache.live_objects;
    }
    ++g_kmem_global_stats.alloc_count;

    const SlabListKind target = (0u == slab->free_count) ? SlabListKind::Full : SlabListKind::Partial;
    move_slab(cache, slab, target);
    return object;
}

[[nodiscard]] bool free_object_locked(KmemCache& cache, SlabPageHeader* slab, void* ptr)
{
    if(!pointer_in_slab(*slab, ptr))
    {
        ++g_kmem_global_stats.invalid_free_count;
        kmem_corruption("pointer outside slab",
                        ptr,
                        reinterpret_cast<uint64_t>(slab),
                        slab->physical_page,
                        cache.name);
    }

    if(pointer_on_free_list(*slab, ptr))
    {
        ++g_kmem_global_stats.invalid_free_count;
        kmem_corruption("double free",
                        ptr,
                        reinterpret_cast<uint64_t>(slab),
                        slab->physical_page,
                        cache.name);
    }

    if(0u == cache.live_objects)
    {
        kmem_corruption("live object underflow",
                        ptr,
                        reinterpret_cast<uint64_t>(slab),
                        slab->physical_page,
                        cache.name);
    }

    *reinterpret_cast<void**>(ptr) = slab->free_list;
    slab->free_list = ptr;
    ++slab->free_count;
    --cache.live_objects;
    ++cache.free_count;
    ++g_kmem_global_stats.free_count;
    if(slab->free_count > slab->object_count)
    {
        kmem_corruption("free count overflow",
                        ptr,
                        reinterpret_cast<uint64_t>(slab),
                        slab->physical_page,
                        cache.name);
    }

    const SlabListKind target = (slab->free_count == slab->object_count)
                                    ? SlabListKind::Empty
                                    : SlabListKind::Partial;
    move_slab(cache, slab, target);
    return (SlabListKind::Empty == target) && (nullptr != slab->next) &&
           (cache.slab_count > kKeepEmptySlabsPerCache);
}

void reset_cache(KmemCache& cache)
{
    cache.empty = nullptr;
    cache.partial = nullptr;
    cache.full = nullptr;
    cache.slab_count = 0;
    cache.live_objects = 0;
    cache.peak_live_objects = 0;
    cache.alloc_count = 0;
    cache.free_count = 0;
    cache.failed_alloc_count = 0;
    cache.slab_growth_count = 0;
    cache.slab_return_count = 0;
}
}  // namespace

void kmem_init(PageFrameContainer& frames)
{
    g_kmem_frames = &frames;
    g_kmem_global_stats = {};
    for(size_t index = 0; index < builtin_cache_count(); ++index)
    {
        reset_cache(g_builtin_caches[index]);
    }
    g_kmem_ready = true;
}

void* kmalloc(size_t size, KmallocFlags flags)
{
    if(!g_kmem_ready || (nullptr == g_kmem_frames))
    {
        return kmem_fail("allocator not initialized", size, flags);
    }
    if(0u == size)
    {
        return kmem_fail("zero-sized request", size, flags);
    }

    KmemCache* cache = select_cache(size);
    if(nullptr == cache)
    {
        return allocate_large(size, flags);
    }

    const bool allow_growth = !has_flag(flags, KmallocFlags::NoGrow) &&
                              !has_flag(flags, KmallocFlags::Atomic);
    bool attempted_growth = false;

    for(;;)
    {
        void* object = nullptr;
        {
            CacheLockGuard guard(cache->lock);
            SlabPageHeader* slab = select_slab(*cache);
            if(nullptr != slab)
            {
                object = allocate_from_slab_locked(*cache, slab);
            }
        }

        if(nullptr != object)
        {
            if(has_flag(flags, KmallocFlags::Zero))
            {
                memset(object, 0, cache->object_size);
            }
            return object;
        }

        if(!allow_growth || attempted_growth)
        {
            record_cache_failure(*cache);
            return kmem_fail("cache exhausted", size, flags);
        }

        if(!grow_cache(*cache))
        {
            record_cache_failure(*cache);
            return kmem_fail("out of physical pages", size, flags);
        }
        attempted_growth = true;
    }
}

void* kcalloc(size_t count, size_t size, KmallocFlags flags)
{
    size_t total_size = 0;
    if(multiply_overflow_size(count, size, total_size))
    {
        return kmem_fail("kcalloc overflow", 0, flags);
    }
    return kmalloc(total_size, flags | KmallocFlags::Zero);
}

void kfree(void* ptr)
{
    if(nullptr == ptr)
    {
        return;
    }
    if(!g_kmem_ready || (nullptr == g_kmem_frames))
    {
        ++g_kmem_global_stats.invalid_free_count;
        kmem_corruption("free before initialization", ptr);
    }

    const uint64_t page_base = allocation_page_base(ptr);
    const auto kind = allocation_kind_from_magic(*reinterpret_cast<const uint32_t*>(page_base));
    if(AllocationKind::Large == kind)
    {
        auto* large = reinterpret_cast<LargeAllocationHeader*>(page_base);
        if(!pointer_is_large_allocation(*large, ptr))
        {
            ++g_kmem_global_stats.invalid_free_count;
            kmem_corruption("invalid large allocation pointer",
                            ptr,
                            reinterpret_cast<uint64_t>(large),
                            large->physical_base,
                            "large");
        }

        release_large_allocation(large);
        return;
    }

    auto* slab = reinterpret_cast<SlabPageHeader*>(page_base);
    if((AllocationKind::Slab != kind) || (nullptr == slab->cache))
    {
        ++g_kmem_global_stats.invalid_free_count;
        kmem_corruption("invalid slab header", ptr, reinterpret_cast<uint64_t>(slab));
    }

    KmemCache& cache = *slab->cache;
    bool release_slab = false;
    uint64_t release_physical_page = 0;
    {
        CacheLockGuard guard(cache.lock);
        if((slab->cache != &cache) || (slab->object_size != cache.object_size))
        {
            ++g_kmem_global_stats.invalid_free_count;
            kmem_corruption("cache mismatch",
                            ptr,
                            reinterpret_cast<uint64_t>(slab),
                            slab->physical_page,
                            cache.name);
        }

        release_slab = free_object_locked(cache, slab, ptr);
        if(release_slab)
        {
            release_physical_page = slab->physical_page;
            detach_slab(cache, slab);
            if(0u == cache.slab_count)
            {
                kmem_corruption("slab count underflow",
                                ptr,
                                reinterpret_cast<uint64_t>(slab),
                                slab->physical_page,
                                cache.name);
            }
            --cache.slab_count;
            ++cache.slab_return_count;
            if(0u == g_kmem_global_stats.slab_page_count)
            {
                kmem_corruption("global slab page underflow",
                                ptr,
                                reinterpret_cast<uint64_t>(slab),
                                slab->physical_page,
                                cache.name);
            }
            --g_kmem_global_stats.slab_page_count;
            memset(slab, 0, sizeof(*slab));
        }
    }

    if(release_slab)
    {
        (void)g_kmem_frames->free(release_physical_page);
    }
}

bool kmem_allocation_usable_size(const void* ptr, size_t& size)
{
    size = 0;
    return allocation_usable_size_internal(ptr, size);
}

size_t kmem_cache_stats_count()
{
    return builtin_cache_count();
}

bool kmem_get_cache_stats(size_t index, KmemCacheStats& stats)
{
    if(index >= builtin_cache_count())
    {
        return false;
    }

    KmemCache& cache = g_builtin_caches[index];
    KmemCacheStats snapshot{};
    snapshot.name = cache.name;
    snapshot.object_size = cache.object_size;
    snapshot.alignment = cache.alignment;

    {
        CacheLockGuard guard(cache.lock);
        snapshot.slab_count = cache.slab_count;
        snapshot.live_object_count = cache.live_objects;
        snapshot.peak_live_object_count = cache.peak_live_objects;
        snapshot.alloc_count = cache.alloc_count;
        snapshot.free_count = cache.free_count;
        snapshot.failed_alloc_count = cache.failed_alloc_count;
        snapshot.slab_growth_count = cache.slab_growth_count;
        snapshot.slab_return_count = cache.slab_return_count;
        snapshot.total_object_count = 0;
        snapshot.free_object_count = 0;
        snapshot.empty_slab_count = 0;
        snapshot.partial_slab_count = 0;
        snapshot.full_slab_count = 0;
        snapshot_list(cache.empty,
                      snapshot.empty_slab_count,
                      snapshot.total_object_count,
                      snapshot.free_object_count);
        snapshot_list(cache.partial,
                      snapshot.partial_slab_count,
                      snapshot.total_object_count,
                      snapshot.free_object_count);
        snapshot_list(cache.full,
                      snapshot.full_slab_count,
                      snapshot.total_object_count,
                      snapshot.free_object_count);
    }

    stats = snapshot;
    return true;
}

void kmem_get_global_stats(KmemGlobalStats& stats)
{
    stats = g_kmem_global_stats;
    stats.cache_count = kmem_cache_stats_count();
}

void kmem_dump_stats()
{
    KmemGlobalStats global{};
    kmem_get_global_stats(global);
    debug("kmem: slab_pages=")(global.slab_page_count)(" alloc=")(global.alloc_count)(" free=")(
        global.free_count)(" failed=")(global.failed_alloc_count)(" large=")(
        global.live_large_allocation_count)(" large_bytes=")(global.live_large_allocation_bytes)();

    for(size_t index = 0; index < kmem_cache_stats_count(); ++index)
    {
        KmemCacheStats cache{};
        if(!kmem_get_cache_stats(index, cache))
        {
            continue;
        }

        debug("kmem: cache ")(cache.name)(" obj=")(static_cast<uint64_t>(cache.object_size))(" slabs=")(
            cache.slab_count)(" live=")(cache.live_object_count)(" free=")(cache.free_object_count)(
            " alloc=")(cache.alloc_count)(" fail=")(cache.failed_alloc_count)();
    }
}