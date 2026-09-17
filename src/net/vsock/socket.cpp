#include <net/vsock/socket.hpp>
#include <net/vsock/transport.hpp>
#include <sys/socket.h>

#define VSOCK_SOCKET_DEBUG
#ifdef VSOCK_SOCKET_DEBUG
#define DEBUG(fmt, ...) printf("[ VSOCK: Socket ] " fmt"\n", ##__VA_ARGS__)
#else
#endif

using namespace net::vsock;

Socket::Socket_ptr Socket::socket(int type) {
    if (type != SOCK_STREAM && type != SOCK_SEQPACKET) {
        return nullptr;
    }
    DEBUG("Creating socket of type %s", 
        type == SOCK_STREAM ? "SOCK_STREAM" : "SOCK_SEQPACKET");
    return Transport::getInstance().alloc_socket(type);
}

int Socket::bind(vsock::Address addr) {
    if (state() != State::UNBOUND) {
        DEBUG("Can not bind with state %d", state());
        return -1;
    }
    if (!_transport.bind_socket(*this, addr)) {
        DEBUG("Failed to bind socku<Listener>et to port %u", addr.port());
        return -1;
    }
    _state = State::BOUND;
    _bind_addr = addr;
    DEBUG("Socket successfully bound to port %u", addr.port());
    return 0;
}

int Socket::listen(int backlog, Listener::ConnectCallback cb) {
    if (_state != State::BOUND) {
        DEBUG("Can not listen on unbound socket\n");
        return -1;
    }
    if (!_transport.create_listener(_bind_addr, backlog, cb)) {
        return -1;
    }
    _state = State::Listening;
    return 0;
}

Socket::Socket_ptr Socket::accept() {
    DEBUG("Accepting connection");
    return nullptr;
}

int Socket::connect(vsock::Address addr) {
    DEBUG("Connecting to CID %u PORT %u", addr.cid(), addr.port());
    if (state() != State::BOUND) {
        DEBUG("Socket not bound");
        return -1;
    }
    if (!_transport.connect(*this, addr)) {
        // TODO: Assumes Transport::connect blocks... Change this?
        DEBUG("Failed to establish connection");
        return -1;
    }
    DEBUG("Connection established");
    return 0;
}

int Socket::send(auto buf, int size, int flags) {
    DEBUG("Sending data (%x) of size (%d)", buf, size);
    if (state() != State::Connected) {
        DEBUG("Socket not connected (state: %d)", _state);
        return -1;
    }
    return _conn->write();
}

int Socket::recv(auto buf, int size, int flags) {
    DEBUG("Receiving data (%x) of maximum size (%d)", buf, size);
    if (state() != State::Connected) {
        DEBUG("Socket not connected (state: %d)", _state);
        return -1;
    }
    return _conn->read();
}

int Socket::write(auto buf, int size, int flags) {
    return send(buf, size, flags);
}

int Socket::read(auto buf, int size, int flags) {
    return recv(buf, size, flags);
}

int Socket::close() {
    DEBUG("Closing socket");
    // conn->close();
    // Needs to be async
    _state = State::Closed;
    return -1;
}

int Socket::shutdown() {
    // TODO: Differenciate between shutdown and close
    return close();
}


