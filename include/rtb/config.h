#ifndef RTB_CONFIG_H
#define RTB_CONFIG_H

#include <cstdint>

namespace rtb::config {
    inline constexpr bool kEnableLogs = true;
    inline constexpr std::uint64_t kResponseSafetyMarginNs = 1'000'000;
    inline constexpr std::uint64_t kNsPerMs = 1'000'000;
    inline constexpr double kMinBidPrice = 0.05;
    inline constexpr double kMaxBidPrice = 5.00;
    inline constexpr std::uint64_t kDefaultRngSeed = 0x9E3779B97F4A7C15ULL;
    inline constexpr std::uint32_t kMaxOutboundFrameSize = 64 * 1024;
}
#endif
