#include "address.hpp"

#include <sys/socket.h> // SOCK_STREAM
#include <cstdint>
#include <memory>
#include <utility>

#pragma once

namespace net::vsock {
class Socket;

/**
 * Should store and manage the connection and state
 */
class Connection {
public:
    using Tuple = std::pair<Socket, Socket>;
    using Connection_ptr = std::shared_ptr<Connection>;

private:
    uint32_t _tx_cnt;
    uint32_t _last_fwd_cnt;
    uint32_t _peer_buf_alloc;

    uint32_t _fwd_cnt;
    uint32_t _last_fwd_cnt;
    uint32_t _rx_bytes;
    uint32_t _buf_alloc;
    uint32_t _buf_used;

    int& _backlog;
    Address _bind_addr;
    Address _connect_addr;
};

}