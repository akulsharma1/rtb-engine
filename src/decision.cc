#include "rtb/decision.h"

#include <limits>

#include "rtb/config.h"

namespace {

const rtb::engine::CreativeRecord* choose_random_creative(
    std::span<const rtb::engine::CreativeRecord> creatives,
    rtb::engine::WorkerRng& rng
) {
    if (creatives.empty()) {
        return nullptr;
    }

    return &creatives[rng.uniform_index(creatives.size())];
}

double generate_bid_price(rtb::engine::WorkerRng& rng) {
    const double unit = rng.next_unit_double();
    return rtb::config::kMinBidPrice +
           (unit * (rtb::config::kMaxBidPrice - rtb::config::kMinBidPrice));
}

}  // namespace

namespace rtb::engine {

BidDecision make_bid_decision(
    const RequestContext& request_context,
    std::span<const CampaignView> candidates,
    WorkerRng& rng
) {
    if (!request_context.is_eligible()) {
        return BidDecision {
            .has_bid = false,
            .no_bid_reason = request_context.no_bid_reason,
        };
    }

    if (candidates.empty()) {
        return BidDecision {
            .has_bid = false,
            .no_bid_reason = NoBidReason::kNoEligibleCampaign,
        };
    }

    BidDecision best_decision {
        .has_bid = false,
        .no_bid_reason = NoBidReason::kNoEligibleCampaign,
    };
    double best_bid_price = -std::numeric_limits<double>::infinity();

    for (const CampaignView& candidate : candidates) {
        if (!candidate.valid()) {
            continue;
        }

        const CreativeRecord* creative = choose_random_creative(candidate.creatives, rng);
        if (creative == nullptr) {
            continue;
        }

        const double bid_price = generate_bid_price(rng);
        if (bid_price <= best_bid_price) {
            continue;
        }

        best_bid_price = bid_price;
        best_decision = BidDecision {
            .has_bid = true,
            .no_bid_reason = NoBidReason::kNone,
            .campaign_id = candidate.campaign->campaign_id,
            .creative_id = creative->creative_id,
            .model_score = 0.0,
            .bid_price = bid_price,
        };
    }

    return best_decision;
}

}  // namespace rtb::engine
