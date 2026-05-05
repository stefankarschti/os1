#include "drivers/net/arp_cache.hpp"

#if defined(OS1_HOST_TEST)
#include <string.h>
#else
#include "util/string.h"
#endif

namespace
{
using ArpCacheGuard = IrqSpinGuard<Spinlock>;

[[nodiscard]] bool ipv4_equal(const uint8_t lhs[4], const uint8_t rhs[4])
{
    return 0 == memcmp(lhs, rhs, 4u);
}
}  // namespace

bool ArpCache::initialize(const char* cache_name)
{
    if((nullptr == cache_name) || (0 == cache_name[0]))
    {
        return false;
    }
    if(nullptr != cache_)
    {
        return true;
    }

    cache_ = kmem_cache_create(cache_name, sizeof(Entry), alignof(Entry));
    return nullptr != cache_;
}

bool ArpCache::upsert(const uint8_t ipv4[4], const uint8_t mac[6], KmallocFlags flags)
{
    if((nullptr == ipv4) || (nullptr == mac) || (nullptr == cache_))
    {
        return false;
    }

    ArpCacheGuard guard(lock_);
    for(Entry* entry = head_; nullptr != entry; entry = entry->next)
    {
        if(ipv4_equal(entry->ipv4, ipv4))
        {
            memcpy(entry->mac, mac, 6u);
            return true;
        }
    }

    auto* entry = static_cast<Entry*>(kmem_cache_alloc(cache_, flags));
    if(nullptr == entry)
    {
        return false;
    }

    entry->next = head_;
    memcpy(entry->ipv4, ipv4, 4u);
    memcpy(entry->mac, mac, 6u);
    head_ = entry;
    ++count_;
    return true;
}

bool ArpCache::lookup(const uint8_t ipv4[4], uint8_t mac[6])
{
    if((nullptr == ipv4) || (nullptr == mac) || (nullptr == cache_))
    {
        return false;
    }

    ArpCacheGuard guard(lock_);
    for(Entry* entry = head_; nullptr != entry; entry = entry->next)
    {
        if(ipv4_equal(entry->ipv4, ipv4))
        {
            memcpy(mac, entry->mac, 6u);
            return true;
        }
    }

    return false;
}

size_t ArpCache::entry_count()
{
    ArpCacheGuard guard(lock_);
    return count_;
}

void ArpCache::clear()
{
    if(nullptr == cache_)
    {
        return;
    }

    ArpCacheGuard guard(lock_);
    Entry* entry = head_;
    head_ = nullptr;
    count_ = 0;
    while(nullptr != entry)
    {
        Entry* next = entry->next;
        kmem_cache_free(cache_, entry);
        entry = next;
    }
}

bool ArpCache::destroy()
{
    if(nullptr == cache_)
    {
        return true;
    }

    clear();
    if(!kmem_cache_destroy(cache_))
    {
        return false;
    }
    cache_ = nullptr;
    return true;
}
