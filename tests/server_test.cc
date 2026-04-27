#include "rtb/server.h"

#include <string>
#include <string_view>
#include <vector>

int main() {
    using namespace rtb::engine;

    {
        ServerConfig config;
        std::string error;
        const std::vector<std::string_view> args;
        const auto status = parse_server_config(args, config, error);
        if (status != ParseServerConfigStatus::kOk ||
            config.port != 8080 ||
            config.workers != 1 ||
            config.campaign_data_path.find("sample_campaigns.csv") == std::string::npos ||
            !error.empty()) {
            return 1;
        }
    }

    {
        ServerConfig config;
        std::string error;
        const std::vector<std::string_view> args {
            "--port", "9090",
            "--workers", "3",
            "--campaign-data-path", "/tmp/campaigns.csv",
        };
        const auto status = parse_server_config(args, config, error);
        if (status != ParseServerConfigStatus::kOk ||
            config.port != 9090 ||
            config.workers != 3 ||
            config.campaign_data_path != "/tmp/campaigns.csv") {
            return 1;
        }
    }

    {
        ServerConfig config;
        std::string error;
        const std::vector<std::string_view> args {"--help"};
        const auto status = parse_server_config(args, config, error);
        if (status != ParseServerConfigStatus::kHelpRequested) {
            return 1;
        }
    }

    {
        ServerConfig config;
        std::string error;
        const std::vector<std::string_view> args {"--port", "70000"};
        const auto status = parse_server_config(args, config, error);
        if (status != ParseServerConfigStatus::kInvalidArgument ||
            error != "invalid value for --port") {
            return 1;
        }
    }

    {
        ServerConfig config;
        std::string error;
        const std::vector<std::string_view> args {"--workers", "0"};
        const auto status = parse_server_config(args, config, error);
        if (status != ParseServerConfigStatus::kInvalidArgument ||
            error != "invalid value for --workers") {
            return 1;
        }
    }

    {
        ServerConfig config;
        std::string error;
        const std::vector<std::string_view> args {"--bogus"};
        const auto status = parse_server_config(args, config, error);
        if (status != ParseServerConfigStatus::kInvalidArgument ||
            error != "unknown argument: --bogus") {
            return 1;
        }
    }

    return 0;
}
