// Modern virtio-net PCI driver using the shared virtio transport, explicit DMA
// packet buffers, and interrupt-driven RX/TX completion.
#include "drivers/net/virtio_net.hpp"

#include "arch/x86_64/cpu/x86.hpp"
#include "debug/debug.hpp"
#include "debug/event_ring.hpp"
#include "drivers/bus/device.hpp"
#include "drivers/bus/driver_registry.hpp"
#include "drivers/bus/resource.hpp"
#include "drivers/virtio/pci_transport.hpp"
#include "drivers/virtio/virtqueue.hpp"
#include "handoff/memory_layout.h"
#include "mm/dma.hpp"
#include "mm/page_frame.hpp"
#include "platform/irq_registry.hpp"
#include "platform/pci_config.hpp"
#include "util/string.h"

extern PageFrameContainer page_frames;

namespace
{
constexpr uint16_t kPciVendorVirtio = 0x1AF4;
constexpr uint16_t kPciDeviceVirtioNetModern = 0x1041;
constexpr PciMatch kVirtioNetMatches[]{
    {
        .vendor_id = kPciVendorVirtio,
        .device_id = kPciDeviceVirtioNetModern,
        .match_flags = static_cast<uint8_t>(kPciMatchVendorId | kPciMatchDeviceId),
    },
};
constexpr uint64_t kVirtioNetFeatureMac = 1ull << 5;
constexpr uint16_t kVirtioNetRxQueueIndex = 0;
constexpr uint16_t kVirtioNetTxQueueIndex = 1;
constexpr uint16_t kVirtioNetQueueTargetSize = 8;
constexpr size_t kVirtioNetPacketBufferBytes = 2048;
constexpr size_t kVirtioNetArpPacketBytes = 42;
constexpr uint16_t kEtherTypeArp = 0x0806;
constexpr uint16_t kArpHardwareEthernet = 0x0001;
constexpr uint16_t kArpProtocolIpv4 = 0x0800;
constexpr uint16_t kArpOpcodeRequest = 0x0001;
constexpr uint16_t kArpOpcodeReply = 0x0002;
constexpr uint8_t kProbeSenderIp[]{10u, 0u, 2u, 15u};
constexpr uint8_t kProbeTargetIp[]{10u, 0u, 2u, 2u};

struct [[gnu::packed]] VirtioNetConfig
{
    uint8_t mac[6];
};

struct [[gnu::packed]] VirtioNetHdr
{
    uint8_t flags;
    uint8_t gso_type;
    uint16_t hdr_len;
    uint16_t gso_size;
    uint16_t csum_start;
    uint16_t csum_offset;
    uint16_t num_buffers;
};

enum class PacketBufferState : uint8_t
{
    Free = 0,
    Submitted = 1,
};

struct PacketBuffer
{
    DmaBuffer dma{};
    VirtioNetHdr* header = nullptr;
    uint8_t* frame = nullptr;
    uint16_t frame_capacity = 0;
    PacketBufferState state = PacketBufferState::Free;
    uint32_t frame_length = 0;
};

struct VirtioNetState
{
    bool present = false;
    DeviceId owner{DeviceBus::Pci, 0};
    uint16_t pci_index = 0;
    uint16_t queue_size = 0;
    VirtioPciTransport transport{};
    Virtqueue rx_queue{};
    Virtqueue tx_queue{};
    PacketBuffer rx_buffer{};
    PacketBuffer tx_buffer{};
    uint8_t mac[6]{};
    volatile bool tx_inflight = false;
    volatile bool tx_completed = false;
    volatile bool smoke_rx_seen = false;
    volatile uint32_t last_rx_bytes = 0;
    volatile uint16_t last_rx_ethertype = 0;
};

constinit VirtioNetState g_virtio_net{};

[[nodiscard]] uint16_t load_be16(const uint8_t* data)
{
    return static_cast<uint16_t>((static_cast<uint16_t>(data[0]) << 8) |
                                 static_cast<uint16_t>(data[1]));
}

void store_be16(uint8_t* data, uint16_t value)
{
    data[0] = static_cast<uint8_t>(value >> 8);
    data[1] = static_cast<uint8_t>(value & 0xFFu);
}

void copy_mac(uint8_t* dst, const uint8_t* src)
{
    memcpy(dst, src, 6u);
}

bool macs_equal(const uint8_t* lhs, const uint8_t* rhs)
{
    return 0 == memcmp(lhs, rhs, 6u);
}

void load_mac_from_config(volatile const uint8_t* src, uint8_t* dst)
{
    for(size_t i = 0; i < 6u; ++i)
    {
        dst[i] = src[i];
    }
}

void reset_packet_buffer(PacketBuffer& buffer)
{
    if(!buffer.dma.active)
    {
        return;
    }

    memset(buffer.dma.virtual_address, 0, buffer.dma.size_bytes);
    buffer.frame_length = 0;
    buffer.state = PacketBufferState::Free;
}

bool allocate_packet_buffer(PageFrameContainer& frames,
                            DeviceId owner,
                            DmaDirection direction,
                            PacketBuffer& buffer)
{
    buffer = {};
    if(!dma_allocate_buffer(frames, owner, kVirtioNetPacketBufferBytes, direction, buffer.dma))
    {
        return false;
    }

    buffer.header = static_cast<VirtioNetHdr*>(buffer.dma.virtual_address);
    buffer.frame = static_cast<uint8_t*>(buffer.dma.virtual_address) + sizeof(VirtioNetHdr);
    buffer.frame_capacity =
        static_cast<uint16_t>(buffer.dma.size_bytes - sizeof(VirtioNetHdr));
    reset_packet_buffer(buffer);
    return true;
}

bool assign_shared_queue_interrupt(VirtioNetState& state, uint16_t queue_index)
{
    if(nullptr == state.transport.common_cfg)
    {
        return false;
    }

    state.transport.common_cfg->queue_select = queue_index;
    if(PciInterruptMode::Msix == state.transport.interrupt.mode)
    {
        state.transport.common_cfg->queue_msix_vector = 0;
        return 0 == state.transport.common_cfg->queue_msix_vector;
    }

    state.transport.common_cfg->queue_msix_vector = kVirtioNoVector;
    return kVirtioNoVector == state.transport.common_cfg->queue_msix_vector;
}

bool submit_rx_buffer(VirtioNetState& state)
{
    auto& buffer = state.rx_buffer;
    if(PacketBufferState::Submitted == buffer.state)
    {
        return true;
    }

    reset_packet_buffer(buffer);
    state.rx_queue.desc[0].addr = buffer.dma.physical_address;
    state.rx_queue.desc[0].len = static_cast<uint32_t>(buffer.dma.size_bytes);
    state.rx_queue.desc[0].flags = kVirtqDescFlagWrite;
    state.rx_queue.desc[0].next = 0;
    dma_sync_for_device(buffer.dma);
    dma_sync_for_device(state.rx_queue.ring_memory);
    if(!virtqueue_submit(state.rx_queue, 0))
    {
        return false;
    }

    buffer.state = PacketBufferState::Submitted;
    virtio_pci_notify_queue(state.transport, kVirtioNetRxQueueIndex);
    return true;
}

void build_arp_probe(PacketBuffer& buffer, const uint8_t mac[6])
{
    reset_packet_buffer(buffer);
    uint8_t* frame = buffer.frame;

    memset(buffer.header, 0, sizeof(*buffer.header));
    memset(frame, 0, kVirtioNetArpPacketBytes);
    memset(frame, 0xFF, 6u);
    copy_mac(frame + 6u, mac);
    store_be16(frame + 12u, kEtherTypeArp);
    store_be16(frame + 14u, kArpHardwareEthernet);
    store_be16(frame + 16u, kArpProtocolIpv4);
    frame[18] = 6u;
    frame[19] = 4u;
    store_be16(frame + 20u, kArpOpcodeRequest);
    copy_mac(frame + 22u, mac);
    memcpy(frame + 28u, kProbeSenderIp, sizeof(kProbeSenderIp));
    memset(frame + 32u, 0, 6u);
    memcpy(frame + 38u, kProbeTargetIp, sizeof(kProbeTargetIp));
    buffer.frame_length = kVirtioNetArpPacketBytes;
}

bool submit_arp_probe(VirtioNetState& state)
{
    if(state.tx_inflight)
    {
        return false;
    }

    auto& buffer = state.tx_buffer;
    build_arp_probe(buffer, state.mac);
    state.tx_queue.desc[0].addr = buffer.dma.physical_address;
    state.tx_queue.desc[0].len = static_cast<uint32_t>(sizeof(VirtioNetHdr) + buffer.frame_length);
    state.tx_queue.desc[0].flags = 0;
    state.tx_queue.desc[0].next = 0;
    dma_sync_for_device(buffer.dma);
    dma_sync_for_device(state.tx_queue.ring_memory);
    state.tx_completed = false;
    state.tx_inflight = true;
    if(!virtqueue_submit(state.tx_queue, 0))
    {
        state.tx_inflight = false;
        return false;
    }

    buffer.state = PacketBufferState::Submitted;
    virtio_pci_notify_queue(state.transport, kVirtioNetTxQueueIndex);
    return true;
}

uint16_t packet_ethertype(const PacketBuffer& buffer)
{
    return (buffer.frame_length >= 14u) ? load_be16(buffer.frame + 12u) : 0u;
}

bool is_probe_arp_reply(const PacketBuffer& buffer, const uint8_t mac[6])
{
    if(buffer.frame_length < kVirtioNetArpPacketBytes)
    {
        return false;
    }

    const uint8_t* frame = buffer.frame;
    return macs_equal(frame, mac) && (load_be16(frame + 12u) == kEtherTypeArp) &&
           (load_be16(frame + 14u) == kArpHardwareEthernet) &&
           (load_be16(frame + 16u) == kArpProtocolIpv4) && (6u == frame[18]) &&
           (4u == frame[19]) && (load_be16(frame + 20u) == kArpOpcodeReply) &&
           macs_equal(frame + 32u, mac) &&
           (0 == memcmp(frame + 28u, kProbeTargetIp, sizeof(kProbeTargetIp))) &&
           (0 == memcmp(frame + 38u, kProbeSenderIp, sizeof(kProbeSenderIp)));
}

bool wait_for_flag(volatile bool& flag)
{
    for(uint32_t spin = 0; spin < 50000000u; ++spin)
    {
        if(flag)
        {
            return true;
        }
        pause();
    }
    return false;
}

void virtio_net_irq(void* data)
{
    auto& state = *static_cast<VirtioNetState*>(data);
    if(nullptr != state.transport.isr_status &&
       (PciInterruptMode::LegacyIntx == state.transport.interrupt.mode))
    {
        (void)*state.transport.isr_status;
    }

    dma_sync_for_cpu(state.rx_queue.ring_memory);
    dma_sync_for_cpu(state.tx_queue.ring_memory);

    VirtqUsedElem used{};
    while(virtqueue_consume_used(state.rx_queue, used))
    {
        dma_sync_for_cpu(state.rx_buffer.dma);
        state.rx_buffer.state = PacketBufferState::Free;
        state.rx_buffer.frame_length =
            (used.len > sizeof(VirtioNetHdr)) ? (used.len - sizeof(VirtioNetHdr)) : 0u;
        state.last_rx_bytes = state.rx_buffer.frame_length;
        state.last_rx_ethertype = packet_ethertype(state.rx_buffer);
        kernel_event::record(OS1_KERNEL_EVENT_NET_RX,
                             OS1_KERNEL_EVENT_FLAG_SUCCESS,
                             state.last_rx_bytes,
                             state.last_rx_ethertype,
                             state.pci_index,
                             state.transport.interrupt.vector);
        if(is_probe_arp_reply(state.rx_buffer, state.mac))
        {
            state.smoke_rx_seen = true;
        }
        (void)submit_rx_buffer(state);
    }

    while(virtqueue_consume_used(state.tx_queue, used))
    {
        state.tx_buffer.state = PacketBufferState::Free;
        state.tx_inflight = false;
        state.tx_completed = true;
    }
}
}  // namespace

bool probe_virtio_net_device(VirtualMemory& kernel_vm,
                             PageFrameContainer& frames,
                             const PciDevice& device,
                             size_t device_index)
{
    if((device.vendor_id != kPciVendorVirtio) || (device.device_id != kPciDeviceVirtioNetModern))
    {
        return true;
    }
    if(g_virtio_net.present)
    {
        debug("virtio-net: additional device ignored")();
        return true;
    }

    auto& state = g_virtio_net;
    state = {};
    state.owner = DeviceId{DeviceBus::Pci, static_cast<uint16_t>(device_index)};
    state.pci_index = static_cast<uint16_t>(device_index);

    if(!virtio_pci_bind_transport(
           kernel_vm, state.owner, device, sizeof(VirtioNetConfig), state.transport))
    {
        debug("virtio-net: transport bind failed")();
        return false;
    }

    virtio_pci_write_device_status(state.transport, 0);
    virtio_pci_write_device_status(state.transport, kVirtioStatusAcknowledge);
    virtio_pci_write_device_status(
        state.transport,
        static_cast<uint8_t>(state.transport.common_cfg->device_status | kVirtioStatusDriver));
    if(!virtio_pci_negotiate_features(state.transport,
                                      kVirtioFeatureVersion1 | kVirtioNetFeatureMac,
                                      kVirtioFeatureVersion1 | kVirtioNetFeatureMac))
    {
        debug("virtio-net: feature negotiation rejected")();
        return false;
    }

    state.transport.common_cfg->queue_select = kVirtioNetRxQueueIndex;
    const uint16_t rx_device_queue_size = state.transport.common_cfg->queue_size;
    state.transport.common_cfg->queue_select = kVirtioNetTxQueueIndex;
    const uint16_t tx_device_queue_size = state.transport.common_cfg->queue_size;
    const uint16_t device_queue_size =
        (rx_device_queue_size < tx_device_queue_size) ? rx_device_queue_size : tx_device_queue_size;
    if(0 == device_queue_size)
    {
        debug("virtio-net: queue unavailable")();
        return false;
    }
    state.queue_size =
        (device_queue_size < kVirtioNetQueueTargetSize) ? device_queue_size : kVirtioNetQueueTargetSize;

    if(!virtqueue_allocate(frames, state.owner, state.queue_size, state.rx_queue) ||
       !virtqueue_allocate(frames, state.owner, state.queue_size, state.tx_queue))
    {
        debug("virtio-net: queue DMA allocation failed")();
        return false;
    }
    if(!allocate_packet_buffer(frames, state.owner, DmaDirection::FromDevice, state.rx_buffer) ||
       !allocate_packet_buffer(frames, state.owner, DmaDirection::ToDevice, state.tx_buffer))
    {
        debug("virtio-net: packet DMA allocation failed")();
        return false;
    }

    if(!virtio_pci_bind_queue_interrupt(
           kernel_vm, state.transport, kVirtioNetRxQueueIndex, 0, virtio_net_irq, &state))
    {
        debug("virtio-net: interrupt bind failed")();
        return false;
    }
    if(!virtio_pci_setup_queue(state.transport,
                               kVirtioNetRxQueueIndex,
                               state.queue_size,
                               state.rx_queue.ring_memory.physical_address,
                               state.rx_queue.ring_memory.physical_address + kPageSize,
                               state.rx_queue.ring_memory.physical_address + 2u * kPageSize) ||
       !virtio_pci_setup_queue(state.transport,
                               kVirtioNetTxQueueIndex,
                               state.queue_size,
                               state.tx_queue.ring_memory.physical_address,
                               state.tx_queue.ring_memory.physical_address + kPageSize,
                               state.tx_queue.ring_memory.physical_address + 2u * kPageSize))
    {
        debug("virtio-net: queue setup failed")();
        return false;
    }
    if(!assign_shared_queue_interrupt(state, kVirtioNetTxQueueIndex))
    {
        debug("virtio-net: tx queue vector assign failed")();
        return false;
    }
    if(!submit_rx_buffer(state))
    {
        debug("virtio-net: rx submit failed")();
        return false;
    }

    auto* config = reinterpret_cast<volatile VirtioNetConfig*>(state.transport.device_cfg);
    load_mac_from_config(config->mac, state.mac);
    virtio_pci_write_device_status(
        state.transport,
        static_cast<uint8_t>(state.transport.common_cfg->device_status | kVirtioStatusDriverOk));
    state.present = true;

    (void)device_binding_publish(state.owner, static_cast<uint16_t>(device_index), "virtio-net", &state);
    (void)device_binding_set_state(state.owner, DeviceState::Started);

    debug("virtio-net: ready pci=")(device.bus, 16, 2)(":")(device.slot, 16, 2)(".")(
        device.function, 16, 1)(" qsize=")(state.queue_size)(" irq=")(
        state.transport.interrupt.vector, 16, 2)(" mac=")(state.mac[0], 16, 2)(":")(
        state.mac[1], 16, 2)(":")(state.mac[2], 16, 2)(":")(state.mac[3], 16, 2)(":")(
        state.mac[4], 16, 2)(":")(state.mac[5], 16, 2)();
    kernel_event::record(OS1_KERNEL_EVENT_PCI_BIND,
                         OS1_KERNEL_EVENT_FLAG_SUCCESS,
                         static_cast<uint64_t>(device_index),
                         pci_bdf(device),
                         (static_cast<uint64_t>(device.vendor_id) << 16) | device.device_id,
                         state.queue_size);
    return true;
}

bool probe_virtio_net_pci_driver(VirtualMemory& kernel_vm,
                                 PageFrameContainer& frames,
                                 const PciDevice& device,
                                 size_t device_index,
                                 DeviceId id)
{
    (void)id;
    return probe_virtio_net_device(kernel_vm, frames, device, device_index);
}

void remove_virtio_net_device(DeviceId id)
{
    if(!g_virtio_net.present || (id.bus != g_virtio_net.owner.bus) ||
       (id.index != g_virtio_net.owner.index))
    {
        return;
    }

    pci_release_interrupt(*g_virtio_net.transport.device, g_virtio_net.transport.interrupt);
    dma_release_buffer(page_frames, g_virtio_net.tx_buffer.dma);
    dma_release_buffer(page_frames, g_virtio_net.rx_buffer.dma);
    virtqueue_release(page_frames, g_virtio_net.tx_queue);
    virtqueue_release(page_frames, g_virtio_net.rx_queue);
    release_pci_bars_for_owner(id);
    platform_release_irq_routes_for_owner(id);
    device_binding_remove(id);
    g_virtio_net = {};
}

const PciDriver& virtio_net_pci_driver()
{
    static const PciDriver driver{
        .name = "virtio-net",
        .matches = kVirtioNetMatches,
        .match_count = sizeof(kVirtioNetMatches) / sizeof(kVirtioNetMatches[0]),
        .probe = probe_virtio_net_pci_driver,
        .remove = remove_virtio_net_device,
    };
    return driver;
}

bool run_virtio_net_smoke()
{
    if(!g_virtio_net.present)
    {
        return true;
    }
    if(!submit_rx_buffer(g_virtio_net))
    {
        debug("virtio-net: rx smoke arm failed")();
        return false;
    }

    g_virtio_net.smoke_rx_seen = false;
    if(!submit_arp_probe(g_virtio_net))
    {
        debug("virtio-net: tx smoke submit failed")();
        return false;
    }
    if(!wait_for_flag(g_virtio_net.tx_completed))
    {
        g_virtio_net.tx_inflight = false;
        debug("virtio-net: tx smoke timeout")();
        return false;
    }
    if(!wait_for_flag(g_virtio_net.smoke_rx_seen))
    {
        debug("virtio-net: rx smoke timeout")();
        return false;
    }

    debug("virtio-net smoke ok bytes=")(g_virtio_net.last_rx_bytes)(" ethertype=0x")(
        g_virtio_net.last_rx_ethertype, 16, 4)();
    return true;
}