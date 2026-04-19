#ifndef RTB_CONSTANTS_H
#define RTB_CONSTANTS_H
#include <string_view>


namespace rtb::constants {
    inline constexpr std::string_view DESKTOP = "desktop";
    inline constexpr std::string_view MOBILE = "mobile";
    inline constexpr std::string_view TABLET = "tablet";

    inline constexpr std::string_view CTV = "ctv";
    inline constexpr std::string_view CONNECTED_TV = "connected_tv";

    // since connected_tv can be either ctv or connected_tv, use this helper function to return if it is
    constexpr bool is_connected_tv(std::string_view device_type) {
        return device_type == CTV || device_type == CONNECTED_TV;
    }

}
#endif