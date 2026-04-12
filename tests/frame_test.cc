#include "rtb/frame.h"

#include <arpa/inet.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

int main() {
    rtb::engine::ReusableBuffer buffer;
    buffer.reserve(64);

    if (buffer.storage.size() != 64) {
        return 1;
    }

    auto writable = buffer.writable_region();
    if (writable.size() != 64) {
        return 1;
    }

    std::uint32_t payload_length = htonl(4);
    std::memcpy(writable.data(), &payload_length, sizeof(payload_length));
    writable[4] = std::byte {0x01};
    writable[5] = std::byte {0x02};
    writable[6] = std::byte {0x03};
    writable[7] = std::byte {0x04};

    buffer.commit_write(8);
    if (buffer.readable_bytes() != 8) {
        return 1;
    }

    rtb::engine::FrameView frame;
    const auto status = buffer.try_extract_frame(1024, frame);
    if (status != rtb::engine::FrameStatus::kFrameReady) {
        return 1;
    }

    if (frame.payload_length != 4 || frame.total_frame_bytes != 8 || frame.payload.size() != 4) {
        return 1;
    }

    if (frame.payload[0] != std::byte {0x01} ||
        frame.payload[1] != std::byte {0x02} ||
        frame.payload[2] != std::byte {0x03} ||
        frame.payload[3] != std::byte {0x04}) {
        return 1;
    }

    buffer.consume_frame(8);
    if (buffer.readable_bytes() != 0) {
        return 1;
    }

    return 0;
}
