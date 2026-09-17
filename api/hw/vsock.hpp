#pragma once

#include <cstdint>
#include <memory>
#include <delegate>
#include <net/vsock/packet.hpp>
#include <info>

namespace hw {

class Vsock {
public:
    using Packet_ptr = std::unique_ptr<net::vsock::Packet>;
    using Receive_handler = delegate<void(Packet_ptr)>;
    using Notify_handler = delegate<void()>;

    virtual ~Vsock() = default;

    virtual uint32_t cid() const noexcept = 0;

    // Success: driver takes ownership and clears packet.
    // Queue full: returns false and leaves packet unchanged.
    virtual bool try_transmit(Packet_ptr& packet) = 0;

    void on_receive(Receive_handler handler) {
        receive_handler_ = std::move(handler);
    }

    void on_transmit_available(Notify_handler handler) {
        transmit_handler_ = std::move(handler);
    }

    void on_transport_reset(Notify_handler handler) {
        reset_handler_ = std::move(handler);
    }

protected:
    void deliver(Packet_ptr packet) {
        if (receive_handler_)
            receive_handler_(std::move(packet));
    }

    void notify_transmit_available() {
        if (transmit_handler_)
            transmit_handler_();
    }

    void notify_transport_reset() {
        if (reset_handler_)
            reset_handler_();
    }

private:
    Receive_handler receive_handler_;
    Notify_handler transmit_handler_;
    Notify_handler reset_handler_;
};

} // namespace hw