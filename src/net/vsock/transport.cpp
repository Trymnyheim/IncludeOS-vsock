#include "net/vsock/transport.hpp"
#include <net/vsock/socket.hpp>

using namespace net::vsock;

Transport& Transport::get() {
    static Transport singleton;
    return singleton;
}

std::unique_ptr<Socket> Transport::alloc_socket(int type) {
    if (type != SOCK_STREAM && type != SOCK_SEQPACKET) {
        return nullptr;
    }
    
    return std::unique_ptr<Socket>{
        new Socket(type, *this)
    };
}