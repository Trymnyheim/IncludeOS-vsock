#include "connection.hpp"
#include "listener.hpp"

#include <map>
#include <unordered_map>

#pragma once

namespace net::vsock {

class Socket;

class Transport {
public:


    /**
     * Returns the vsock::Transport singleton
     */
    static Transport& get();

    /**
     * Allocates and returns a non-bound, non-connected socket object
     * Returns a nullptr
     */
    std::unique_ptr<Socket> alloc_socket(int type);

private:
    Transport() = default;

    using Listeners = std::map<Address, std::shared_ptr<Listener>>;
    using Connections = std::unordered_map<Connection::Tuple, Connection::Connection_ptr>;
};

}