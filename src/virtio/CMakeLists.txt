set(SRCS
  virtio_queue.cpp
  virtqueue.cpp
  virtio_pci.cpp
)
if (NOT CMAKE_TESTING_ENABLED)
  list(APPEND SRCS
    virtio.cpp
  )
endif()
add_library(virtio OBJECT ${SRCS})
