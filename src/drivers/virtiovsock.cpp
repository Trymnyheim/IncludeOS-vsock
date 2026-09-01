
#include "virtiovsock.hpp"
#include <hw/pci_manager.hpp>
#include <info>

#define VSOCK_DEBUG

#ifdef VSOCK_DEBUG
#define VDBG(fmt, ...) INFO("VirtioVsock", fmt, ##__VA_ARGS__)
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

    probe_features();
    uint32_t *offered_features = features();

    // TODO: Check and handle offered_features

    uint32_t *wanted_features = offered_features;
    if (!negotiate_features(wanted_features)) {
        setup_complete(false);
        return;
    }

    // TODO: Change features() to take select_value instead
    CHECK(features()[0] & (1U << VIRTIO_VSOCK_F_STREAM), "Stream socket type is supported");
    CHECK(features()[0] & (1U << VIRTIO_VSOCK_F_SEQPACKET), "Seqpacket socket type is supported");
    CHECK(features()[0] & (1U << VIRTIO_VSOCK_F_NO_IMPLIED_STREAM), "Stream socket type is not implied");
    CHECK(features()[0] & (1U << VIRTIO_CONFIG_S_SUSPEND), "Device can be suspended");

    /**
     * 7. Perform device-specific setup, including discovery of virtqueues for the device, optional per-bus setup, 
     * reading and possibly writing the device’s virtio configuration space, and population of virtqueues.
     */

    auto *device_cfg = map_capability(VIRTIO_PCI_CAP_DEVICE_CFG);
    struct virtio_vsock_config *cfg = (virtio_vsock_config*)device_cfg;

    uint32_t cid = cfg->guest_cid;
    bool valid_cid = (cid > VIRTIO_VSOCK_CID_HOST || cid >= VIRTIO_VSOCK_CID_MAX);
    CHECKSERT(valid_cid, "CID (%u) assigned to device", cid);
    _cid = cid;

    /** RX que is 0, TX Queue is 1 - Virtio Std. §5.1.2  */

    uint16_t msix_vector = 0; // How to handle this?

    std::string name = "vsock";

    new (&rx_q) Virtqueue(name + ".rx_q", queue_size(), 0, 0);
    bool success = setup_queue(0, rx_q, msix_vector);
    CHECKSERT(success, "RX queue (%u) assigned (%p) to device",
                rx_q.size(), rx_q.queue_desc());

    new (&tx_q) Virtqueue(name + ".tx_q", queue_size(), 0, 0);
    success = setup_queue(1, tx_q, msix_vector);
    CHECKSERT(success, "TX queue (%u) assigned (%p) to device",
                tx_q.size(), tx_q.queue_desc());

    new (&ctrl_q) Virtqueue(name + ".ctrl_q", queue_size(), 0, 0);
    success = setup_queue(2, ctrl_q, msix_vector);
    CHECKSERT(success, "CTRL queue (%u) assigned (%p) to device",
                ctrl_q.size(), ctrl_q.queue_desc());

    this->setup_complete(true);
}

int enqueue_tx(VirtioVsock::Packet& pkt) {
    VDBG("[VirtioVsock] tx: Transmitting %#zu sized packet \n", pkt.header()->len);
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

