#include "net/vsock/transport.hpp"
#include "net/vsock/socket.hpp"
#include "net/vsock/listener.hpp"

#include <hw/pci_manager.hpp>
#include <hw/vsock.hpp>
#include <hal/machine.hpp>
#include <delegate>
#include <os>

#define VSOCK_TRANSPORT_DEBUG
#ifdef VSOCK_TRANSPORT_DEBUG
#define DEBUG(fmt, ...) printf("[ VSOCK: Transport ] " fmt"\n", ##__VA_ARGS__)
#else
#endif

using namespace net::vsock;

static Transport* _transport = nullptr;

Transport& Transport::getInstance() {
    if (_transport == nullptr) {
        hw::Vsock& dev = os::machine().get<hw::Vsock>(0);
        _transport = new Transport(dev);
    }
    return *_transport;
}

Transport::Transport(hw::Vsock& dev) : _dev(dev) {
    _dev.on_receive([this](Packet_ptr packet) {
        on_receive(std::move(packet));
    });
    // _dev.on_transmit_available();
    // _dev.on_transport_reset();
}

/**
 * @param packet Packet received from the driver
 */
void Transport::on_receive(Packet_ptr packet) {
    DEBUG("Received packet");

    // Check if a port is bound to destination port 
    if (!port_bound(packet->header().destination().port())) {
        DEBUG("Ignoring packet for unbound port %u", 
            packet->header().destination().port());
        return;
    }

    const auto op = packet->header().op();
    if (op == VIRTIO_VSOCK_OP_INVALID || op > VIRTIO_VSOCK_OP_CREDIT_REQUEST) {
        // Ingore invalid operations
        return;
    }
    // Request-packets are forwarded to a Listener
    if (op == VIRTIO_VSOCK_OP_REQUEST) {
        Listener::Listener_ptr listener = nullptr;
        if (auto search = _listeners.find(packet->header().destination().port()); search != _listeners.end()) {
            listener = search->second;
        }
        if (listener == nullptr) {
            // No listener on port, sending reset packet
            DEBUG("Ignoring connect. No listener on port %u", 
                packet->header().destination().port());
            send_reset(packet->header().source());
        }
        listener->handle_connect(packet->header().source());
        return;
    }
    /** All other valid operations are passed to a connection if one exists */
    Connection::Tuple tuple = {packet->header().destination(), packet->header().source()};
    Connection::Connection_ptr conn = nullptr;
    if (auto search = _connections.find(tuple); search != _connections.end()) {
        conn = search->second;
    }

    // If no conn exists, send reset packet
    if (!conn) {
        DEBUG("Ignoring packet for connection on port %u", 
            packet->header().destination().port());
        send_reset(packet->header().source());
        return;
    }

    DEBUG("Delivering packet to connection");
    // conn->on_receive(std::move(packet));
}

int Transport::transmit(Connection::Connection_ptr conn, Connection::Buffer buf) {
    /*
    size_t sent = 0;
    Packet_ptr packet = nullptr;
    while(sent < buf.second) {
        packet = build_packet(conn, buf)
        
        _dev.try_transmit(std::move(packet));
    } */
   return -1;
}

Transport::Packet_ptr Transport::build_packet(Connection::Connection_ptr conn, 
                                            Connection::Buffer buf) {
    return nullptr;
}

std::unique_ptr<Socket> Transport::alloc_socket(int type) {
    if (type != SOCK_STREAM && type != SOCK_SEQPACKET) {
        return nullptr;
    }
    
    return std::unique_ptr<Socket>{
        new Socket(type, Socket::State::UNBOUND, *this)
    };
}

bool Transport::create_listener(Address addr, size_t backlog, Listener::ConnectCallback cb) {
    if (addr.cid() != _dev.cid()) {
        DEBUG("CID %u does not match device's CID %u", addr.cid(), _dev.cid());
        return false;
    }
    // Check first if a listener already exists
    if (_listeners.count(addr.port()) > 0) {
        DEBUG("Listener already exists on port %u", addr.port());
        return false;
    }
    
    Listener::Listener_ptr ls = std::unique_ptr<Listener>{new Listener(backlog, cb)};
    _listeners.insert({addr.port(), std::ref(ls)});
    DEBUG("Created listener on port %u", addr.port());
    return true;
}

bool Transport::bind_socket(Socket& socket, Address addr) {
    if (addr.cid() != _dev.cid()) {
        DEBUG("CID %u does not match device's CID %u", addr.cid(), _dev.cid());
        return false;
    }
    if (port_bound(addr.port())) {
        DEBUG("Port %u is busy", addr.port());
        return false;
    }

    _bindings.insert({addr.port(), std::ref(socket)});
    DEBUG("Socket bound to port %u", addr.port());
    return true;
}

bool Transport::connect(Socket& socket, Address addr) {

    return false;
    /*
    Packet packet = new Packet::Packet(
        new Packet::Header(src, addr, 0, socket.type(), VIRTIO_VSOCK_OP_REQUEST, 
        0, conn.buf_alloc(), conn.fwd_cnt())
        , nullptr
    );

    if (!hw::Vsock::try_transmit(&packet)) {

    }
    */
}

void Transport::send_reset(Address dest) {
    DEBUG("Sending reset to %u:%u", dest.cid(), dest.port());
}
