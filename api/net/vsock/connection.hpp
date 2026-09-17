#include "address.hpp"
#include "common.hpp"

#include <sys/socket.h> // SOCK_STREAM
#include <cstdint>
#include <memory>
#include <utility>
#include <deque>
#include <delegate>

#pragma once

namespace net::vsock {
class Socket;

/**
 * Should store and manage the connection and state
 */
class Connection {
public:
    using Tuple = std::pair<Address, Address>;
    using Connection_ptr = std::shared_ptr<Connection>;
    using Buffer = std::pair<uint8_t*, size_t>;

    Connection(Address local, Address remote, int type) 
        : _local(local), _remote(remote), _stream_type(type) {}

    Connection::Tuple tuple() const noexcept {
        return {_local, _remote}; 
    }

    int stream_type() const noexcept {
        return _stream_type;
    };

    Address local() const noexcept {
        return _local;
    }

    Address remote() const noexcept {
        return _remote;
    }

    Flow flow() const noexcept {
        return _flow;
    }

    int write();

    int read();

    // int close();

private:

    Address _local;
    Address _remote;

    #define MAX_BUFFER_SIZE 1024 * 1024 * 4
    class WriteBuffer {
    public:
        using WriteCallback = delegate<void(size_t)>;

        explicit WriteBuffer() : _size(0) {}

        size_t write();

        size_t read();

        bool empty() const noexcept {
            return _size == 0;
        }

        size_t avail_space() const noexcept {
            return MAX_BUFFER_SIZE - _size;
        }

        void on_avail_space(WriteCallback cb) {
            _on_avail_space = std::move(cb);
        }
        
    private:
        std::deque<uint8_t> _q;
        size_t _size;
        WriteCallback _on_avail_space;
    };

    int _stream_type;
    WriteBuffer _queue;

    // Contains credit info
    Flow _flow;
};


}

/**
 * 5.10.6.1 Virtqueue Flow Control
 * 
 * The tx virtqueue carries packets initiated by applications and replies to received packets. The rx virtqueue carries packets initiated by the device and replies to previously transmitted packets.
 * If both rx and tx virtqueues are filled by the driver and device at the same time then it appears that a deadlock is reached. The driver has no free tx descriptors to send replies. The device has no free rx descriptors to send replies either. Therefore neither device nor driver can process virtqueues since that may involve sending new replies.
 * This is solved using additional resources outside the virtqueue to hold packets. With additional resources, it becomes possible to process incoming packets even when outgoing packets cannot be sent.
 * Eventually even the additional resources will be exhausted and further processing is not possible until the other side processes the virtqueue that it has neglected. This stop to processing prevents one side from causing unbounded resource consumption in the other side.
 * 
 * 5.10.6.1.1
 * The rx virtqueue MUST be processed even when the tx virtqueue is full so long as there are additional resources available to hold packets outside the tx virtqueue.
 */