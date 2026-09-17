/**
 * From virtio standard v1.4 
 * https://docs.oasis-open.org/virtio/virtio/v1.4/cs01/virtio-v1.4-cs01.html
 */

#include <cstdint>

#pragma once

namespace net::vsock {

/** 
 * 5.10.3. Feature bits
 * 
 * The driver SHOULD accept the VIRTIO_VSOCK_F_NO_IMPLIED_STREAM feature if offered by the device.
 * If no feature bit has been negotiated, the driver SHOULD act as if VIRTIO_VSOCK_F_STREAM has
 * been negotiated. If VIRTIO_VSOCK_F_SEQPACKET has been negotiated, but not 
 * VIRTIO_VSOCK_F_NO_IMPLIED_STREAM, the driver MAY act as if VIRTIO_VSOCK_F_STREAM has also 
 * been negotiated.
*/
#define VIRTIO_VSOCK_F_STREAM            0
#define VIRTIO_VSOCK_F_SEQPACKET         1
#define VIRTIO_VSOCK_F_NO_IMPLIED_STREAM 2

/** 5.10.4 Reserved CIDs. The upper 32 bits of the CID are reserved and zeroed */
#define VIRTIO_VSOCK_CID_HYPERVISOR     0
#define VIRTIO_VSOCK_CID_RESERVED       1
#define VIRTIO_VSOCK_CID_HOST           2
#define VIRTIO_VSOCK_CID_ANY            0xffffffffU

/**
 * Packed header to be placed before the payload (5.10.6)
 */
struct virtio_vsock_hdr { 
    uint64_t src_cid;
    uint64_t dst_cid;
    uint32_t src_port; 
    uint32_t dst_port; 
    uint32_t len;       // Size of payload in bytes, buffer may be longer
    uint16_t type; 
    uint16_t op;        // Contains ONE of the operation constants defined below
    uint32_t flags; 
    uint32_t buf_alloc; 
    uint32_t fwd_cnt; 
}__attribute__((packed));

/**
 * Operation constants used for connection and buffer space management (5.10.6)
 */
#define VIRTIO_VSOCK_OP_INVALID        0 
#define VIRTIO_VSOCK_OP_REQUEST        1 /* Connect operations */ 
#define VIRTIO_VSOCK_OP_RESPONSE       2 
#define VIRTIO_VSOCK_OP_RST            3 
#define VIRTIO_VSOCK_OP_SHUTDOWN       4 
#define VIRTIO_VSOCK_OP_RW             5 /* To send payload */ 
#define VIRTIO_VSOCK_OP_CREDIT_UPDATE  6 /* Tell the peer our credit info */
#define VIRTIO_VSOCK_OP_CREDIT_REQUEST 7 /* Request the peer to send the credit info to us */ 

/**
 * Stream sockets provide in-order, guaranteed, connection-oriented delivery without 
 * message boundaries. Seqpacket sockets provide in-order, guaranteed, connection-oriented 
 * delivery with message and record boundaries. (5.10.6.2)
 */
#define VIRTIO_VSOCK_TYPE_STREAM    1 
#define VIRTIO_VSOCK_TYPE_SEQPACKET 2

/**
 * buf_alloc and fwd_cnt are used for buffer space management of stream sockets. 
 * The guest and the device publish how much buffer space is available per socket. 
 * Only payload bytes are counted and header bytes are not included. 
 * This facilitates flow control so data is never dropped.
 */

/**
 * Calculates the amount of free receive buffer space. Counts only payload bytes, not header bytes.
 * Facilitates flow control so data is never dropped. 
 * If there is insufficient buffer space, the sender waits until virtqueue buffers are returned 
 * and checks buf_alloc and fwd_cnt again. Sending the VIRTIO_VSOCK_OP_CREDIT_REQUEST packet 
 * queries how much buffer space is available. The reply to this query is a 
 * VIRTIO_VSOCK_OP_CREDIT_UPDATE packet. It is also valid to send a VIRTIO_VSOCK_OP_CREDIT_UPDATE 
 * packet without previously receiving a VIRTIO_VSOCK_OP_CREDIT_REQUEST packet. 
 * This allows communicating updates any time a change in buffer space occurs. (5.10.6.3)
 * 
 * @param buf_alloc: is the total receive buffer space, in bytes, for this socket. This 
 * includes both free and in-use buffers.
 * @param fwd_cnt: The free-running bytes received counter.
 * @param tx_cnt: Sender's free-running bytes transmitted counter.
 */
inline uint32_t calculate_peer_free(uint32_t peer_buf_alloc, uint32_t tx_cnt, uint32_t peer_fwd_cnt) {
    return peer_buf_alloc - (tx_cnt - peer_fwd_cnt);
}

struct virtio_vsock_pkt { 
    struct virtio_vsock_hdr hdr; 
    uint8_t data[]; 
};


/** TODO: 5.10.6.5/6 Stream and seqpacket */


/**
 * 5.10.6.7 Device Events
 * 
 * The VIRTIO_VSOCK_EVENT_TRANSPORT_RESET event indicates that communication has been interrupted.
 * This usually occurs if the guest has been physically migrated. The driver shuts down established 
 * connections and the guest_cid configuration field is fetched again. Existing listen sockets 
 * remain but their CID is updated to reflect the current guest_cid.
 */
#define VIRTIO_VSOCK_EVENT_TRANSPORT_RESET 0 
 
struct virtio_vsock_event { 
    uint32_t id; 
};

/** ---- Custom implementation specific constants ---- */



static bool cid_is_valid(uint64_t cid) {
    return cid > VIRTIO_VSOCK_CID_HOST && cid < VIRTIO_VSOCK_CID_ANY;
}

class Flow {
public:
    uint32_t last_fwd_cnt;
    uint32_t peer_buf_alloc;

    uint32_t fwd_cnt;
    uint32_t tx_cnt;
    uint32_t rx_bytes;
    uint32_t buf_alloc;
    uint32_t buf_used;
};

}
