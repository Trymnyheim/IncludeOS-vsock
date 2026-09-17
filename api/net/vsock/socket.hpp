#include "address.hpp"
#include "listener.hpp"

#include <cstdint>
#include <memory>

#pragma once

namespace net::vsock {

class Transport;
class Connection;

class Socket {
public:
    using Socket_ptr = std::unique_ptr<Socket>;

    /** Factory method that calls lower layers to allocate socket 
     * @param type SOCK_STREAM or SOCK_SEQPACKET from sys/socket.h */
    static Socket_ptr socket(int type);

    int bind(Address addr);

    /**
     * @param backlog The maximum length to which the queue of pending connections for 
     * sockfd may grow
     */
    int listen(int backlog, Listener::ConnectCallback);

    /**
     * Accepts the first connection request in the backlog and creates a socket for the 
     * new connection
     */
    Socket_ptr accept();

    /**
     * Attempts to 
     * @param addr Contains the CID and port of the peer
     */
    int connect(Address addr);

    /**
     * Transmits data to the destination specified on connect.
     * @param buf Buffer containing the data to send
     * @param size Size of data (bytes)
     * @param flags ...
     */
    int send(auto buf, int size, int flags);
    
    int recv(auto buf, int size, int flags);
    
    /** Wrapper for the send-function */
    int write(auto buf, int size, int flags);
    
    /** Wrapper for the recv-function */
    int read(auto buf, int size, int flags);

    /** Tears down a socket's connection and frees it from memory */
    int close();

    // TODO: Differeniate between close and shutdown
    /** Calls close */
    int shutdown();

    // void setsockopt();

private:
    friend class Transport;

    enum class State {
        UNBOUND,
        BOUND,
        Listening,
        Connecting,
        Connected,
        Closed
    };

    Socket(int type, State state, Transport& transport) 
        : _type(type), _bind_addr({0, 0}), _state(state), _transport(transport) 
    {};

    int type() const noexcept {
        return _type;
    }

    State state() const noexcept {
        return _state;
    }

    int _type;
    Address _bind_addr;
    State _state;
    std::shared_ptr<Connection> _conn;
    std::shared_ptr<Listener> _listener;
    Transport& _transport;
};
}