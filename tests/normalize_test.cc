#include "rtb/normalize.h"

int main() {
    using namespace rtb::engine;

    const ParsedMessage valid {
        .request_id = "req-1",
        .tmax_ms = 100,
        .country = "US",
        .device_type = "desktop",
        .ad_slot = "homepage-top",
        .auction_type = AuctionType::kFirstPrice,
    };

    const RequestContext valid_context = build_request_context(valid, 10'000'000);
    if (valid_context.request_id != "req-1" ||
        valid_context.device_type_key != DeviceTypeKey::kDesktop ||
        valid_context.processing_status != RequestProcessingStatus::kOk ||
        valid_context.no_bid_reason != NoBidReason::kNone ||
        valid_context.deadline_ns <= valid_context.received_at_ns) {
        return 1;
    }

    const ParsedMessage expired {
        .request_id = "req-2",
        .tmax_ms = 0,
        .country = "US",
        .device_type = "desktop",
        .ad_slot = "homepage-top",
        .auction_type = AuctionType::kFirstPrice,
    };

    const RequestContext expired_context = build_request_context(expired, 20'000'000);
    if (expired_context.processing_status != RequestProcessingStatus::kExpired ||
        expired_context.no_bid_reason != NoBidReason::kDeadlineExceeded) {
        return 1;
    }

    return 0;
}
