#include "common.hpp"
#include "address.hpp"

#include <cstdint>
#include <utility>
#include <string>

#pragma once

namespace net::vsock {

class Packet {
public:

    using Buffer = std::vector<uint8_t>;

    class Header {
    public:
        Header(Address src, Address dst, uint32_t len, uint16_t type, uint16_t op,
                            uint32_t flags, uint32_t buf_alloc, uint32_t fwd_cnt)
            : _hdr{
                .src_cid   = src.cid(),
                .dst_cid   = dst.cid(),
                .src_port  = src.port(),
                .dst_port  = dst.port(),
                .len       = len,
                .type      = type,
                .op        = op,
                .flags     = flags,
                .buf_alloc = buf_alloc,
                .fwd_cnt   = fwd_cnt
            }
        {}

        Header(struct virtio_vsock_hdr hdr) : _hdr(hdr) {}

        static uint32_t size() {
            return sizeof(struct virtio_vsock_hdr);
        }

        Address source() const noexcept {
            return Address(_hdr.src_cid, _hdr.src_port);
        }

        Address destination() const noexcept {
            return Address(_hdr.dst_cid, _hdr.dst_port); 
        }

        uint32_t len() const noexcept {
            return _hdr.len;
        }

        uint16_t type() const noexcept {
            return _hdr.type;
        }

        uint16_t op() const noexcept {
            return _hdr.op;
        }
    
        uint32_t flags() const noexcept {
            return _hdr.flags;
        }

        uint32_t buf_alloc() const noexcept {
            return _hdr.buf_alloc;
        }

        uint32_t fwd_cnt() const noexcept {
            return _hdr.fwd_cnt;
        }

        struct virtio_vsock_hdr serialized() const noexcept {
            return _hdr;
        }

    private:
        struct virtio_vsock_hdr _hdr;
    };

    Packet(Header hdr, std::vector<uint8_t> payload)
        : _hdr(std::move(hdr)), _payload(std::move(payload)) {}

    const Header& header() const noexcept {
        return _hdr;
    }

    const Buffer& data() const noexcept {
        return _payload;
    }

    uint32_t payload_len() const noexcept {
        return header().len();
    }

    std::string to_string() const {
        auto src = header().source();
        auto dst = header().destination();
        return "src=" + std::to_string(src.cid()) + ":" + std::to_string(src.port())
            + "dest=" + std::to_string(dst.cid()) + ":" + std::to_string(dst.port())
            + " len=" + std::to_string(header().len())
            + " type=" + std::to_string(header().type())
            + " op=" + std::to_string(header().op());
    }

private:
    Header _hdr;
    Buffer _payload;
};

}