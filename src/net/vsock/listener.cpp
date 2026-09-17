#include "net/vsock/listener.hpp"
#include "net/vsock/transport.hpp"

#define VSOCK_LISTENER_DEBUG
#ifdef VSOCK_LISTENER_DEBUG
#define DEBUG(fmt, ...) printf("[ VSOCK: Transport ] " fmt"\n", ##__VA_ARGS__)
#else
#endif

using namespace net::vsock;

Listener::Listener(size_t backlog, ConnectCallback cb):  _max_backlog(backlog), 
        _transport(Transport::getInstance()), _cb(cb)
{}

bool Listener::handle_connect(Address peer) {
    if (!can_queue()) {
        DEBUG("Backlog is full. Rejecting connect-request");
        return false;
    }
    DEBUG("Adding connect-request to backlog");
    // TODO: Enqueue and call sockets listen callback

    _cb();
    return true;
}

void Listener::accept_connection() {
    DEBUG("accept_connection not implemented");
}