// Modern virtio-blk PCI driver using the shared virtio transport, one DMA
// backed queue, and interrupt-driven completions.
#include "drivers/block/virtio_blk.hpp"

#include "arch/x86_64/cpu/x86.hpp"
#include "core/kernel_state.hpp"
#include "debug/debug.hpp"
#include "debug/event_ring.hpp"
#include "drivers/bus/device.hpp"
#include "drivers/bus/resource.hpp"
#include "drivers/virtio/pci_transport.hpp"
#include "drivers/virtio/virtqueue.hpp"
#include "handoff/memory_layout.h"
#include "mm/dma.hpp"
#include "platform/pci_config.hpp"
#include "platform/irq_registry.hpp"
#include "platform/state.hpp"
#include "proc/thread.hpp"
#include "storage/block_device.hpp"
#include "util/string.h"

namespace
{
constexpr uint16_t kPciVendorVirtio = 0x1AF4;
constexpr uint16_t kPciDeviceVirtioBlkModern = 0x1042;
constexpr PciMatch kVirtioBlkMatches[]{{
    .vendor_id = kPciVendorVirtio,
    .device_id = kPciDeviceVirtioBlkModern,
    .match_flags = static_cast<uint8_t>(kPciMatchVendorId | kPciMatchDeviceId),
}};
constexpr uint32_t kVirtioBlkRequestIn = 0;
constexpr uint32_t kVirtioBlkRequestOut = 1;
constexpr uint16_t kVirtioBlkQueueTargetSize = 8;
constexpr uint16_t kVirtioBlkDescriptorsPerRequest = 3;
constexpr uint16_t kVirtioBlkMaxRequestSlots =
    kVirtqueueMaxSize / kVirtioBlkDescriptorsPerRequest;
constexpr uint64_t kVirtioSectorSize = 512;
constexpr const char* kVirtioSector0Prefix = "OS1 VIRTIO TEST DISK SECTOR 0 SIGNATURE";
constexpr const char* kVirtioSector1Prefix = "OS1 VIRTIO TEST DISK SECTOR 1 PAYLOAD";
constexpr const char* kVirtioWriteProbe = "OS1 VIRTIO WRITE PATH OK";
constexpr uint8_t kVirtioStatusOk = 0;

struct [[gnu::packed]] VirtioBlkConfig
{
    uint64_t capacity;
    uint32_t size_max;
    uint32_t seg_max;
    uint16_t cylinders;
    uint8_t heads;
    uint8_t sectors;
    uint32_t blk_size;
};

struct [[gnu::packed]] VirtioBlkRequestHeader
{
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
};

struct VirtioBlkRequestSlot
{
    bool active = false;
    uint16_t head_descriptor = 0;
    uint16_t descriptor_mask = 0;
    DmaBuffer buffer{};
    VirtioBlkRequestHeader* header = nullptr;
    uint8_t* data = nullptr;
    volatile uint8_t* status = nullptr;
    BlockRequest* request = nullptr;
};

struct VirtioBlkState
{
    bool present = false;
    DeviceId owner{DeviceBus::Pci, 0};
    uint16_t queue_size = 0;
    uint16_t request_slot_count = 0;
    uint16_t pci_index = 0;
    uint16_t descriptor_in_use_mask = 0;
    uint64_t capacity_sectors = 0;
    VirtioPciTransport transport{};
    Virtqueue queue{};
    VirtioBlkRequestSlot request_slots[kVirtioBlkMaxRequestSlots]{};
};

constinit VirtioBlkState g_virtio_blk{};
BlockDevice g_virtio_blk_device{};

[[nodiscard]] uint16_t virtio_blk_slot_count(uint16_t queue_size)
{
    return static_cast<uint16_t>(queue_size / kVirtioBlkDescriptorsPerRequest);
}

[[nodiscard]] uint16_t virtio_blk_descriptor_mask(uint16_t head_descriptor)
{
    return static_cast<uint16_t>((1u << head_descriptor) | (1u << (head_descriptor + 1u)) |
                                 (1u << (head_descriptor + 2u)));
}

void virtio_blk_fail_immediately(BlockRequest& request, BlockRequestStatus status)
{
    request.completed = true;
    request.status = status;
    request.bytes_transferred = 0;
    request.driver_context = nullptr;
}

bool virtio_blk_allocate_request_buffers(PageFrameContainer& frames, VirtioBlkState& state)
{
    const size_t request_buffer_size = sizeof(VirtioBlkRequestHeader) + kVirtioSectorSize + 1u;
    for(uint16_t slot_index = 0; slot_index < state.request_slot_count; ++slot_index)
    {
        auto& slot = state.request_slots[slot_index];
        slot = {};
        slot.head_descriptor = static_cast<uint16_t>(slot_index * kVirtioBlkDescriptorsPerRequest);
        slot.descriptor_mask = virtio_blk_descriptor_mask(slot.head_descriptor);
        if(!dma_allocate_buffer(
               frames, state.owner, request_buffer_size, DmaDirection::Bidirectional, slot.buffer))
        {
            return false;
        }

        slot.header = static_cast<VirtioBlkRequestHeader*>(slot.buffer.virtual_address);
        slot.data = static_cast<uint8_t*>(slot.buffer.virtual_address) + sizeof(VirtioBlkRequestHeader);
        slot.status = reinterpret_cast<volatile uint8_t*>(
            static_cast<uint8_t*>(slot.buffer.virtual_address) + sizeof(VirtioBlkRequestHeader) +
            kVirtioSectorSize);
    }
    return true;
}

void virtio_blk_release_request_slot(VirtioBlkState& state, VirtioBlkRequestSlot& slot)
{
    state.descriptor_in_use_mask =
        static_cast<uint16_t>(state.descriptor_in_use_mask & ~slot.descriptor_mask);
    slot.active = false;
    slot.request = nullptr;
}

void virtio_blk_release_request_buffers(PageFrameContainer& frames, VirtioBlkState& state)
{
    for(uint16_t slot_index = 0; slot_index < kVirtioBlkMaxRequestSlots; ++slot_index)
    {
        auto& slot = state.request_slots[slot_index];
        if(nullptr != slot.request)
        {
            block_request_complete(*slot.request, BlockRequestStatus::Timeout);
        }
        dma_release_buffer(frames, slot.buffer);
        slot = {};
    }
    state.request_slot_count = 0;
    state.descriptor_in_use_mask = 0;
}

void virtio_blk_release_runtime_resources(PageFrameContainer& frames, VirtioBlkState& state)
{
    if(nullptr != state.transport.device)
    {
        pci_release_interrupt(*state.transport.device, state.transport.interrupt);
    }
    virtio_blk_release_request_buffers(frames, state);
    virtqueue_release(frames, state.queue);
    release_pci_bars_for_owner(state.owner);
    platform_release_irq_routes_for_owner(state.owner);
}

VirtioBlkRequestSlot* virtio_blk_find_request_slot(VirtioBlkState& state, uint16_t head_descriptor)
{
    for(uint16_t slot_index = 0; slot_index < state.request_slot_count; ++slot_index)
    {
        auto& slot = state.request_slots[slot_index];
        if(slot.head_descriptor == head_descriptor)
        {
            return &slot;
        }
    }
    return nullptr;
}

VirtioBlkRequestSlot* virtio_blk_acquire_request_slot(VirtioBlkState& state)
{
    for(uint16_t slot_index = 0; slot_index < state.request_slot_count; ++slot_index)
    {
        auto& slot = state.request_slots[slot_index];
        if(slot.active || (0 != (state.descriptor_in_use_mask & slot.descriptor_mask)))
        {
            continue;
        }

        slot.active = true;
        state.descriptor_in_use_mask =
            static_cast<uint16_t>(state.descriptor_in_use_mask | slot.descriptor_mask);
        return &slot;
    }
    return nullptr;
}

void virtio_blk_drain_used(VirtioBlkState& state)
{
    dma_sync_for_cpu(state.queue.ring_memory);

    VirtqUsedElem used{};
    while(virtqueue_consume_used(state.queue, used))
    {
        auto* slot = virtio_blk_find_request_slot(state, static_cast<uint16_t>(used.id));
        if((nullptr == slot) || !slot->active || (nullptr == slot->request))
        {
            continue;
        }

        BlockRequest* request = slot->request;
        dma_sync_for_cpu(slot->buffer);

        const uint8_t completion_status = *slot->status;
        const uint32_t completion_bytes = used.len;
        const bool success = kVirtioStatusOk == completion_status;
        if(success && (BlockOperation::Read == request->operation) && (nullptr != request->buffer))
        {
            memcpy(request->buffer, slot->data, kVirtioSectorSize);
        }

        virtio_blk_release_request_slot(state, *slot);
        request->driver_context = nullptr;
        if(success)
        {
            kernel_event::record(OS1_KERNEL_EVENT_BLOCK_IO,
                                 OS1_KERNEL_EVENT_FLAG_SUCCESS,
                                 request->sector,
                                 kVirtioSectorSize,
                                 static_cast<uint64_t>(request->operation),
                                 completion_bytes);
            block_request_complete(*request,
                                   BlockRequestStatus::Success,
                                   static_cast<uint32_t>(kVirtioSectorSize));
            continue;
        }

        kernel_event::record(OS1_KERNEL_EVENT_BLOCK_IO,
                             OS1_KERNEL_EVENT_FLAG_FAILURE,
                             request->sector,
                             kVirtioSectorSize,
                             completion_status,
                             completion_bytes);
        block_request_complete(*request, BlockRequestStatus::DeviceError);
    }
}

bool virtio_blk_wait_for_boot_completion(VirtioBlkState& state, BlockRequest& request)
{
    Thread* thread = current_thread();
    if((nullptr == thread) || (nullptr != thread->process))
    {
        return true;
    }

    for(uint32_t spin = 0; spin < 10000000u; ++spin)
    {
        if(request.completed)
        {
            return true;
        }

        virtio_blk_drain_used(state);
        if(request.completed)
        {
            return true;
        }

        pause();
    }
    return false;
}

void virtio_blk_irq(void* data)
{
    auto& state = *static_cast<VirtioBlkState*>(data);
    if(nullptr != state.transport.isr_status &&
       (PciInterruptMode::LegacyIntx == state.transport.interrupt.mode))
    {
        (void)*state.transport.isr_status;
    }

    virtio_blk_drain_used(state);
}

bool virtio_blk_submit_request(BlockDevice& device, BlockRequest& request)
{
    auto& state = *static_cast<VirtioBlkState*>(device.driver_state);
    if(!state.present || (nullptr == request.buffer) || (1u != request.sector_count))
    {
        virtio_blk_fail_immediately(request, BlockRequestStatus::Invalid);
        return false;
    }
    if(request.sector >= state.capacity_sectors)
    {
        virtio_blk_fail_immediately(request, BlockRequestStatus::Invalid);
        return false;
    }

    const bool is_read = BlockOperation::Read == request.operation;
    const bool is_write = BlockOperation::Write == request.operation;
    if(!is_read && !is_write)
    {
        virtio_blk_fail_immediately(request, BlockRequestStatus::Invalid);
        return false;
    }

    auto* slot = virtio_blk_acquire_request_slot(state);
    if(nullptr == slot)
    {
        virtio_blk_fail_immediately(request, BlockRequestStatus::Busy);
        return false;
    }

    kernel_event::record(OS1_KERNEL_EVENT_BLOCK_IO,
                         OS1_KERNEL_EVENT_FLAG_BEGIN,
                         request.sector,
                         device.sector_size,
                         static_cast<uint64_t>(request.operation),
                         0);

    request.completed = false;
    request.status = BlockRequestStatus::Pending;
    request.bytes_transferred = 0;
    request.driver_context = slot;
    slot->request = &request;

    memset(slot->header, 0, sizeof(*slot->header));
    memset(slot->data, 0, static_cast<uint64_t>(kVirtioSectorSize));
    *slot->status = 0xFFu;
    if(is_write)
    {
        memcpy(slot->data, request.buffer, kVirtioSectorSize);
    }

    slot->header->type = is_read ? kVirtioBlkRequestIn : kVirtioBlkRequestOut;
    slot->header->sector = request.sector;

    state.queue.desc[slot->head_descriptor].addr = slot->buffer.physical_address;
    state.queue.desc[slot->head_descriptor].len = sizeof(VirtioBlkRequestHeader);
    state.queue.desc[slot->head_descriptor].flags = kVirtqDescFlagNext;
    state.queue.desc[slot->head_descriptor].next = static_cast<uint16_t>(slot->head_descriptor + 1u);

    state.queue.desc[slot->head_descriptor + 1u].addr =
        slot->buffer.physical_address + sizeof(VirtioBlkRequestHeader);
    state.queue.desc[slot->head_descriptor + 1u].len = kVirtioSectorSize;
    state.queue.desc[slot->head_descriptor + 1u].flags =
        is_read ? static_cast<uint16_t>(kVirtqDescFlagWrite | kVirtqDescFlagNext) : kVirtqDescFlagNext;
    state.queue.desc[slot->head_descriptor + 1u].next = static_cast<uint16_t>(slot->head_descriptor + 2u);

    state.queue.desc[slot->head_descriptor + 2u].addr =
        slot->buffer.physical_address + sizeof(VirtioBlkRequestHeader) + kVirtioSectorSize;
    state.queue.desc[slot->head_descriptor + 2u].len = 1;
    state.queue.desc[slot->head_descriptor + 2u].flags = kVirtqDescFlagWrite;
    state.queue.desc[slot->head_descriptor + 2u].next = 0;

    dma_sync_for_device(slot->buffer);
    dma_sync_for_device(state.queue.ring_memory);
    if(!virtqueue_submit(state.queue, slot->head_descriptor))
    {
        request.driver_context = nullptr;
        virtio_blk_release_request_slot(state, *slot);
        virtio_blk_fail_immediately(request, BlockRequestStatus::Invalid);
        return false;
    }
    virtio_pci_notify_queue(state.transport, 0);
    if(!virtio_blk_wait_for_boot_completion(state, request))
    {
        request.driver_context = nullptr;
        if(slot->active && (slot->request == &request))
        {
            virtio_blk_release_request_slot(state, *slot);
        }
        kernel_event::record(OS1_KERNEL_EVENT_BLOCK_IO,
                             OS1_KERNEL_EVENT_FLAG_FAILURE,
                             request.sector,
                             device.sector_size,
                             3,
                             0);
        virtio_blk_fail_immediately(request, BlockRequestStatus::Timeout);
        return false;
    }
    return true;
}

bool virtio_blk_flush(BlockDevice&, BlockRequest& request)
{
    request.completed = true;
    request.status = BlockRequestStatus::Invalid;
    return false;
}

bool verify_virtio_blk_prefix(BlockDevice& device, uint64_t sector, const char* expected_prefix)
{
    uint8_t buffer[kVirtioSectorSize]{};
    if(!block_read_sync(device, sector, buffer, 1))
    {
        return false;
    }
    const size_t prefix_length = strlen(expected_prefix);
    if(0 != memcmp(buffer, expected_prefix, prefix_length))
    {
        debug("virtio-blk: sector verification failed sector=")(sector)();
        return false;
    }
    return true;
}

bool verify_virtio_blk_write(BlockDevice& device)
{
    uint8_t buffer[kVirtioSectorSize]{};
    memcpy(buffer, kVirtioWriteProbe, strlen(kVirtioWriteProbe));
    if(!block_write_sync(device, 2, buffer, 1))
    {
        debug("virtio-blk: write smoke failed")();
        return false;
    }

    uint8_t verify[kVirtioSectorSize]{};
    if(!block_read_sync(device, 2, verify, 1))
    {
        return false;
    }
    if(0 != memcmp(verify, buffer, sizeof(buffer)))
    {
        debug("virtio-blk: write verification failed")();
        return false;
    }
    return true;
}

void virtio_blk_threaded_smoke_thread()
{
    BlockDevice* device = const_cast<BlockDevice*>(virtio_blk_block_device());
    const bool ok = (nullptr != device) && verify_virtio_blk_prefix(*device, 0, kVirtioSector0Prefix) &&
                    verify_virtio_blk_write(*device);
    debug(ok ? "virtio-blk threaded smoke ok" : "virtio-blk threaded smoke failed")();
    mark_current_thread_dying(ok ? 0 : 1);
    for(;;)
    {
        asm volatile("hlt" : : : "memory");
    }
}
}  // namespace

bool probe_virtio_blk_device(VirtualMemory& kernel_vm,
                             PageFrameContainer& frames,
                             const PciDevice& device,
                             size_t device_index,
                             VirtioBlkDevice& public_device)
{
    if((device.vendor_id != kPciVendorVirtio) || (device.device_id != kPciDeviceVirtioBlkModern))
    {
        return true;
    }
    if(g_virtio_blk.present)
    {
        debug("virtio-blk: additional device ignored")();
        return true;
    }

    auto& state = g_virtio_blk;
    state = {};
    state.owner = DeviceId{DeviceBus::Pci, static_cast<uint16_t>(device_index)};
    state.pci_index = static_cast<uint16_t>(device_index);

    if(!virtio_pci_bind_transport(
           kernel_vm, state.owner, device, sizeof(VirtioBlkConfig), state.transport))
    {
        debug("virtio-blk: transport bind failed")();
        return false;
    }

    virtio_pci_write_device_status(state.transport, 0);
    virtio_pci_write_device_status(state.transport, kVirtioStatusAcknowledge);
    virtio_pci_write_device_status(
        state.transport,
        static_cast<uint8_t>(state.transport.common_cfg->device_status | kVirtioStatusDriver));
    if(!virtio_pci_negotiate_features(
           state.transport, kVirtioFeatureVersion1, kVirtioFeatureVersion1))
    {
        debug("virtio-blk: feature negotiation rejected")();
        return false;
    }

    state.transport.common_cfg->queue_select = 0;
    const uint16_t device_queue_size = state.transport.common_cfg->queue_size;
    if(device_queue_size < 3u)
    {
        debug("virtio-blk: queue too small")();
        return false;
    }
    state.queue_size =
        (device_queue_size < kVirtioBlkQueueTargetSize) ? device_queue_size : kVirtioBlkQueueTargetSize;
    state.request_slot_count = virtio_blk_slot_count(state.queue_size);
    if(0 == state.request_slot_count)
    {
        debug("virtio-blk: queue leaves no request slots")();
        return false;
    }

    if(!virtqueue_allocate(frames, state.owner, state.queue_size, state.queue))
    {
        debug("virtio-blk: queue DMA allocation failed")();
        return false;
    }
    if(!virtio_blk_allocate_request_buffers(frames, state))
    {
        debug("virtio-blk: request DMA allocation failed")();
        virtqueue_release(frames, state.queue);
        return false;
    }

    if(!virtio_pci_bind_queue_interrupt(kernel_vm, state.transport, 0, 0, virtio_blk_irq, &state))
    {
        debug("virtio-blk: interrupt bind failed")();
        virtio_blk_release_request_buffers(frames, state);
        virtqueue_release(frames, state.queue);
        release_pci_bars_for_owner(state.owner);
        return false;
    }
    if(!virtio_pci_setup_queue(state.transport,
                               0,
                               state.queue_size,
                               state.queue.ring_memory.physical_address,
                               state.queue.ring_memory.physical_address + kPageSize,
                               state.queue.ring_memory.physical_address + 2u * kPageSize))
    {
        debug("virtio-blk: queue setup failed")();
        virtio_blk_release_runtime_resources(frames, state);
        return false;
    }

    auto* config = reinterpret_cast<volatile VirtioBlkConfig*>(state.transport.device_cfg);
    state.capacity_sectors = config->capacity;
    virtio_pci_write_device_status(
        state.transport,
        static_cast<uint8_t>(state.transport.common_cfg->device_status | kVirtioStatusDriverOk));
    state.present = true;

    public_device.present = true;
    public_device.queue_size = state.queue_size;
    public_device.capacity_sectors = state.capacity_sectors;
    public_device.pci_index = static_cast<uint16_t>(device_index);

    g_virtio_blk_device.name = "virtio-blk";
    g_virtio_blk_device.sector_count = state.capacity_sectors;
    g_virtio_blk_device.sector_size = static_cast<uint32_t>(kVirtioSectorSize);
    g_virtio_blk_device.max_sectors_per_request = 1;
    g_virtio_blk_device.queue_depth = state.request_slot_count;
    g_virtio_blk_device.driver_state = &state;
    g_virtio_blk_device.submit = virtio_blk_submit_request;
    g_virtio_blk_device.flush = virtio_blk_flush;

    (void)device_binding_publish(state.owner, static_cast<uint16_t>(device_index), "virtio-blk", &state);
    (void)device_binding_set_state(state.owner, DeviceState::Started);

    debug("virtio-blk: ready pci=")(device.bus, 16, 2)(":")(device.slot, 16, 2)(".")(
        device.function, 16, 1)(" sectors=")(state.capacity_sectors)(" qsize=")(state.queue_size)(
        " depth=")(state.request_slot_count)(" irq=")(state.transport.interrupt.vector, 16, 2)();
    kernel_event::record(OS1_KERNEL_EVENT_PCI_BIND,
                         OS1_KERNEL_EVENT_FLAG_SUCCESS,
                         static_cast<uint64_t>(device_index),
                         pci_bdf(device),
                         (static_cast<uint64_t>(device.vendor_id) << 16) | device.device_id,
                         state.capacity_sectors);
    return true;
}

bool probe_virtio_blk_pci_driver(VirtualMemory& kernel_vm,
                                 PageFrameContainer& frames,
                                 const PciDevice& device,
                                 size_t device_index,
                                 DeviceId id)
{
    (void)id;
    return probe_virtio_blk_device(
        kernel_vm, frames, device, device_index, g_platform.virtio_blk_public);
}

void remove_virtio_blk_device(DeviceId id)
{
    if(!g_virtio_blk.present || (id.bus != g_virtio_blk.owner.bus) || (id.index != g_virtio_blk.owner.index))
    {
        return;
    }

    virtio_blk_release_runtime_resources(page_frames, g_virtio_blk);
    device_binding_remove(id);
    g_platform.block_device = nullptr;
    memset(&g_platform.virtio_blk_public, 0, sizeof(g_platform.virtio_blk_public));
    g_virtio_blk = {};
    g_virtio_blk_device = {};
}

const PciDriver& virtio_blk_pci_driver()
{
    static const PciDriver driver{
        .name = "virtio-blk",
        .matches = kVirtioBlkMatches,
        .match_count = sizeof(kVirtioBlkMatches) / sizeof(kVirtioBlkMatches[0]),
        .probe = probe_virtio_blk_pci_driver,
        .remove = remove_virtio_blk_device,
    };
    return driver;
}

const BlockDevice* virtio_blk_block_device()
{
    return g_virtio_blk.present ? &g_virtio_blk_device : nullptr;
}

bool run_virtio_blk_smoke()
{
    if(!g_virtio_blk.present)
    {
        return true;
    }
    if(g_virtio_blk.capacity_sectors < 3u)
    {
        debug("virtio-blk: test disk too small")();
        return false;
    }
    if(!verify_virtio_blk_prefix(g_virtio_blk_device, 0, kVirtioSector0Prefix) ||
       !verify_virtio_blk_prefix(g_virtio_blk_device, 1, kVirtioSector1Prefix) ||
       !verify_virtio_blk_write(g_virtio_blk_device))
    {
        return false;
    }
    debug("virtio-blk smoke ok")();
    return true;
}

Thread* start_virtio_blk_threaded_smoke(Process* kernel_process, PageFrameContainer& frames)
{
    if(!g_virtio_blk.present)
    {
        return nullptr;
    }
    if(nullptr == kernel_process)
    {
        return nullptr;
    }

    return create_kernel_thread(kernel_process, virtio_blk_threaded_smoke_thread, frames);
}
