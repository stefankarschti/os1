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

struct VirtioBlkState
{
    bool present = false;
    DeviceId owner{DeviceBus::Pci, 0};
    uint16_t queue_size = 0;
    uint16_t pci_index = 0;
    uint64_t capacity_sectors = 0;
    VirtioPciTransport transport{};
    Virtqueue queue{};
    DmaBuffer request_buffer{};
    VirtioBlkRequestHeader* request_header = nullptr;
    uint8_t* request_data = nullptr;
    volatile uint8_t* request_status = nullptr;
    volatile bool request_inflight = false;
    volatile bool request_completed = false;
    volatile uint8_t last_completion_status = 0xFFu;
    volatile uint32_t last_completion_bytes = 0;
};

constinit VirtioBlkState g_virtio_blk{};
BlockDevice g_virtio_blk_device{};

void virtio_blk_irq(void* data)
{
    auto& state = *static_cast<VirtioBlkState*>(data);
    if(nullptr != state.transport.isr_status &&
       (PciInterruptMode::LegacyIntx == state.transport.interrupt.mode))
    {
        (void)*state.transport.isr_status;
    }

    VirtqUsedElem used{};
    while(virtqueue_consume_used(state.queue, used))
    {
        state.last_completion_status = *state.request_status;
        state.last_completion_bytes = used.len;
        state.request_completed = true;
        state.request_inflight = false;
    }
}

bool virtio_blk_wait_for_completion(VirtioBlkState& state)
{
    for(uint32_t spin = 0; spin < 10000000u; ++spin)
    {
        if(state.request_completed)
        {
            return true;
        }
        pause();
    }
    return false;
}

bool virtio_blk_submit_request(BlockDevice& device, BlockRequest& request)
{
    auto& state = *static_cast<VirtioBlkState*>(device.driver_state);
    if(!state.present || state.request_inflight || (nullptr == request.buffer) || (1u != request.sector_count))
    {
        request.completed = true;
        request.status = BlockRequestStatus::Invalid;
        return false;
    }
    if(request.sector >= state.capacity_sectors)
    {
        request.completed = true;
        request.status = BlockRequestStatus::Invalid;
        return false;
    }

    const bool is_read = BlockOperation::Read == request.operation;
    const bool is_write = BlockOperation::Write == request.operation;
    if(!is_read && !is_write)
    {
        request.completed = true;
        request.status = BlockRequestStatus::Invalid;
        return false;
    }

    kernel_event::record(OS1_KERNEL_EVENT_BLOCK_IO,
                         OS1_KERNEL_EVENT_FLAG_BEGIN,
                         request.sector,
                         device.sector_size,
                         static_cast<uint64_t>(request.operation),
                         0);

    memset(state.request_header, 0, sizeof(*state.request_header));
    memset(state.request_data, 0, static_cast<uint64_t>(kVirtioSectorSize));
    *state.request_status = 0xFFu;
    if(is_write)
    {
        memcpy(state.request_data, request.buffer, kVirtioSectorSize);
    }

    state.request_header->type = is_read ? kVirtioBlkRequestIn : kVirtioBlkRequestOut;
    state.request_header->sector = request.sector;

    state.queue.desc[0].addr = state.request_buffer.physical_address;
    state.queue.desc[0].len = sizeof(VirtioBlkRequestHeader);
    state.queue.desc[0].flags = kVirtqDescFlagNext;
    state.queue.desc[0].next = 1;

    state.queue.desc[1].addr = state.request_buffer.physical_address + sizeof(VirtioBlkRequestHeader);
    state.queue.desc[1].len = kVirtioSectorSize;
    state.queue.desc[1].flags = is_read ? static_cast<uint16_t>(kVirtqDescFlagWrite | kVirtqDescFlagNext)
                                        : kVirtqDescFlagNext;
    state.queue.desc[1].next = 2;

    state.queue.desc[2].addr =
        state.request_buffer.physical_address + sizeof(VirtioBlkRequestHeader) + kVirtioSectorSize;
    state.queue.desc[2].len = 1;
    state.queue.desc[2].flags = kVirtqDescFlagWrite;
    state.queue.desc[2].next = 0;

    dma_sync_for_device(state.request_buffer);
    dma_sync_for_device(state.queue.ring_memory);
    state.request_completed = false;
    state.request_inflight = true;
    state.last_completion_status = 0xFFu;
    state.last_completion_bytes = 0;
    if(!virtqueue_submit(state.queue, 0))
    {
        state.request_inflight = false;
        request.completed = true;
        request.status = BlockRequestStatus::Invalid;
        return false;
    }
    virtio_pci_notify_queue(state.transport, 0);

    if(!virtio_blk_wait_for_completion(state))
    {
        state.request_inflight = false;
        request.completed = true;
        request.status = BlockRequestStatus::Timeout;
        kernel_event::record(OS1_KERNEL_EVENT_BLOCK_IO,
                             OS1_KERNEL_EVENT_FLAG_FAILURE,
                             request.sector,
                             device.sector_size,
                             3,
                             0);
        return false;
    }

    dma_sync_for_cpu(state.request_buffer);
    request.completed = true;
    if(kVirtioStatusOk != state.last_completion_status)
    {
        request.status = BlockRequestStatus::DeviceError;
        kernel_event::record(OS1_KERNEL_EVENT_BLOCK_IO,
                             OS1_KERNEL_EVENT_FLAG_FAILURE,
                             request.sector,
                             device.sector_size,
                             state.last_completion_status,
                             state.last_completion_bytes);
        return false;
    }

    if(is_read)
    {
        memcpy(request.buffer, state.request_data, kVirtioSectorSize);
    }
    request.bytes_transferred = static_cast<uint32_t>(kVirtioSectorSize);
    request.status = BlockRequestStatus::Success;
    kernel_event::record(OS1_KERNEL_EVENT_BLOCK_IO,
                         OS1_KERNEL_EVENT_FLAG_SUCCESS,
                         request.sector,
                         device.sector_size,
                         static_cast<uint64_t>(request.operation),
                         state.last_completion_bytes);
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

    if(!virtqueue_allocate(frames, state.owner, state.queue_size, state.queue))
    {
        debug("virtio-blk: queue DMA allocation failed")();
        return false;
    }
    if(!dma_allocate_buffer(
           frames, state.owner, sizeof(VirtioBlkRequestHeader) + kVirtioSectorSize + 1u, DmaDirection::Bidirectional, state.request_buffer))
    {
        debug("virtio-blk: request DMA allocation failed")();
        return false;
    }

    state.request_header = static_cast<VirtioBlkRequestHeader*>(state.request_buffer.virtual_address);
    state.request_data =
        static_cast<uint8_t*>(state.request_buffer.virtual_address) + sizeof(VirtioBlkRequestHeader);
    state.request_status = reinterpret_cast<volatile uint8_t*>(
        static_cast<uint8_t*>(state.request_buffer.virtual_address) + sizeof(VirtioBlkRequestHeader) +
        kVirtioSectorSize);

    if(!virtio_pci_bind_queue_interrupt(kernel_vm, state.transport, 0, 0, virtio_blk_irq, &state))
    {
        debug("virtio-blk: interrupt bind failed")();
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
    g_virtio_blk_device.queue_depth = 1;
    g_virtio_blk_device.driver_state = &state;
    g_virtio_blk_device.submit = virtio_blk_submit_request;
    g_virtio_blk_device.flush = virtio_blk_flush;

    (void)device_binding_publish(state.owner, static_cast<uint16_t>(device_index), "virtio-blk", &state);
    (void)device_binding_set_state(state.owner, DeviceState::Started);

    debug("virtio-blk: ready pci=")(device.bus, 16, 2)(":")(device.slot, 16, 2)(".")(
        device.function, 16, 1)(" sectors=")(state.capacity_sectors)(" qsize=")(state.queue_size)(
        " irq=")(state.transport.interrupt.vector, 16, 2)();
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

    pci_release_interrupt(*g_virtio_blk.transport.device, g_virtio_blk.transport.interrupt);
    dma_release_buffer(page_frames, g_virtio_blk.request_buffer);
    virtqueue_release(page_frames, g_virtio_blk.queue);
    release_pci_bars_for_owner(id);
    platform_release_irq_routes_for_owner(id);
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
