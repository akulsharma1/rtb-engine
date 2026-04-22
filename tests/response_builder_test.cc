#include "rtb/response_builder.h"

int main() {
    using namespace rtb::engine;

    const RequestContext context {
        .request_id = "req-1",
        .country_key = {1},
        .device_type_key = DeviceTypeKey::kDesktop,
        .ad_slot_key = {42},
        .auction_type = AuctionType::kFirstPrice,
        .received_at_ns = 1'000,
        .deadline_ns = 2'000,
    };

    const BidDecision decision {
        .has_bid = false,
        .no_bid_reason = NoBidReason::kNoEligibleCampaign,
    };

    const rtb::v1::BidResponse response = build_bid_response(context, decision);
    if (response.request_id() != "req-1" ||
        response.status() != rtb::v1::BID_STATUS_NO_BID ||
        !response.campaign_id().empty() ||
        !response.creative_id().empty() ||
        response.price() != 0.0) {
        return 1;
    }

    const BidDecision bid_decision {
        .has_bid = true,
        .no_bid_reason = NoBidReason::kNone,
        .campaign_id = 42,
        .creative_id = 77,
        .model_score = 0.0,
        .bid_price = 1.75,
    };

    const rtb::v1::BidResponse bid_response = build_bid_response(context, bid_decision);
    if (bid_response.request_id() != "req-1" ||
        bid_response.status() != rtb::v1::BID_STATUS_BID ||
        bid_response.campaign_id() != "42" ||
        bid_response.creative_id() != "77" ||
        bid_response.price() != 1.75) {
        return 1;
    }

    return 0;
}
