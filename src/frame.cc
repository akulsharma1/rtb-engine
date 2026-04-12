#include "rtb/frame.h"

#include <algorithm>
#include <arpa/inet.h>
#include <cstring>

namespace rtb::engine {

void ReusableBuffer::reserve(std::size_t capacity) {
    if (storage.size() < capacity) {
        storage.resize(capacity);
    }
}

std::span<std::byte> ReusableBuffer::writable_region() {
    if (write_offset > storage.size()) {
        return {};
    }
    return std::span<std::byte>(storage.data() + write_offset,
                                storage.size() - write_offset);
}

void ReusableBuffer::commit_write(std::size_t bytes_written) {
    write_offset = std::min(write_offset + bytes_written, storage.size());
}

FrameStatus ReusableBuffer::try_extract_frame(std::uint32_t max_frame_size, FrameView& out) {
    out = {};

    const std::size_t total_bytes = readable_bytes();
    // The first 4 bytes are the big-endian payload length prefix.
    if (total_bytes < 4) {
        return FrameStatus::kNeedMoreData;
    }
    
    std::uint32_t network_payload_length = 0;
    std::memcpy(&network_payload_length, storage.data() + read_offset, sizeof(network_payload_length));
    const std::uint32_t payload_length = ntohl(network_payload_length);

    if (payload_length == 0 || payload_length > max_frame_size) {
        return FrameStatus::kInvalidLength;
    }

    if (total_bytes < sizeof(std::uint32_t) + payload_length) {
        return FrameStatus::kNeedMoreData;
    }

    out.payload = std::span<const std::byte> { storage.data() + read_offset + sizeof(std::uint32_t), payload_length };
    out.payload_length = payload_length;
    out.total_frame_bytes = sizeof(std::uint32_t) + payload_length;

    return FrameStatus::kFrameReady;
}

void ReusableBuffer::consume_frame(std::size_t total_frame_bytes) {
    read_offset = std::min(read_offset + total_frame_bytes, write_offset);
    if (read_offset == write_offset) {
        read_offset = 0;
        write_offset = 0;
    }
}

void ReusableBuffer::compact() {
    if (read_offset == 0) {
        return;
    }

    const std::size_t unread_bytes = readable_bytes();
    if (unread_bytes > 0) {
        std::move(storage.begin() + static_cast<std::ptrdiff_t>(read_offset),
                  storage.begin() + static_cast<std::ptrdiff_t>(write_offset),
                  storage.begin());
    }

    read_offset = 0;
    write_offset = unread_bytes;
}

}  // namespace rtb::engine
