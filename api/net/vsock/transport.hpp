#include "connection.hpp"
#include "listener.hpp"
#include "packet.hpp"
#include <hw/vsock.hpp>

#include <functional>
#include <map>
#include <unordered_map>

#pragma once

namespace net::vsock {

class Socket;

class Transport {
public:

    using Packet_ptr = std::unique_ptr<net::vsock::Packet>;
    using Bindings = std::map<uint32_t, std::reference_wrapper<Socket>>;
    using Listeners = std::map<uint32_t, Listener::Listener_ptr>;
    using Connections = std::map<Connection::Tuple, Connection::Connection_ptr>;

    /**
     * Returns the vsock::Transport singleton
     */
    static Transport& getInstance();

    /**
     * Allocates and returns a non-bound, non-connected socket object
     * Returns a nullptr
     */
    std::unique_ptr<Socket> alloc_socket(int type);

    bool create_listener(Address addr, size_t backlog, Listener::ConnectCallback cb);

    bool bind_socket(Socket& socket, Address addr);

    bool connect(Socket&, Address);

    int transmit(Connection::Connection_ptr conn, Connection::Buffer buf); 

    void on_receive(Packet_ptr packet);

    Packet_ptr build_packet(Connection::Connection_ptr conn, Connection::Buffer buf);

    void send_reset(Address dest);

    bool port_bound(uint32_t port) {
        return _bindings.count(port) > 0;
    };

protected:
    Transport(hw::Vsock& dev);

private:

    hw::Vsock& dev() const noexcept {
        return _dev;
    }

    Bindings _bindings;
    Connections _connections;
    Listeners _listeners;
    hw::Vsock& _dev;
};

}
