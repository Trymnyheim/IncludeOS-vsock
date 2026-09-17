#include <cstdint>

#pragma once

namespace net::vsock {

class Address {
public: 
    Address(uint32_t cid, uint32_t port) : _cid(cid), _port(port) {}
    
    uint32_t cid() const noexcept { 
        return _cid; 
    }

    uint32_t port() const noexcept {
        return _port;
    }

    bool operator<(const Address& other) const noexcept {
        return _cid < other._cid
            || (_cid == other._cid && _port < other._port);
    }
private:
    uint32_t _cid;
    uint32_t _port;
};

}