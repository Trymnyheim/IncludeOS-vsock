#ifndef VIRTIO_VSOCK_HPP
#define VIRTIO_VSOCK_HPP

#include <cstdint>
#include <virtio/virtio_pci.hpp>
#include <virtio/virtqueue.hpp>
#include <hw/pci_device.hpp>
#include <hw/vsock.hpp>
#include <net/vsock/packet.hpp>

/** 5.10.3. Feature bits */
#define VIRTIO_VSOCK_F_STREAM 0
#define VIRTIO_VSOCK_F_SEQPACKET 1
#define VIRTIO_VSOCK_F_NO_IMPLIED_STREAM 2

// Virtio-vsock device driver
class VirtioVsock : public hw::Vsock, private VirtioPci {
public:
    explicit VirtioVsock(hw::PCI_Device&);

    static std::unique_ptr<hw::Vsock> new_instance(hw::PCI_Device& d) {
        return std::make_unique<VirtioVsock>(d);
    }

    void handle_rx();

    void handle_tx();

    void handle_event();

    void reset();

    bool try_transmit(Packet_ptr& packet);

    uint32_t cid() const noexcept {
        return _cid;
    }

    uint16_t queue_size(uint16_t index) {
        common_cfg()->queue_select = index;
        return common_cfg()->queue_size;
    }

private:

    uint32_t _cid = 0;

    struct virtio_vsock_config { 
        uint64_t guest_cid; 
    };

    Virtqueue rx_q;
    Virtqueue tx_q;
    Virtqueue ctrl_q;

    // Better way to do this
    std::array<
        std::array<uint8_t, 4096>,
        32
    > rx_buffers_;
};




/**
 * The driver enqueues outgoing packets to the tx virtqueue and incoming packet receive 
 * buffers on the rx virtqueue. Packets are of the following form:
 */

/**
 * Virtqueue buffers for outgoing packets are read-only. 
 * Virtqueue buffers for incoming packets are write-only.
 */






// Vsock behaviour notes:

/** 
 * len is the size of the payload, in bytes. However, the driver may provide 
 * buffer(s) for the payload that have a total size longer than len, in which 
 * case only the first len bytes will be used for the actual data. (5.10.6)
 */

/**
 * Flows are identified by a (source, destination) address tuple. An address 
 * consists of a (cid, port number) tuple. The header fields used for this are
 * src_cid, src_port, dst_cid, and dst_port. (5.10.6.2)
 */

/**
 * The guest_cid configuration field MUST be used as the source CID when sending 
 * outgoing packets. AVIRTIO_VSOCK_OP_RST  reply MUST be sent if a packet is received 
 * with an unknown type value. (5.10.6.4.1)
 */


/**
 * The guest_cid configuration field MUST NOT contain a reserved CID as listed in 5.10.4.
 * A VIRTIO_VSOCK_OP_RST reply MUST be sent if a packet is received with an unknown type 
 * value. (5.10.6.4.2)
 */




// Stream sockets (5.10.6.5):
/**
 * Connections are established by sending a VIRTIO_VSOCK_OP_REQUEST packet. If a listening 
 * socket exists on the destination a VIRTIO_VSOCK_OP_RESPONSE reply is sent and the connection 
 * is established. A VIRTIO_VSOCK_OP_RST reply is sent if a listening socket does not exist on 
 * the destination or the destination has insufficient resources to establish the connection.
 */
/**
 * When a connected socket receives VIRTIO_VSOCK_OP_SHUTDOWN the header flags field bit 
 * VIRTIO_VSOCK_SHUTDOWN_F_RECEIVE (bit 0) set indicates that the peer will not receive any more
 * data and bit VIRTIO_VSOCK_SHUTDOWN_F_SEND (bit 1) set indicates that the peer will not send
 * any more data. These hints are permanent once sent and successive packets with bits clear 
 * do not reset them.
 */
/**
 * The VIRTIO_VSOCK_OP_RST packet aborts the connection process or forcibly disconnects a connected 
 * socket. Clean disconnect is achieved by one or more VIRTIO_VSOCK_OP_SHUTDOWN packets that indicate
 * no more data will be sent and received, followed by a VIRTIO_VSOCK_OP_RST response from the peer.
 * If no VIRTIO_VSOCK_OP_RST response is received within an implementation-specific amount of time, 
 * a VIRTIO_VSOCK_OP_RST packet is sent to forcibly disconnect the socket.
 * The clean disconnect process ensures that neither peer reuses the (source, destination) address 
 * tuple for a new connection while the other peer is still processing the old connection.
 */

#define VIRTIO_VSOCK_SHUTDOWN_F_RECEIVE 0 
#define VIRTIO_VSOCK_SHUTDOWN_F_SEND 1


// Seqpacket sockets (5.10.6.6):
/**
 * Two types of boundaries are supported: message and record boundaries.
 * A message contains data sent in a single operation. A single message can be split into multiple 
 * RW packets. 
 * To provide message boundaries, last RW packet of each message has VIRTIO_VSOCK_SEQ_EOM bit (bit 0) 
 * set in the flags of packet’s header. 
 * Record is any number of subsequent messages, where last message is sent with POSIX MSG_EOR flag set. 
 * Record boundary means that receiver gets MSG_EOR flag set in the corresponding message where sender 
 * set it. To provide record boundaries, last RW packet of each record has 
 * VIRTIO_VSOCK_SEQ_EOR bit (bit 1) set in the flags of packet’s header.
 */

#define VIRTIO_VSOCK_SEQ_EOM (1 << 0) 
#define VIRTIO_VSOCK_SEQ_EOR (1 << 1)

// Device events (5.10.6.7):
/**
 * Certain events are communicated by the device to the driver using the event virtqueue.
 * The VIRTIO_VSOCK_EVENT_TRANSPORT_RESET event indicates that communication has been interrupted. This 
 * usually occurs if the guest has been physically migrated. The driver shuts down established connections 
 * and the guest_cid configuration field is fetched again. Existing listen sockets remain but their CID is 
 * updated to reflect the current guest_cid. (5.10.6.7)
 */

/**
 * Event virtqueue buffers SHOULD be replenished quickly so that no events are missed.
 * The guest_cid configuration field MUST be fetched to determine the current CID when a 
 * VIRTIO_VSOCK_EVENT_TRANSPORT_RESET event is received.
 * Existing connections MUST be shut down when a VIRTIO_VSOCK_EVENT_TRANSPORT_RESET event is received.
 * Listen connections MUST remain operational with the current CID when a VIRTIO_VSOCK_EVENT_TRANSPORT_RESET 
 * event is received. (5.10.6.7.1)
 */

#define VIRTIO_VSOCK_EVENT_TRANSPORT_RESET 0 
 
struct virtio_vsock_event { 
    uint32_t id; 
};

#endif
