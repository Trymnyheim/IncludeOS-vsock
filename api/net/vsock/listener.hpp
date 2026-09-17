#include "address.hpp"

#include <memory>
#include <deque>
#include <map>
#include <delegate>

#pragma once

namespace net::vsock {

class Transport;

class Listener {
public:
    using Listener_ptr = std::shared_ptr<Listener>;
    using Backlog = std::pair<std::deque<Address>, size_t>;
    using ConnectCallback = delegate<void()>;

    Listener(size_t backlog, ConnectCallback cb);

    bool handle_connect(Address peer);

    void accept_connection();

    bool can_queue() const noexcept {
        return _backlog.second < _max_backlog;
    }

private:
    size_t _max_backlog;
    Backlog _backlog;
    ConnectCallback _cb;
    Transport& _transport;
};
}