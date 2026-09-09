// This file is a part of the IncludeOS unikernel - www.includeos.org
//
// Copyright 2015 Oslo and Akershus University College of Applied Sciences
// and Alfred Bratterud
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

//#undef NO_DEBUG
#define DEBUG // Allow debug
#define DEBUG2

#include <kernel/memmap.hpp>
#include <os.hpp>
#include <kernel.hpp>
#include <virtio/virtqueue.hpp>
#if !defined(__MACH__)
#include <malloc.h>
#else
extern void *memalign(size_t, size_t);
#endif
#include <expects>
#include <cstring>
#include <cassert>
#include <debug>

Virtqueue::Virtqueue(const std::string& name, uint16_t size, uint16_t q_index, uint16_t iobase)
    : qname(name), _size(size), _iobase(iobase), _free_head(0), _num_added(0),_last_used_idx(0), _pci_index(q_index) {
        
    size_t total_bytes = virtq_size(size);
    // Allocate page-aligned size and clear it
    void* buffer = memalign(PAGE_SIZE, total_bytes);
    if (!buffer) {
        os::panic("Virtio queue could not allocate aligned queue area");
    }

    memset(buffer, 0, total_bytes);

    debug(">>> Virtio Queue %s of size %i (%u bytes) initializing \n",
        qname.c_str(), _size, total_bytes);
    init_queue(size, (char*) buffer);

  // Chain buffers
    debug("\t * Chaining buffers \n");
    for (int i=0; i<size; i++) {
        _queue.desc[i].next = i+1;
    }
    _queue.desc[size -1].next = 0;
    debug(" >> Virtio Queue %s setup complete. \n", qname.c_str());
}

void Virtqueue::init_queue(int size, char* buf)
{
    // The buffer starts with is an array of queue descriptors (i.e. tokens)
    _queue.desc = (virtq_desc*) buf;
    debug("\t * Queue desc  @ 0x%lx \n ",(long)_queue.desc);

    // The available buffer starts right after the queue descriptors
    _queue.avail = (virtq_avail*) &buf[size * sizeof(virtq_desc)];
    debug("\t * Queue avail @ 0x%lx \n ",(long)_queue.avail);

    // The used queue starts at the beginning of the next page
    _queue.used = (virtq_used*) (((uintptr_t) &_queue.avail->ring[size] + 
                        sizeof(uint16_t) + PAGE_SIZE-1) & ~(PAGE_SIZE-1));
    debug("\t * Queue used  @ 0x%lx \n ",(long)_queue.used);
}

/** Ported more or less directly from SanOS. */
int Virtqueue::enqueue(std::span<Token> buffers) {
    debug("<%s> Enqueuing %i tokens \n", qname.c_str(), buffers.size());

    uint16_t last = _free_head;
    uint16_t first = _free_head;
    // Place each buffer in a token
    for (auto buf : buffers) {
        debug ("%s:  buf @ %p \n", qname.c_str(), buf.data());

        // Set read / write flags
        _queue.desc[_free_head].flags =
        buf.direction() ? VIRTQ_DESC_F_NEXT : VIRTQ_DESC_F_NEXT | VIRTQ_DESC_F_WRITE;

        // Assign raw buffer
        _queue.desc[_free_head].addr =
        os::mem::virt_to_phys(
        reinterpret_cast<uintptr_t>(buf.data()));
        _queue.desc[_free_head].len = buf.size();

        last = _free_head;
        _free_head = _queue.desc[_free_head].next;
    }

    _desc_in_flight += buffers.size();
    Ensures(_desc_in_flight <= size());

    // No continue on last buffer
    _queue.desc[last].flags &= ~VIRTQ_DESC_F_NEXT;


    // Place the head of this current chain in the avail ring
    uint16_t avail_index = (_queue.avail->idx + _num_added) % size();

    // we added a token
    _num_added++;

    _queue.avail->ring[avail_index] = first;

    debug("<%s> avail_index: %u size: %u, free_head %u num free: %u\n",
        qname.c_str(), avail_index, size(), _free_head, num_free());

    return buffers.size();
}

void Virtqueue::release(uint32_t head) {
    // Mark queue element "head" as free (the whole token chain)
    uint32_t i = head;
    _desc_in_flight --;

    while (_queue.desc[i].flags & VIRTQ_DESC_F_NEXT) {
        i = _queue.desc[i].next;
        _desc_in_flight --;
    }

    // Add buffers back to free list
    _queue.desc[i].next = _free_head;
    _free_head = head;

    debug("<%s> Descriptors in flight: %i \n", qname.c_str(), _desc_in_flight);
}

Virtqueue::Token Virtqueue::dequeue() {
    debug("<%s> Dequeueing  last_used index %i ", qname.c_str(), _last_used_idx);

    // Get next completed buffer
    auto& e = _queue.used->ring[_last_used_idx % _size];
    debug("<%s> Releasing token @%p, nr. %i Len: %i\n", qname.c_str(), &e, e.id, e.len);

    // Release buffer
    release(e.id);
    _last_used_idx++;
    // return token:
    return {{(uint8_t*) _queue.desc[e.id].addr, e.len }, Token::IN};
}

void Virtqueue::disable_interrupts() {
    _queue.avail->flags |= (1 << VIRTQ_AVAIL_F_NO_INTERRUPT);
}
void Virtqueue::enable_interrupts() {
    _queue.avail->flags &= ~(1 << VIRTQ_AVAIL_F_NO_INTERRUPT);
}
bool Virtqueue::interrupts_enabled() const noexcept {
    return (_queue.avail->flags & (1 << VIRTQ_AVAIL_F_NO_INTERRUPT)) == 0;
}

// this will force most of the implementation to not use PCI
// and thus be more easily testable
#include <hw/pci.hpp>
void Virtqueue::kick() {
    update_avail_idx();

#if defined (PLATFORM_UNITTEST)
  // do nothing here
#elif defined(ARCH_x86)
  // Std. §3.2.1 pt. 4
  __arch_hw_barrier();
  if (!(_queue.used->flags & VIRTQ_USED_F_NO_NOTIFY)) {
    debug("<%s> Kicking virtio. Iobase 0x%x \n", qname.c_str(), _iobase);
    // hw::outpw(_iobase + VIRTIO_PCI_QUEUE_NOTIFY , _pci_index); // TODO: Fix notify_cap
  } else {
    debug("<%s> Virtio device says we can't kick!\n", qname.c_str());
  }
#else
#warning "kick() not implemented for selected arch"
#endif
}
