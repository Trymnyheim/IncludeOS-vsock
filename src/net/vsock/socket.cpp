#include <net/vsock/socket.hpp>
#include <net/vsock/transport.hpp>
#include <sys/socket.h>

#define VSOCK_SOCKET_DEBUG
#ifdef VSOCK_SOCKET_DEBUG
#define DEBUG(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
#endif

using namespace net::vsock;

Socket::Socket_ptr Socket::socket(int type) {
    if (type != SOCK_STREAM && type != SOCK_SEQPACKET) {
        return nullptr;
    }
    DEBUG("[ Socket ] Creating socket of type %s", 
        type == SOCK_STREAM ? "SOCK_STREAM" : "SOCK_SEQPACKET");
    return Transport::get().alloc_socket(type);
}

int Socket::bind(vsock::Address addr) {
    return -1;
}

int Socket::listen(int backlog) {
    DEBUG("[ Socket ] Listening with backlog ", backlog);
    return -1;
}

Socket::Socket_ptr Socket::accept() {
    DEBUG("[ Socket ] Accepting connection in backlog");
    return nullptr;
}

int Socket::connect(vsock::Address addr) {
    DEBUG("[ Socket ] Connecting to CID %u PORT %u", addr.cid(), addr.port());
    return -1;
}

int Socket::send(auto buf, int size, int flags) {
    DEBUG("[ Socket ] Sending data (%x) of size (%d)", buf, size);
    return -1;
}

int Socket::recv(auto buf, int size, int flags) {
    DEBUG("[ Socket ] Receiving data (%x) of maximum size (%d)", buf, size);
    return -1;
}

int Socket::close() {
    DEBUG("[ Socket ] Closing socket");
    return -1;
}

int Socket::shutdown() {
    // TODO: Difference between shutdown and close
    return close();
}


