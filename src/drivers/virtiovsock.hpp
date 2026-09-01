#ifndef VIRTIO_VSOCK_HPP
#define VIRTIO_VSOCK_HPP

#include <cstdint>
#include <virtio/virtio_pci.hpp>
#include <virtio/virtqueue.hpp>
#include <hw/pci_device.hpp>
#include <hw/vsock.hpp>

/**
 * From virtio standard v1.4 
 * https://docs.oasis-open.org/virtio/virtio/v1.4/cs01/virtio-v1.4-cs01.html
 */

#define VIRTIO_VSOCK_F_STREAM 0
#define VIRTIO_VSOCK_F_SEQPACKET 1
#define VIRTIO_VSOCK_F_NO_IMPLIED_STREAM 2

#define VIRTIO_VSOCK_CID_HOST 2
#define VIRTIO_VSOCK_CID_MAX 0xffffffff - 1

/**
 * Driver should except NO_IMPLIED_STREAM if offered by device 
 * If no feature bit negotiated, the driver acts a if STREAM is negotiated
 * If SEQPACKET is negotiated, driver may assume STREAM is also (5.10.3.1)
*/

// Virtio-vsock device driver
class VirtioVsock : public hw::Vsock, private VirtioPci {

    public:
        explicit VirtioVsock(hw::PCI_Device&);

        static std::unique_ptr<hw::Vsock> new_instance(hw::PCI_Device& d) {
            return std::make_unique<VirtioVsock>(d);
        }

        int enqueue_tx(struct virtio_vsock_packet *pkt);

        void receive();

        uint32_t cid() const noexcept {
            return _cid;
        }

        uint16_t queue_size() {
            return 128;
        }

        struct virtio_vsock_hdr { 
            uint64_t src_cid; // Upper 32 bits reserved and zeroed
            uint64_t dst_cid; // Upper 32 bits reserved and zeroed
            uint32_t src_port; 
            uint32_t dst_port; 
            uint32_t len; 
            uint16_t type; 
            uint16_t op; 
            uint32_t flags; 
            uint32_t buf_alloc; 
            uint32_t fwd_cnt; 
        }__attribute__((packed));

        struct virtio_vsock_pkt { 
            struct virtio_vsock_hdr hdr; 
            uint8_t data[]; 
        };

        class Packet {
        public:
            struct virtio_vsock_hdr* header() {
                return &_pkt->hdr;
            }

            uint8_t* data() noexcept {
                return _pkt->data;
            }

        private:
            struct virtio_vsock_pkt *_pkt;
        };

    private:
    
        uint32_t _cid = 0;

        struct virtio_vsock_config { 
            uint64_t guest_cid; 
        };

        Virtqueue rx_q;
        Virtqueue tx_q;
        Virtqueue ctrl_q;
};

/**
 * Internal transport interface for the socket API to use
 */
class VsockTransport {
public:
    // Internal transport interf
};

/**
 * buf_alloc and fwd_cnt are used for buffer space management of stream sockets. 
 * The guest and the device publish how much buffer space is available per socket. 
 * Only payload bytes are counted and header bytes are not included. 
 * This facilitates flow control so data is never dropped.
 */

/**
 * buf_alloc is the total receive buffer space, in bytes, for this socket. This 
 * includes both free and in-use buffers. fwd_cnt is the free-running bytes received 
 * counter. The sender calculates the amount of free receive buffer space as follows:
 * tx_cnt: Sender's free-running bytes transmitted counter
 */
uint32_t calculate_peer_free(uint32_t peer_buf_alloc, uint32_t tx_cnt, uint32_t peer_fwd_cnt) {
    return peer_fwd_cnt - (tx_cnt - peer_fwd_cnt);
}

/**
 * If there is insufficient buffer space, the sender waits until virtqueue buffers are returned 
 * and checks buf_alloc and fwd_cnt again. Sending the VIRTIO_VSOCK_OP_CREDIT_REQUEST packet 
 * queries how much buffer space is available. The reply to this query is a 
 * VIRTIO_VSOCK_OP_CREDIT_UPDATE packet. It is also valid to send a VIRTIO_VSOCK_OP_CREDIT_UPDATE 
 * packet without previously receiving a VIRTIO_VSOCK_OP_CREDIT_REQUEST packet. 
 * This allows communicating updates any time a change in buffer space occurs. (5.10.6.3)
 */

/**
 * Operation constants used for connection and buffer space management (5.10.6)
 */
#define VIRTIO_VSOCK_OP_INVALID        0 
/* Connect operations */ 
#define VIRTIO_VSOCK_OP_REQUEST        1 
#define VIRTIO_VSOCK_OP_RESPONSE       2 
#define VIRTIO_VSOCK_OP_RST            3 
#define VIRTIO_VSOCK_OP_SHUTDOWN       4 
/* To send payload */ 
#define VIRTIO_VSOCK_OP_RW             5 
/* Tell the peer our credit info */ 
#define VIRTIO_VSOCK_OP_CREDIT_UPDATE  6 
/* Request the peer to send the credit info to us */ 
#define VIRTIO_VSOCK_OP_CREDIT_REQUEST 7

/**
 * VIRTIO_VSOCK_OP_RW data packets MUST only be transmitted when the peer has sufficient 
 * free buffer space for the payload. All packets associated with a stream flow MUST 
 * contain valid information in buf_alloc and fwd_cnt fields. (5.10.6.3.1)
 */

/**
 * Stream sockets provide in-order, guaranteed, connection-oriented delivery without 
 * message boundaries. Seqpacket sockets provide in-order, guaranteed, connection-oriented 
 * delivery with message and record boundaries. (5.10.6.2)
 */
#define VIRTIO_VSOCK_TYPE_STREAM    1 
#define VIRTIO_VSOCK_TYPE_SEQPACKET 2


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