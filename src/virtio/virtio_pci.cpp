#include <virtio/virtio_pci.hpp>
#include <virtio/virtqueue.hpp>
#include <kernel/events.hpp>
#include <kernel/memmap.hpp>
#include <os.hpp>
#include <kernel.hpp>
#include <info>
#include <assert.h>
#include <cstddef>
#include <debug>

VirtioPci::VirtioPci(hw::PCI_Device& dev)
  : _pcidev(dev), _virtio_device_id(dev.product_id())
{
    INFO("VirtioPci", "Attaching to PCI addr 0x%x", dev.pci_addr());

    assert (dev.vendor_id() == PCI::VENDOR_VIRTIO && "Must be a Virtio device");
    CHECK(true, "Vendor ID is VIRTIO");

    bool _STD_ID = _virtio_device_id >= 0x1040 and _virtio_device_id < 0x107f;
    bool _LEGACY_ID = dev.product_id() >= 0x1000 and dev.product_id() <= 0x103f;

    CHECK(_STD_ID or _LEGACY_ID, "Device ID 0x%x is in a valid range (%s)",
            _pcidev.product_id(),
            _STD_ID ? "VirtIO MODERN" : (_LEGACY_ID ? "VirtIO LEGACY" : "INVALID"));
    assert(_STD_ID or _LEGACY_ID);
    assert(_STD_ID and dev.rev_id() > 0);

    // find and store capabilities
    _pcidev.parse_capabilities();
    // find BARs etc.
    _pcidev.probe_resources();

    // initialize MSI-X if available
    if (dev.msix_cap()) {
        dev.init_msix();
        uint8_t msix_vectors = dev.get_msix_vectors();
        if (msix_vectors) {
            INFO2("[x] Device has %u MSI-X vectors", msix_vectors);
            this->current_cpu = SMP::cpu_id();

            // setup all the MSI-X vectors
            for (int i = 0; i < msix_vectors; i++)
            {
                auto irq = Events::get().subscribe(nullptr);
                dev.setup_msix_vector(current_cpu, IRQ_BASE + irq);
                // store IRQ for later
                this->irqs.push_back(irq);
            }
        }
        else {
            INFO2("[ ] No MSI-X vectors");
        }
    } else {
        INFO2("[ ] No MSI-X vectors");
    }

    assert(has_msix() && "Device has not enable MSI-X");
    CHECK(true, "MSI-X is enabled");

    const bool common_mapped = map_common_cfg();
    CHECKSERT(common_mapped, "Config read and stored");

    const bool notify_mapped = map_notify_cfg();
    CHECKSERT(notify_mapped, "Notification registers mapped");

    // TODO: 3.1.2 About legacy driver initialization


    /** Initializing the device by following the sequence specified in §3.1.1 */

    /** 1. Reset the device. */
    INFO2("Resetting device");
    reset();

    /** 2. Set the ACKNOWLEDGE status bit: the guest OS has noticed the device. */
    INFO2("Setting ack-bit");
    _common_cfg->device_status |= VIRTIO_CONFIG_S_ACKNOWLEDGE;

    /** 3. Set the DRIVER status bit: the guest OS knows how to drive the device. */
    INFO2("Setting driver-status-bit");
    _common_cfg->device_status |= VIRTIO_CONFIG_S_DRIVER;
    


    /** The remaining steps are device specific and therefore needs to be done in a subclass */

    /**
     * 4. Read device feature bits, and write the subset of feature bits understood by the OS and driver to the device. 
     * During this step the driver MAY read (but MUST NOT write) the device-specific configuration fields to check '
     * that it can support the device before accepting it.
     */ 

    /** 5. Set the FEATURES_OK status bit. The driver MUST NOT accept new feature bits after this step. */


    /** 
     * 6. Re-read device status to ensure the FEATURES_OK bit is still set: otherwise, the device does not support our 
     * subset of features and the device is unusable.
     */

    /**
     * 7. Perform device-specific setup, including discovery of virtqueues for the device, optional per-bus setup, 
     * reading and possibly writing the device’s virtio configuration space, and population of virtqueues.
     */

    /** 
     * 8. Set the DRIVER_OK status bit. At this point the device is “live”. If any of these steps go irrecoverably wrong, 
     * the driver SHOULD set the FAILED status bit to indicate that it has given up on the device (it can reset the device 
     * later to restart if desired). The driver MUST NOT continue initialization in that case.
     */
}

void VirtioPci::probe_features() {
    for (int i = 0; i < 4; i++) {
        common_cfg()->device_feature_select = i;
        _features[i] = common_cfg()->device_feature;
    }
}

bool VirtioPci::negotiate_features(uint32_t *wanted_features) {
    for (int i = 0; i < 4; i++) {
        common_cfg()->driver_feature_select = i;
        common_cfg()->driver_feature = wanted_features[i];
    }
    common_cfg()->device_status |= VIRTIO_CONFIG_S_FEATURES_OK;
    if (!(common_cfg()->device_status & VIRTIO_CONFIG_S_FEATURES_OK)) {
        return false;
    }
    probe_features();
    return true;
}

/**
 * Parses the list of PCI capabilities for the common_cfg
 */
bool VirtioPci::map_common_cfg() {
    _common_cfg =
        reinterpret_cast<volatile virtio_pci_common_cfg*>(
            map_capability(VIRTIO_PCI_CAP_COMMON_CFG));
    return _common_cfg != nullptr;
}

bool VirtioPci::map_notify_cfg() {
    _notify_multiplier = 0;
    _notify_length = 0;
    _notify_base =
        reinterpret_cast<volatile uint8_t*>(
            map_capability(VIRTIO_PCI_CAP_NOTIFY_CFG));
    return _notify_base != nullptr;
}

void* VirtioPci::map_capability(const uint32_t type) {
    uint8_t cap_pos =
        static_cast<uint8_t>(_pcidev.read16(PCI_CAPABILITY_REG) & 0xff);
    bool visited[256] = {};

    while (cap_pos != 0) {
        if ((cap_pos & 0x3) != 0 || cap_pos > 0xf0 || visited[cap_pos]) {
            return nullptr;
        }
        visited[cap_pos] = true;

        const uint32_t header = _pcidev.read32(cap_pos);
        const uint8_t cap_vndr = header & 0xff;
        const uint8_t cap_next = (header >> 8) & 0xff;
        const uint8_t cap_len  = (header >> 16) & 0xff;
        const uint8_t cfg_type = (header >> 24) & 0xff;

        if (cap_vndr == PCI_CAP_ID_VNDR &&
            cfg_type == type)
        {
            if (cap_len < sizeof(virtio_pci_cap)) {
                return nullptr;
            }

            const uint32_t bar_info = _pcidev.read32(cap_pos + 4);
            const uint8_t bar_index = bar_info & 0xff;
            const uint32_t offset = _pcidev.read32(cap_pos + 8);
            const uint32_t length = _pcidev.read32(cap_pos + 12);

            if (!_pcidev.validate_bar(bar_index))
                return nullptr;

            const auto& bar = _pcidev.get_bar(bar_index);

            if (bar.start == 0 || offset > bar.len || length > bar.len - offset) {
                return nullptr;
            }

            if (type == VIRTIO_PCI_CAP_NOTIFY_CFG) {
                // The multiplier is in PCI config space, not in the BAR.
                if (cap_len < sizeof(virtio_pci_notify_cap) || cap_pos > 0xec ||
                    length < sizeof(uint16_t) || (offset & 1))
                    return nullptr;
                const uint32_t multiplier = _pcidev.read32(cap_pos + 16);
                if (multiplier & 1)
                    return nullptr;
                _notify_multiplier = multiplier;
                _notify_length = length;
            }

            return (void*)(bar.start + offset);
        }
        cap_pos = cap_next;
    }
    return nullptr;
}

bool VirtioPci::setup_queue(uint16_t index, Virtqueue& queue, uint16_t msix_vector)
{
    auto* cfg = common_cfg();
    if (cfg == nullptr)
        return false;

    cfg->queue_select = index;
    const uint16_t maximum_size = cfg->queue_size;
    if (maximum_size == 0 || queue.size() > maximum_size)
        return false;

    cfg->queue_size = queue.size();

    cfg->queue_desc = reinterpret_cast<uintptr_t>(queue.queue_desc());

    cfg->queue_driver = reinterpret_cast<uintptr_t>(queue.queue_avail());

    cfg->queue_device = reinterpret_cast<uintptr_t>(queue.queue_used());

    if (has_msix()) {
        cfg->queue_msix_vector = msix_vector;

        if (cfg->queue_msix_vector == 0xffff)
            return false;
    }

    const uint64_t offset =
        uint64_t(cfg->queue_notify_off) * _notify_multiplier;

    if (!_notify_base || offset + sizeof(uint16_t) > _notify_length)
        return false;

    queue.set_notify_addr(
        reinterpret_cast<volatile uint16_t*>(_notify_base + offset));

    cfg->queue_enable = 1;
    return cfg->queue_enable == 1;
}

void VirtioPci::setup_complete(bool ok) {
    if (ok) {
        common_cfg()->device_status |= VIRTIO_CONFIG_S_DRIVER_OK;
    }
    CHECK(ok, "Device setup comleted");
}

void VirtioPci::reset() {
    common_cfg()->device_status = 0;
}

void VirtioPci::suspend()
{
    /*
    3.4
    If VIRTIO_F_SUSPEND is negotiated, the driver is eligible to suspend the device by setting the SUSPEND bit in device status to 1, and the device sets the DRIVER_OK bit to 0 once it has been suspended.
    If the device has been suspended, the driver can resume the device running by setting the DRIVER_OK bit in device status to 1, and the device sets the SUSPEND bit to 0 once it resumes running.
    */

    /*
    3.4.1
    The driver SHOULD NOT set SUSPEND bit if DRIVER_OK is not set or VIRTIO_F_SUSPEND is not negotiated.

    Once the driver sets SUSPEND bit in device status to 1:

    The driver MUST verify whether the device has been suspended by re-reading device status, examining whether the SUSPEND bit is set to 1 and the DRIVER_OK bit is set to 0.

    The driver MUST NOT make any more buffers available to the device.

    The driver MUST NOT send notifications for any virtqueues.

    The driver MUST NOT make any changes to Device Configuration Space except for device status if it is part of the Configuration Space.

    When the device has been suspended, once the driver sets DRIVER_OK bit in device status to 1, the driver MUST wait for the SUSPEND bit in device status to turn 0 and the DRIVER_OK bit in device_status to turn 1 before any normal operations."
    */

    /*
    3.4.2
    The device MUST ignore any operations on the SUSPEND bit from the driver if the device has not been completely initialized by the procedures in 3.1

    The device SHOULD ignore any write access to its Configuration Space while suspended, except for device status if it is part of the Configuration Space.

    A device MUST NOT send any notifications for any virtqueues, access any virtqueues, or modify any fields in its Configuration Space while suspended.

    If changes occur in the Configuration Space during suspended period, the device MUST NOT send any configuration change notifications. Instead, the device MUST send the notification when it resumes running.

    If the driver sets the SUSPEND bit in device status to 1, the device MUST either suspend itself or set the DEVICE_NEEDS_RESET bit in device status to 1 when it fails to suspend.

    If the device has been suspended and the driver resumes the device running by setting the DRIVER_OK bit in device status to 1, the device MUST either resume normal operation or set the DEVICE_NEEDS_RESET bit in device status to 1 when it fails to resume.

    When the driver sets the SUSPEND bit to 1, the device SHOULD perform the following actions before presenting the SUSPEND bit as 1 and DRIVER_OK bit as 0 in the device status:

    Stop consuming more buffers of any virtqueues.

    Wait until all buffers that are being processed have been used.

    Send used buffer notifications to the driver.
    */
}

void VirtioPci::resume() {
    
}

void VirtioPci::move_to_this_cpu() {
    if (has_msix()){
        // unsubscribe IRQs on old CPU
        for (size_t i = 0; i < irqs.size(); i++) {
            auto& oldman = Events::get(this->current_cpu);
            oldman.unsubscribe(this->irqs[i]);
        }
        // resubscribe on the new CPU
        this->current_cpu = SMP::cpu_id();
        for (size_t i = 0; i < irqs.size(); i++) {
            this->irqs[i] = Events::get().subscribe(nullptr);
            _pcidev.rebalance_msix_vector(i, current_cpu, IRQ_BASE + this->irqs[i]);
        }
    }
}

void VirtioPci::default_irq_handler() {
    // TODO: is this a function?
}

/**
4.1.4.1 Driver requirements: Virtio Structure PCI Capabilities

The driver MUST ignore any vendor-specific capability structure which has a reserved cfg_type value.

The driver SHOULD use the first instance of each virtio structure type they can support.

The driver MUST accept a cap_len value which is larger than specified here.

The driver MUST ignore any vendor-specific capability structure which has a reserved bar value.

The drivers SHOULD only map part of configuration structure large enough for device operation. The drivers MUST handle an unexpectedly large length, but MAY check that length is large enough for device operation.

The driver MUST NOT write into any field of the capability structure, with the exception of those with cap_type VIRTIO_PCI_CAP_PCI_CFG as detailed in 4.1.4.9.2.
 */


/*
    2.6.1 Virtqueue Reset
    When VIRTIO_F_RING_RESET is negotiated, the driver can reset a virtqueue individually. The way to reset the virtqueue is transport specific.

    Virtqueue reset is divided into two parts. The driver first resets a queue and can afterwards optionally re-enable it.

    2.6.1.1 Virtqueue Reset
    2.6.1.1.1 Device Requirements: Virtqueue Reset
    After a queue has been reset by the driver, the device MUST NOT execute any requests from that virtqueue, or notify the driver for it.
    The device MUST reset any state of a virtqueue to the default state, including the available state and the used state.

    2.6.1.1.2 Driver Requirements: Virtqueue Reset
    After the driver tells the device to reset a queue, the driver MUST verify that the queue has actually been reset.
    After the queue has been successfully reset, the driver MAY release any resource associated with that virtqueue.

    2.6.1.2 Virtqueue Re-enable
    This process is the same as the initialization process of a single queue during the initialization of the entire device.

    2.6.1.2.1 Device Requirements: Virtqueue Re-enable
    The device MUST observe any queue configuration that may have been changed by the driver, like the maximum queue size.
    2.6.1.2.2 Driver Requirements: Virtqueue Re-enable
    When re-enabling a queue, the driver MUST configure the queue resources as during initial virtqueue discovery, but optionally with different parameters.

 */
