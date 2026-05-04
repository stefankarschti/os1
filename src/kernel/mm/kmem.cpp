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
constexpr size_t kKeepEmptySlabsPerCache = 1;

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
    if(object_size < 16u)
    {
        return 16u;
    }
    if(object_size > 64u)
    {
        return 64u;
    }
    return object_size;
}

OS1_BSP_ONLY PageFrameContainer* g_kmem_frames = nullptr;
OS1_BSP_ONLY bool g_kmem_ready = false;
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

[[noreturn]] void kmem_corruption(const char* reason, const void* ptr, const SlabPageHeader* slab = nullptr)
{
    debug("kmem: corruption: ")(reason)(" ptr=0x")(reinterpret_cast<uint64_t>(ptr), 16, 16)();
    if(nullptr != slab)
    {
        debug("kmem: slab=0x")(reinterpret_cast<uint64_t>(slab), 16, 16)(" phys=0x")(
            slab->physical_page, 16, 16)();
        if(nullptr != slab->cache)
        {
            debug("kmem: cache=")(slab->cache->name)();
        }
    }
    kmem_halt();
}

[[nodiscard]] void* kmem_fail(const char* reason, size_t size, KmallocFlags flags)
{
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
            kmem_corruption("invalid slab list", &cache);
    }

    kmem_corruption("invalid slab list", &cache);
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

[[nodiscard]] void* allocate_from_slab_locked(KmemCache& cache, SlabPageHeader* slab)
{
    if((nullptr == slab) || (nullptr == slab->free_list) || (0u == slab->free_count))
    {
        kmem_corruption("empty slab allocation", slab);
    }

    void* object = slab->free_list;
    slab->free_list = *reinterpret_cast<void**>(object);
    --slab->free_count;

    const SlabListKind target = (0u == slab->free_count) ? SlabListKind::Full : SlabListKind::Partial;
    move_slab(cache, slab, target);
    return object;
}

[[nodiscard]] bool free_object_locked(KmemCache& cache, SlabPageHeader* slab, void* ptr)
{
    if(!pointer_in_slab(*slab, ptr))
    {
        kmem_corruption("pointer outside slab", ptr, slab);
    }

    if(pointer_on_free_list(*slab, ptr))
    {
        kmem_corruption("double free", ptr, slab);
    }

    *reinterpret_cast<void**>(ptr) = slab->free_list;
    slab->free_list = ptr;
    ++slab->free_count;
    if(slab->free_count > slab->object_count)
    {
        kmem_corruption("free count overflow", ptr, slab);
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
}
}  // namespace

void kmem_init(PageFrameContainer& frames)
{
    g_kmem_frames = &frames;
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
        return kmem_fail("unsupported size", size, flags);
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
            return kmem_fail("cache exhausted", size, flags);
        }

        if(!grow_cache(*cache))
        {
            return kmem_fail("out of physical pages", size, flags);
        }
        attempted_growth = true;
    }
}

void kfree(void* ptr)
{
    if(nullptr == ptr)
    {
        return;
    }
    if(!g_kmem_ready || (nullptr == g_kmem_frames))
    {
        kmem_corruption("free before initialization", ptr);
    }

    auto* slab = reinterpret_cast<SlabPageHeader*>(
        align_down(reinterpret_cast<uint64_t>(ptr), static_cast<uint64_t>(kPageSize)));
    if((nullptr == slab) || (kSlabMagic != slab->magic) || (nullptr == slab->cache))
    {
        kmem_corruption("invalid slab header", ptr, slab);
    }

    KmemCache& cache = *slab->cache;
    bool release_slab = false;
    uint64_t release_physical_page = 0;
    {
        CacheLockGuard guard(cache.lock);
        if((slab->cache != &cache) || (slab->object_size != cache.object_size))
        {
            kmem_corruption("cache mismatch", ptr, slab);
        }

        release_slab = free_object_locked(cache, slab, ptr);
        if(release_slab)
        {
            release_physical_page = slab->physical_page;
            detach_slab(cache, slab);
            if(0u == cache.slab_count)
            {
                kmem_corruption("slab count underflow", ptr, slab);
            }
            --cache.slab_count;
            memset(slab, 0, sizeof(*slab));
        }
    }

    if(release_slab)
    {
        (void)g_kmem_frames->free(release_physical_page);
    }
}