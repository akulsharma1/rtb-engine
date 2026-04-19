#include "rtb/handle_request.h"
#include "rtb/campaign_store.h"

int main() {
    using namespace rtb::engine;

    const auto campaign_store = load_campaign_store_snapshot(RTB_PROJECT_SOURCE_DIR "/data/sample_campaigns.csv");
    if (campaign_store == nullptr) {
        return 1;
    }

    const ParsedMessage valid {
        .request_id = "req-1",
        .tmax_ms = 100,
        .country = "US",
        .device_type = "desktop",
        .ad_slot = "homepage-top",
        .auction_type = AuctionType::kFirstPrice,
    };

    const HandleRequestResult valid_result = handle_request(valid, 5'000'000, *campaign_store);
    if (valid_result.status != HandleRequestStatus::kResponseReady ||
        valid_result.context.processing_status != RequestProcessingStatus::kOk ||
        valid_result.decision.has_bid ||
        valid_result.decision.no_bid_reason != NoBidReason::kFiltered ||
        valid_result.response.status() != rtb::v1::BID_STATUS_NO_BID) {
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

    const HandleRequestResult expired_result = handle_request(expired, 8'000'000, *campaign_store);
    if (expired_result.status != HandleRequestStatus::kResponseReady ||
        expired_result.context.processing_status != RequestProcessingStatus::kExpired ||
        expired_result.decision.no_bid_reason != NoBidReason::kDeadlineExceeded ||
        expired_result.response.status() != rtb::v1::BID_STATUS_NO_BID) {
        return 1;
    }

    const ParsedMessage no_match {
        .request_id = "req-3",
        .tmax_ms = 100,
        .country = "US",
        .device_type = "tablet",
        .ad_slot = "homepage-top",
        .auction_type = AuctionType::kFirstPrice,
    };

    const HandleRequestResult no_match_result = handle_request(no_match, 9'000'000, *campaign_store);
    if (no_match_result.decision.no_bid_reason != NoBidReason::kNoEligibleCampaign) {
        return 1;
    }

    return 0;
}
