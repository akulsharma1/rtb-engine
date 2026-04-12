#ifndef RTB_FRAME_H_
#define RTB_FRAME_H_

#include <cstddef>
#include <cstdint>
#include <span>

#include "rtb/runtime_types.h"

namespace rtb::engine {

enum class FrameStatus : std::uint8_t {
    kNeedMoreData = 0,
    kFrameReady,
    kInvalidLength,
};

struct FrameView {
    std::span<const std::byte> payload {};
    std::uint32_t payload_length = 0;
    std::uint32_t total_frame_bytes = 0;
};

}  // namespace rtb::engine

#endif  // RTB_FRAME_H_
