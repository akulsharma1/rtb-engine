#ifndef RTB_LOGS_LOGS_H_
#define RTB_LOGS_LOGS_H_

#include <cstdio>
#include <utility>

namespace rtb::logger {

inline constexpr bool kEnableLogs = false;

template <typename... Args>
inline void LOG(const char* format, Args&&... args) {
    if constexpr (kEnableLogs) {
        std::fprintf(stdout, format, std::forward<Args>(args)...);
        std::fputc('\n', stdout);
    }
}

template <typename... Args>
inline void LOG_ERROR(const char* format, Args&&... args) {
    std::fprintf(stderr, format, std::forward<Args>(args)...);
    std::fputc('\n', stderr);
}

}  // namespace rtb::logger

#endif  // RTB_LOGS_LOGS_H_
