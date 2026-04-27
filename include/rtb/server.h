#ifndef RTB_SERVER_H_
#define RTB_SERVER_H_

#include <cstdint>
#include <iosfwd>
#include <span>
#include <string>
#include <string_view>

namespace rtb::engine {

struct ServerConfig {
    std::uint16_t port = 8080;
    int workers = 1;
    std::string campaign_data_path;
};

enum class ParseServerConfigStatus : std::uint8_t {
    kOk = 0,
    kHelpRequested,
    kInvalidArgument,
};

ParseServerConfigStatus parse_server_config(
    std::span<const std::string_view> args,
    ServerConfig& out,
    std::string& error
);

void print_server_usage(std::ostream& os, std::string_view program_name);

int run_server(const ServerConfig& config);

}  // namespace rtb::engine

#endif  // RTB_SERVER_H_
