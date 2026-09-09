#include <cstdint>

#pragma once

namespace net::vsock {

class Address {
public: 
    Address(uint32_t cid, uint16_t port) : _cid(cid), _port(port) {}
    uint32_t cid() { return _cid; }
    uint32_t port() { return _port; }
private:
    uint32_t _cid;
    uint16_t _port;
};

}