#pragma once

#include <stddef.h>
#include <stdint.h>

#include "mm/kmem.hpp"
#include "sync/smp.hpp"

class ArpCache
{
public:
    constexpr ArpCache() = default;

    [[nodiscard]] bool initialize(const char* cache_name = "arp_entry");
    [[nodiscard]] bool upsert(const uint8_t ipv4[4],
                              const uint8_t mac[6],
                              KmallocFlags flags = KmallocFlags::None);
    [[nodiscard]] bool lookup(const uint8_t ipv4[4], uint8_t mac[6]);
    [[nodiscard]] size_t entry_count();
    void clear();
    [[nodiscard]] bool destroy();

private:
    struct Entry
    {
        Entry* next = nullptr;
        uint8_t ipv4[4]{};
        uint8_t mac[6]{};
        uint16_t reserved = 0;
    };

    Spinlock lock_{"arp-cache"};
    KmemCache* cache_ = nullptr;
    Entry* head_ = nullptr;
    size_t count_ = 0;
};