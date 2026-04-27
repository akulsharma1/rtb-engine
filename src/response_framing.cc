#include "rtb/response_framing.h"

#include <arpa/inet.h>
#include <cstdint>
#include <cstring>
#include <limits>

#include "rtb/config.h"

namespace rtb::engine {

bool stage_response_frame(const rtb::v1::BidResponse& response, ReusableBuffer& buffer) {
    const std::size_t payload_size = response.ByteSizeLong();
    if (payload_size == 0 ||
        payload_size > rtb::config::kMaxOutboundFrameSize ||
        payload_size > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return false;
    }

    if (buffer.readable_bytes() != 0) {
        return false;
    }

    const std::size_t total_frame_size = sizeof(std::uint32_t) + payload_size;
    buffer.reserve(total_frame_size);
    if (buffer.storage.size() < total_frame_size) {
        return false;
    }

    buffer.read_offset = 0;
    buffer.write_offset = 0;

    const std::uint32_t network_payload_size = htonl(static_cast<std::uint32_t>(payload_size));
    std::memcpy(buffer.storage.data(), &network_payload_size, sizeof(network_payload_size));

    if (!response.SerializeToArray(
            static_cast<void*>(buffer.storage.data() + sizeof(std::uint32_t)),
            static_cast<int>(payload_size))) {
        buffer.read_offset = 0;
        buffer.write_offset = 0;
        return false;
    }

    buffer.write_offset = total_frame_size;
    return true;
}

}  // namespace rtb::engine
