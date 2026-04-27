#ifndef RTB_LOGS_LOGS_H_
#define RTB_LOGS_LOGS_H_

#include <cstdio>
#include <utility>

#include "rtb/config.h"

namespace rtb::logger {

template <typename... Args>
inline void LOG(const char* format, Args&&... args) {
    if constexpr (rtb::config::kEnableLogs) {
        if constexpr (sizeof...(Args) == 0) {
            std::fputs(format, stdout);
        } else {
            std::fprintf(stdout, format, std::forward<Args>(args)...);
        }
        std::fputc('\n', stdout);
        std::fflush(stdout);
    }
}

template <typename... Args>
inline void LOG_ERROR(const char* format, Args&&... args) {
    if constexpr (sizeof...(Args) == 0) {
        std::fputs(format, stderr);
    } else {
        std::fprintf(stderr, format, std::forward<Args>(args)...);
    }
    std::fputc('\n', stderr);
    std::fflush(stderr);
}

}  // namespace rtb::logger

#endif  // RTB_LOGS_LOGS_H_
