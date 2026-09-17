
#include "virtiovsock.hpp"
#include "net/vsock/packet.hpp"
#include <hw/pci_manager.hpp>
#include <info>
#include <kernel/events.hpp>

#define VSOCK_DEBUG

#ifdef VSOCK_DEBUG
#define VDBG(fmt, ...) INFO("VirtioVsock", fmt"\n", ##__VA_ARGS__)
#else
#define VDBG(fmt, ...) /* fmt */
#endif

VirtioVsock::VirtioVsock(hw::PCI_Device& d) : VirtioPci(d) {

    /** Step 1, 2 and 3 in VirtioPci */

    /**
     * 4. Read device feature bits, and write the subset of feature bits understood by the OS and driver to the device. 
     * During this step the driver MAY read (but MUST NOT write) the device-specific configuration fields to check '
     * that it can support the device before accepting it.
     * 
     * 5. Set the FEATURES_OK status bit. The driver MUST NOT accept new feature bits after this step.
     * 
     * 6. Re-read device status to ensure the FEATURES_OK bit is still set: otherwise, the device does not support our 
     * subset of features and the device is unusable.
     */

    VDBG("Starting vsock specific init");

    /** 
     * VirtIO Standard 5.10.3.1
     * Driver should except NO_IMPLIED_STREAM if offered by device 
     * If no feature bit negotiated, the driver acts a if STREAM is negotiated
     * If SEQPACKET is negotiated, driver may assume STREAM is also (5.10.3.1)
     */

    

    probe_features();
    const uint32_t *offered_features = features();
    VDBG("Offered features: %08x %08x %08x %08x",
         offered_features[0], offered_features[1], offered_features[2], offered_features[3]);

    // Bit 32 is VERSION_1. Use split rings and ordinary 16-bit notifications;
    // EVENT_IDX, packed rings and NOTIFICATION_DATA are not implemented.
    if (!(offered_features[1] & 1U)) {
        setup_complete(false);
        return;
    }
    uint32_t wanted_features[4] = {
        offered_features[0] & (1U << VIRTIO_VSOCK_F_STREAM),
        1U, 0U, 0U
    };
    if (!negotiate_features(wanted_features)) {
        setup_complete(false);
        return;
    }

    VDBG("Negotiated features: %08x %08x %08x %08x",
         wanted_features[0], wanted_features[1], wanted_features[2], wanted_features[3]);

    /**
     * 7. Perform device-specific setup, including discovery of virtqueues for the device, optional per-bus setup, 
     * reading and possibly writing the device’s virtio configuration space, and population of virtqueues.
     */

    auto *device_cfg = map_capability(VIRTIO_PCI_CAP_DEVICE_CFG);
    struct virtio_vsock_config *cfg = (virtio_vsock_config*)device_cfg;

    uint64_t cid = cfg->guest_cid;
    bool valid_cid = net::vsock::cid_is_valid(cid);
    CHECKSERT(valid_cid, "CID (%lu) assigned to device", cid);
    _cid = (uint32_t)cid;

    /** RX que is 0, TX Queue is 1 - Virtio Std. §5.1.2  */


    std::string name = "vsock"; // TODO: Temp solution

    new (&rx_q) Virtqueue(name + ".rx_q", queue_size(0), 0, 0);
    bool success = setup_queue(0, rx_q, 0);
    CHECKSERT(success, "RX queue (%u) assigned (%p) to device",
                rx_q.size(), rx_q.queue_desc());

    new (&tx_q) Virtqueue(name + ".tx_q", queue_size(1), 1, 0);
    success = setup_queue(1, tx_q, 1);
    CHECKSERT(success, "TX queue (%u) assigned (%p) to device",
                tx_q.size(), tx_q.queue_desc());

    new (&ctrl_q) Virtqueue(name + ".ctrl_q", queue_size(2), 2, 0);
    success = setup_queue(2, ctrl_q, 2);
    CHECKSERT(success, "CTRL queue (%u) assigned (%p) to device",
                ctrl_q.size(), ctrl_q.queue_desc());

    Events::get().subscribe(get_irq(0), {this, &VirtioVsock::handle_rx});
    Events::get().subscribe(get_irq(1), {this, &VirtioVsock::handle_tx});
    Events::get().subscribe(get_irq(2), {this, &VirtioVsock::handle_event});
    VDBG("Subscribed to IRQs %u (RX), %u (TX), %u (CTRL)", get_irq(0), get_irq(1), get_irq(2));


    for (auto& buffer : rx_buffers_) {
        Virtqueue::Token token{
            {buffer.data(), buffer.size()},
            Virtqueue::Token::IN
        };

        rx_q.enqueue(std::span{&token, 1});
    }

    // TODO: Also add buffers to the ctrl_q
    rx_q.enable_interrupts();
    tx_q.enable_interrupts();
    ctrl_q.enable_interrupts();
    VDBG("Enabled interrupts on all queues");
    this->setup_complete(true);
    rx_q.kick();
}

void VirtioVsock::handle_rx() {
    VDBG("Event occured on RX queue");
    VDBG("RX completed buffers: %u", rx_q.new_incoming());
    Virtqueue::Token token = rx_q.dequeue();

    if (token.size() < sizeof(net::vsock::virtio_vsock_hdr)) {
        VDBG("RX buffer too small (%zu bytes)", token.size());
        // TODO: What? just ignore or send reset?
        return;
    }

    net::vsock::virtio_vsock_hdr wire_hdr;
    std::memcpy(&wire_hdr, token.data(), sizeof(wire_hdr));
    net::vsock::Packet::Header hdr{wire_hdr};

    if (token.size() < sizeof(net::vsock::virtio_vsock_hdr) + hdr.len()) {
        VDBG("RX buffer too small for payload (%zu bytes)", token.size());
        // TODO: Handle if packet is in different buffer
        return;
    }

    const auto* payload_begin = token.data() + sizeof(wire_hdr);
    auto packet = std::make_unique<net::vsock::Packet>(
        hdr,
        std::vector<uint8_t>(payload_begin, payload_begin + hdr.len())
    );
    VDBG("Delivering packet to transport layer: %u bytes, op %u", packet->header().len(), packet->header().op());
    deliver(std::move(packet));
    // TODO: Dequeue, deliver actual packets, and replenish receive buffers.
}

void VirtioVsock::handle_tx() {
    VDBG("[VirtioVsock] Event occured on TX queue");
    notify_transmit_available(); // Calls the registered callback in transport
    VDBG("[VirtioVsock] Notified transport layer of available transmit space");
}

void VirtioVsock::handle_event() {
    VDBG("[VirtioVsock] Event occured on CTRL queue");

    // Temp: Should read from the ctrl_q:
    struct virtio_vsock_event e = {0};

    struct virtio_vsock_event *event = &e;
    VDBG("[VirtioVsock] Handling event with id %d (%s)", event->id, 
            event->id == VIRTIO_VSOCK_EVENT_TRANSPORT_RESET ? "RESET" : "UNKNOWN");
    
    if (event->id == VIRTIO_VSOCK_EVENT_TRANSPORT_RESET) {
        reset();
    }
}

/**
 * The guest_cid configuration field MUST be fetched to determine the current CID when a VIRTIO_VSOCK_EVENT_TRANSPORT_RESET event is received.
 * Existing connections MUST be shut down when a VIRTIO_VSOCK_EVENT_TRANSPORT_RESET event is received.
 * Listen connections MUST remain operational with the current CID when a VIRTIO_VSOCK_EVENT_TRANSPORT_RESET event is received.
 */
void VirtioVsock::reset() {
    VirtioPci::reset(); // Maybe do this, maybe the VirtioPci does it itself already
    // TODO: Refetch guest_cid in config
    // TODO: Reset connections and change cid for listeners
}

bool VirtioVsock::try_transmit(Packet_ptr& packet) {
    return false;
}



/**
 * Discover and initialize the Virtio-vsock PCI device.
 * Negotiate Virtio features.
 * Read and track the guest CID from device configuration.
 * Create and manage RX, TX, and event virtqueues.
 * Allocate and replenish receive buffers.
 */

__attribute__((constructor))
void autoreg_virtiovsock() {
    hw::PCI_manager::register_vsock(PCI::VENDOR_VIRTIO, 0x1053 , &VirtioVsock::new_instance);
}
