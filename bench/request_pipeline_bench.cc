#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <span>
#include <string>
#include <string_view>

#include <arpa/inet.h>

#include "rtb/benchmark.h"
#include "rtb/campaign_store.h"
#include "rtb/config.h"
#include "rtb/handle_request.h"
#include "rtb/parse.h"
#include "rtb/response_framing.h"
#include "rtb/runtime_types.h"
#include "rtb.pb.h"

namespace {

#ifndef RTB_PROJECT_SOURCE_DIR
#define RTB_PROJECT_SOURCE_DIR "."
#endif

std::uint64_t now_ns() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count()
    );
}

bool parse_u64(std::string_view text, std::uint64_t& out) {
    const auto* begin = text.data();
    const auto* end = text.data() + text.size();
    const auto result = std::from_chars(begin, end, out);
    return result.ec == std::errc {} && result.ptr == end;
}

std::string build_framed_request(std::uint64_t request_index) {
    rtb::v1::BidRequest request;
    request.set_request_id("bench-req-" + std::to_string(request_index));
    request.set_tmax_ms(100);
    request.set_country("US");
    request.set_device_type("desktop");
    request.set_ad_slot("homepage-top");
    request.set_auction_type(rtb::v1::AUCTION_TYPE_FIRST_PRICE);

    const std::string payload = request.SerializeAsString();
    std::string framed(sizeof(std::uint32_t) + payload.size(), '\0');
    const std::uint32_t payload_size = htonl(static_cast<std::uint32_t>(payload.size()));
    std::memcpy(framed.data(), &payload_size, sizeof(payload_size));
    std::memcpy(framed.data() + sizeof(std::uint32_t), payload.data(), payload.size());
    return framed;
}

}  // namespace

int main(int argc, char* argv[]) {
    using namespace rtb::engine;

    std::uint64_t warmup = 1000;
    std::uint64_t count = 10000;
    std::uint64_t seed = 12345;
    std::string campaign_data_path = RTB_PROJECT_SOURCE_DIR "/data/sample_campaigns.csv";

    for (int index = 1; index < argc; ++index) {
        const std::string_view arg = argv[index];
        if (arg == "--warmup" && index + 1 < argc) {
            parse_u64(argv[++index], warmup);
        } else if (arg == "--count" && index + 1 < argc) {
            parse_u64(argv[++index], count);
        } else if (arg == "--seed" && index + 1 < argc) {
            parse_u64(argv[++index], seed);
        } else if (arg == "--campaign-data-path" && index + 1 < argc) {
            campaign_data_path = argv[++index];
        } else if (arg == "--help") {
            std::cout << "Usage: rtb_inproc_bench [--warmup N] [--count N] [--seed N] "
                         "[--campaign-data-path PATH]\n";
            return 0;
        }
    }

    GOOGLE_PROTOBUF_VERIFY_VERSION;

    const auto campaign_store = load_campaign_store_snapshot(campaign_data_path);
    if (campaign_store == nullptr) {
        std::cerr << "failed to load campaign store from " << campaign_data_path << '\n';
        google::protobuf::ShutdownProtobufLibrary();
        return 1;
    }

    ParseScratch scratch;
    WorkerRng rng;
    rng.seed(seed);
    ReusableBuffer write_buffer;
    BenchmarkRecorder recorder;

    const std::uint64_t total_iterations = warmup + count;
    for (std::uint64_t request_index = 0; request_index < total_iterations; ++request_index) {
        const std::string framed_request = build_framed_request(request_index + 1);
        const auto payload = std::span<const std::byte>(
            reinterpret_cast<const std::byte*>(framed_request.data() + sizeof(std::uint32_t)),
            framed_request.size() - sizeof(std::uint32_t)
        );

        const std::uint64_t total_start = now_ns();
        ParsedMessage parsed_message;
        RequestTimingBreakdown timing;
        const std::uint64_t parse_start = now_ns();
        const ParseStatus parse_status = parse_bid_request(payload, scratch, parsed_message);
        const std::uint64_t parse_end = now_ns();
        timing.parse_ns = parse_end - parse_start;
        if (parse_status != ParseStatus::kOk) {
            google::protobuf::ShutdownProtobufLibrary();
            return 1;
        }

        HandleRequestTimingBreakdown handle_timing;
        const HandleRequestResult result =
            handle_request(parsed_message, now_ns(), *campaign_store, rng, &handle_timing);
        if (result.status == HandleRequestStatus::kDropConnection) {
            google::protobuf::ShutdownProtobufLibrary();
            return 1;
        }

        timing.normalize_validate_ns = handle_timing.normalize_validate_ns;
        timing.retrieve_ns = handle_timing.retrieve_ns;
        timing.decide_ns = handle_timing.decide_ns;
        timing.response_build_ns = handle_timing.response_build_ns;

        const std::uint64_t stage_start = now_ns();
        if (!stage_response_frame(result.response, write_buffer)) {
            google::protobuf::ShutdownProtobufLibrary();
            return 1;
        }
        const std::uint64_t stage_end = now_ns();
        timing.response_stage_ns = stage_end - stage_start;
        timing.total_internal_ns = stage_end - total_start;

        write_buffer.read_offset = 0;
        write_buffer.write_offset = 0;

        if (request_index >= warmup) {
            recorder.record_request(timing);
        }
    }

    recorder.print_summary(std::cout);
    google::protobuf::ShutdownProtobufLibrary();
    return 0;
}
