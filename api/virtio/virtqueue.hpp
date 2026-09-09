#ifndef VIRTIO_VIRTQUEUE_HPP
#define VIRTIO_VIRTQUEUE_HPP

#include <arch.hpp>
#include <cstddef>
#include <cstdint>
#include <string>
#include <span>
#include <utility>

#define ALIGN(x) (((x) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))

/**
 * Virtqueue implementation in accordance with VirtIO v1.4
 */

class Virtqueue {
public:
    /** @note Using typedefs in order to keep the standard notation. */
    using le64 =  uint64_t;
    using le32 = uint32_t;
    using le16 = uint16_t;
    using u16 = uint16_t;
    using u8 = uint8_t;

    using Buffer = std::pair<uint8_t*, size_t>;

    class Token {
    public:
        enum Direction { IN, OUT };
        
        inline Token(Buffer buf, Direction d) :
            data_{ buf.first }, size_{ buf.second }, dir_{ d } 
        {}

        uint8_t* data() const noexcept { 
            return data_; 
        }

        size_t size() const noexcept {
            return size_; 
        }

        Direction direction() const noexcept {
            return dir_;
        }

    private:
        uint8_t* data_;
        size_t size_;
        Direction dir_;
    };

    /** Virtio Ring Descriptor. Virtio std. §2.7.5  */
    static constexpr le16 VIRTQ_DESC_F_NEXT = 1;
    static constexpr le16 VIRTQ_DESC_F_WRITE = 2;
    static constexpr le16 VIRTQ_DESC_F_INDIRECT = 4;
    static constexpr le16 VIRTQ_AVAIL_F_NO_INTERRUPT = 1;
    static constexpr le16 VIRTQ_USED_F_NO_NOTIFY = 1;

    struct virtq_desc {
        le64 addr;
        le32 len;
        le16 flags;
        le16 next;
    };

    // TODO: Indirect descriptors?

    /** Virtio Available ring. Virtio std. §2.7.6 */
    struct virtq_avail {
        le16 flags;
        le16 idx;
        le16 ring[];
    };

    /** Virtio Used ring elements. Virtio std. §2.7.8 */
    struct virtq_used_elem {
        le32 id;
        le32 len;
    };

    struct virtq_used {
        le16 flags;
        le16 idx;
        virtq_used_elem ring[];
    };

    /** Runtime view of the three variable-sized split-ring areas. */
    struct virtq {
        virtq_desc* desc;
        virtq_avail* avail;
        virtq_used* used;
    };

    /** Virtque size calculation. Virtio std. §2.7.2 */
    static unsigned virtq_size(unsigned int qsz) {
        return ALIGN(sizeof(virtq_desc)*qsz + sizeof(u16)*(3 + qsz))
            + ALIGN(sizeof(u16)*3 + sizeof(virtq_used_elem)*qsz);
    }

    /**
       Update the available index */
    inline void update_avail_idx ()
    {
        // Std. §3.2.1 pt. 4
        __arch_hw_barrier();
        _queue.avail->idx += _num_added;
        _num_added = 0;
    }

    /** Kick hypervisor. Will notify the host about pending data  */
    void kick();

    /** Constructor. @param size should be fetched from PCI device. */
    Virtqueue() = default;

    Virtqueue(const std::string& name, uint16_t size, uint16_t q_index, uint16_t iobase);

    /** Get the queue descriptor. To be written to the Virtio device. */
    virtq_desc* queue_desc() const { return _queue.desc; }
    virtq_avail* queue_avail() const { return _queue.avail; }
    virtq_used* queue_used() const { return _queue.used; }

    /** Push data tokens onto the queue.
        @param buffers : A span of tokens
    */
    int enqueue(std::span<Token> buffers);

    /** Dequeue a received packet */
    Token dequeue();

    void disable_interrupts();
    void enable_interrupts();
    bool interrupts_enabled() const noexcept;

    /** Release token. @param head : the token ID to release*/
    void release(uint32_t head);

    /** Get number of new incoming buffers, i.e. the increase in
        queue_used_->idx since we last checked. An increase means the device
        has inserted tokens into the used ring.*/
    uint16_t new_incoming() const noexcept
    { return _queue.used->idx - _last_used_idx; }

    /** Get number of used buffers */
    uint16_t num_used() const noexcept
    { return _queue.avail->idx - _queue.used->idx; }

    uint16_t num_inflight() const noexcept
    { return _desc_in_flight; }

    /** Get number of free tokens in Queue */
    uint16_t num_free() const noexcept
    {
      //Expects(size() - _free_head == size() - _desc_in_flight);
      return size() - _desc_in_flight;
    }

    /** Access the current index */
    virtq_desc& current()
    { return _queue.desc[_free_head]; }
    
    /** Access the next index */
    virtq_desc& next()
    { return _queue.desc[_queue.desc[_free_head].next]; }

    /** Move current index to next */
    void go_next()
    { _free_head = _queue.desc[_free_head].next; }

    uint16_t size() const noexcept { return _size; }

    uint16_t iobase() const noexcept { return _iobase; }

    uint16_t pci_index() const noexcept { return _pci_index; }

private:
    /** Initialize the queue buffer */
    void init_queue(int size, char* buf);

    std::string qname;

    // The size as read from the PCI device
    uint16_t _size = 0;

    // The actual queue struct
    virtq _queue{};

    // TODO: Likely need to change this
    uint16_t _iobase = 0; // Device PCI location
    uint16_t _free_head = 0; // First available descriptor (_queue.desc[_free_head])
    uint16_t _num_added = 0; // Entries to be added to _queue.avail->idx
    uint16_t _desc_in_flight = 0; // Entries in _queue_desc currently in use
    uint16_t _last_used_idx = 0; // Last known value of _queue.used->idx
    uint16_t _pci_index = 0; // Queue nr.
};

#endif
