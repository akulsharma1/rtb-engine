#include "rtb/campaign_store.h"
#include "rtb/normalize.h"
#include "rtb/retrieve.h"

int main() {
    using namespace rtb::engine;

    const auto snapshot = load_campaign_store_snapshot(RTB_PROJECT_SOURCE_DIR "/data/sample_campaigns.csv");
    if (snapshot == nullptr) {
        return 1;
    }

    const RequestContext matching_request {
        .request_id = "req-1",
        .country_key = normalize_country_key("US"),
        .device_type_key = DeviceTypeKey::kDesktop,
        .ad_slot_key = normalize_ad_slot_key("homepage-top"),
        .auction_type = AuctionType::kFirstPrice,
        .received_at_ns = 10,
        .deadline_ns = 1000,
        .processing_status = RequestProcessingStatus::kOk,
        .no_bid_reason = NoBidReason::kNone,
    };

    const auto matching_candidates = snapshot->retrieve_candidates(matching_request);
    if (matching_candidates.size() != 1 ||
        !matching_candidates.front().valid() ||
        matching_candidates.front().campaign->campaign_id != 1 ||
        matching_candidates.front().creatives.size() != 2) {
        return 1;
    }

    const RequestContext no_match_request {
        .request_id = "req-2",
        .country_key = normalize_country_key("US"),
        .device_type_key = DeviceTypeKey::kTablet,
        .ad_slot_key = normalize_ad_slot_key("homepage-top"),
        .auction_type = AuctionType::kFirstPrice,
        .received_at_ns = 10,
        .deadline_ns = 1000,
        .processing_status = RequestProcessingStatus::kOk,
        .no_bid_reason = NoBidReason::kNone,
    };

    if (!snapshot->retrieve_candidates(no_match_request).empty()) {
        return 1;
    }

    const RequestContext sports_request {
        .request_id = "req-3",
        .country_key = normalize_country_key("US"),
        .device_type_key = DeviceTypeKey::kDesktop,
        .ad_slot_key = normalize_ad_slot_key("sports-sidebar"),
        .auction_type = AuctionType::kFirstPrice,
        .received_at_ns = 10,
        .deadline_ns = 1000,
        .processing_status = RequestProcessingStatus::kOk,
        .no_bid_reason = NoBidReason::kNone,
    };

    const auto sports_candidates = snapshot->retrieve_candidates(sports_request);
    if (sports_candidates.size() != 1 ||
        sports_candidates.front().campaign->campaign_id != 4 ||
        sports_candidates.front().creatives.size() != 2) {
        return 1;
    }

    const RequestContext expired_request {
        .request_id = "req-4",
        .country_key = normalize_country_key("US"),
        .device_type_key = DeviceTypeKey::kDesktop,
        .ad_slot_key = normalize_ad_slot_key("homepage-top"),
        .auction_type = AuctionType::kFirstPrice,
        .received_at_ns = 10,
        .deadline_ns = 10,
        .processing_status = RequestProcessingStatus::kExpired,
        .no_bid_reason = NoBidReason::kDeadlineExceeded,
    };

    if (!snapshot->retrieve_candidates(expired_request).empty()) {
        return 1;
    }

    return 0;
}
