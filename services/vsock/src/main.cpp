#include "kernel/memory.hpp"
#include <os>
#include <service>
#include <vsock>
#include <sys/socket.h>

#define SRC_CID 3
#define SRC_PORT 4500

namespace vsock = net::vsock;

void on_connect() {
    std::cout << "Connection attempt received...\n";
}

void Service::start(const std::string& args){

    std::cout << "Current virtual mappings:\n";
    for (const auto& entry : os::mem::vmmap())
        std::cout << " {}" << entry.second.to_string() << "\n";
    std::cout << "\n";
    
    vsock::Socket::Socket_ptr socket = vsock::Socket::socket(SOCK_STREAM);
    vsock::Address addr = vsock::Address(SRC_CID, SRC_PORT);

    if (socket->bind(addr) < 0) {
        std::cout << "Failed to bind socket\n";
    }

    if (socket->listen(5, [](){on_connect();}) < 0) {
        std::cout << "Failed to listen on vsock\n";
    }
    std::cout << "Listening on vsock: CID " << addr.cid() << " PORT " << addr.port() << "\n";
}
