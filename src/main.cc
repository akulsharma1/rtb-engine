#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "rtb/server.h"
#include "rtb.pb.h"

int main(int argc, char* argv[]) {
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    std::vector<std::string_view> args;
    args.reserve(static_cast<std::size_t>(argc > 1 ? argc - 1 : 0));
    for (int index = 1; index < argc; ++index) {
        args.emplace_back(argv[index]);
    }

    rtb::engine::ServerConfig config;
    std::string error;
    const rtb::engine::ParseServerConfigStatus parse_status =
        rtb::engine::parse_server_config(args, config, error);

    if (parse_status == rtb::engine::ParseServerConfigStatus::kHelpRequested) {
        rtb::engine::print_server_usage(std::cout, argv[0]);
        google::protobuf::ShutdownProtobufLibrary();
        return 0;
    }

    if (parse_status == rtb::engine::ParseServerConfigStatus::kInvalidArgument) {
        std::cerr << "Error: " << error << '\n';
        rtb::engine::print_server_usage(std::cerr, argv[0]);
        google::protobuf::ShutdownProtobufLibrary();
        return 1;
    }

    const int exit_code = rtb::engine::run_server(config);
    google::protobuf::ShutdownProtobufLibrary();
    return exit_code;
}
