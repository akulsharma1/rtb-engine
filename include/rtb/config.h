#ifndef RTB_CONFIG_H
#define RTB_CONFIG_H

#include <cstdint>

namespace rtb::config {
    inline constexpr std::uint64_t kResponseSafetyMarginNs = 1'000'000;
    inline constexpr std::uint64_t kNsPerMs = 1'000'000;
}
#endif