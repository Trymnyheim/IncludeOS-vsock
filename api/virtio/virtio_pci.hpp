// This file is a part of the IncludeOS unikernel - www.includeos.org
//
// Copyright 2015 Oslo and Akershus University College of Applied Sciences
// and Alfred Bratterud
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once
#ifndef VIRTIO_VIRTIO_PCI_HPP
#define VIRTIO_VIRTIO_PCI_HPP

#include <hw/pci_device.hpp>
#include <limits.h>
#include <stdint.h>
#include <vector>

#if ! defined(PAGE_SIZE)
#error "PAGE_SIZE not defined. Expected it to be defined by musl's limits.h"
#endif

/* 2.1 Device Status field */
#define VIRTIO_CONFIG_S_ACKNOWLEDGE         1
#define VIRTIO_CONFIG_S_DRIVER              2
#define VIRTIO_CONFIG_S_DRIVER_OK           4
#define VIRTIO_CONFIG_S_FEATURES_OK         8
#define VIRTIO_CONFIG_S_SUSPEND             16
#define VIRTIO_CONFIG_S_DEVICE_NEEDS_RESET  64
#define VIRTIO_CONFIG_S_FAILED              128

/* 4.1.4 Virtio Structure PCI Capabilities */
/* Common configuration */ 
#define VIRTIO_PCI_CAP_COMMON_CFG        1 
/* Notifications */ 
#define VIRTIO_PCI_CAP_NOTIFY_CFG        2 
/* ISR Status */ 
#define VIRTIO_PCI_CAP_ISR_CFG           3 
/* Device specific configuration */ 
#define VIRTIO_PCI_CAP_DEVICE_CFG        4 
/* PCI configuration access */ 
#define VIRTIO_PCI_CAP_PCI_CFG           5 
/* Shared memory region */ 
#define VIRTIO_PCI_CAP_SHARED_MEMORY_CFG 8 
/* Vendor-specific data */ 
#define VIRTIO_PCI_CAP_VENDOR_CFG        9

class Virtqueue;

class VirtioPci {

public:
    /** Virtio device types. Virtio Std. §4.1.2.1 */
    enum virtiotype_t {
        NIC = 0x1000,
        BLOCK = 0x1001,
        BALLOON = 0x1002,
        CONSOLE = 0x1003,
        SCSI_HOST = 0x1004,
        ENTROPY = 0x1005,
        T9P = 0x1009
    };

    /** @note Using typedefs in order to keep the standard notation. */
    using le64 =  uint64_t;
    using le32 = uint32_t;
    using le16 = uint16_t;
    using u16 = uint16_t;
    using u8 = uint8_t;

    struct virtio_pci_cap { 
        u8 cap_vndr;    /* Generic PCI field: PCI_CAP_ID_VNDR */
        u8 cap_next;    /* Generic PCI field: next ptr. */ 
        u8 cap_len;     /* Generic PCI field: capability length */ 
        u8 cfg_type;    /* Identifies the structure. */ 
        u8 bar;         /* Where to find it. */ 
        u8 id;          /* Multiple capabilities of the same type */ 
        u8 padding[2];  /* Pad to full dword. */ 
        le32 offset;    /* Offset within bar. */ 
        le32 length;    /* Length of the structure, in bytes. */ 
    };

    /** 4.1.4.3 Common configuration structure layout */
    struct virtio_pci_common_cfg { 
        /* About the whole device. */ 
        le32 device_feature_select;     /* read-write */ 
        le32 device_feature;            /* read-only for driver */ 
        le32 driver_feature_select;     /* read-write */ 
        le32 driver_feature;            /* read-write */ 
        le16 config_msix_vector;        /* read-write */ 
        le16 num_queues;                /* read-only for driver */ 
        u8 device_status;               /* read-write */ 
        u8 config_generation;           /* read-only for driver */ 
 
        /* About a specific virtqueue. */ 
        le16 queue_select;              /* read-write */ 
        le16 queue_size;                /* read-write */ 
        le16 queue_msix_vector;         /* read-write */ 
        le16 queue_enable;              /* read-write */ 
        le16 queue_notify_off;          /* read-only for driver */ 
        le64 queue_desc;                /* read-write */ 
        le64 queue_driver;              /* read-write */ 
        le64 queue_device;              /* read-write */ 
        le16 queue_notify_data;         /* read-only for driver */ 
        le16 queue_notif_config_data;   /* read-only for driver */ 
        le16 queue_reset;               /* read-write */ 

        /* About the administration virtqueue. */ 
        le16 admin_queue_index;         /* read-only for driver */ 
        le16 admin_queue_num;         /* read-only for driver */ 
    };

    /** 4.1.4.4 Notification structure layout */
    struct virtio_pci_notify_cap { 
        struct virtio_pci_cap cap; 
        le32 notify_off_multiplier; /* Multiplier for queue_notify_off. */ 
    };

    /** Probe PCI device for features */
    void probe_features();

    /** Negotiate supported features with host */
    bool negotiate_features(uint32_t *features);

    /** Assign a queue descriptor to a PCI queue index */
    bool setup_queue(uint16_t index, const Virtqueue& queue, uint16_t msix_vector);

    bool map_common_cfg();

    bool map_notify_cfg();

    void* map_capability(const uint32_t cfg_type);

    /** Tell Virtio device if we're OK or not. Virtio Std. § 3.1.1,step 8*/
    void setup_complete(bool ok);

    /** Reset the virtio device */
    void reset();

    /** Suspend the device */
    void suspend();

    /** Resume device after suspend */
    void resume();

    /** Get locally stored features */
    uint32_t* features(){ return _features; };

    volatile virtio_pci_common_cfg* common_cfg() noexcept {
        return _common_cfg;
    }

    volatile virtio_pci_notify_cap* notify_cfg() noexcept {
        return _notify_cfg;
    }

    // returns true if MSI-X is supported
    bool has_msix() const noexcept { return _pcidev.has_msix(); }

    // returns non-zero if MSI-x is supported
    uint8_t get_msix_vectors() const noexcept {
        return _pcidev.get_msix_vectors();
    }

    uint8_t get_irq(int vector) const noexcept {
        return irqs[vector];
    }

    void move_to_this_cpu();

    /** Virtio device constructor.
     * Should conform to Virtio std. §3.1.1, steps 1-6
     * (Step 7 is "Device specific" which a subclass will handle)
    */
    VirtioPci(hw::PCI_Device&);

protected:
    void deactivate_msix() {
        _pcidev.deactivate_msix();
    }

private:
    hw::PCI_Device& _pcidev;
    volatile virtio_pci_common_cfg* _common_cfg = nullptr;
    volatile virtio_pci_notify_cap* _notify_cfg = nullptr;

    uint32_t _features[4] = {0};
    uint16_t _virtio_device_id = 0;

    void default_irq_handler();

    uint8_t current_cpu;
    std::vector<uint8_t> irqs;
};

#endif
