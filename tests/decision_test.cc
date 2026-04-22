#include <cmath>
#include <span>
#include <vector>

#include "rtb/config.h"
#include "rtb/decision.h"

namespace {

std::uint64_t next_u64(std::uint64_t& state) {
    std::uint64_t value = state;
    value ^= value << 13;
    value ^= value >> 7;
    value ^= value << 17;
    state = value == 0 ? 1 : value;
    return state;
}

double next_unit_double(std::uint64_t& state) {
    constexpr double kScale = 1.0 / static_cast<double>(1ULL << 53);
    return static_cast<double>(next_u64(state) >> 11) * kScale;
}

std::size_t uniform_index(std::uint64_t& state, std::size_t upper_bound) {
    return upper_bound == 0 ? 0 : static_cast<std::size_t>(next_u64(state) % upper_bound);
}

double generate_expected_bid_price(std::uint64_t& state) {
    return rtb::config::kMinBidPrice +
           (next_unit_double(state) * (rtb::config::kMaxBidPrice - rtb::config::kMinBidPrice));
}

}  // namespace

int main() {
    using namespace rtb::engine;

    const RequestContext request_context {
        .request_id = "req-1",
        .country_key = {1},
        .device_type_key = DeviceTypeKey::kDesktop,
        .ad_slot_key = {42},
        .auction_type = AuctionType::kFirstPrice,
        .received_at_ns = 10,
        .deadline_ns = 1000,
        .processing_status = RequestProcessingStatus::kOk,
        .no_bid_reason = NoBidReason::kNone,
    };

    const CampaignRecord campaigns[] = {
        CampaignRecord {.campaign_id = 10, .creative_offset = 0, .creative_count = 2, .active = true},
        CampaignRecord {.campaign_id = 20, .creative_offset = 0, .creative_count = 2, .active = true},
    };
    const CreativeRecord creatives_a[] = {
        CreativeRecord {.creative_id = 101},
        CreativeRecord {.creative_id = 102},
    };
    const CreativeRecord creatives_b[] = {
        CreativeRecord {.creative_id = 201},
        CreativeRecord {.creative_id = 202},
    };
    const CampaignView candidate_views[] = {
        CampaignView {.campaign = &campaigns[0], .creatives = std::span<const CreativeRecord>(creatives_a, 2)},
        CampaignView {.campaign = &campaigns[1], .creatives = std::span<const CreativeRecord>(creatives_b, 2)},
    };

    WorkerRng rng;
    rng.seed(12345);
    const BidDecision decision = make_bid_decision(
        request_context,
        std::span<const CampaignView>(candidate_views, 2),
        rng
    );

    std::uint64_t expected_state = 12345;
    const std::size_t creative_choice_a = uniform_index(expected_state, 2);
    const double bid_a = generate_expected_bid_price(expected_state);
    const std::size_t creative_choice_b = uniform_index(expected_state, 2);
    const double bid_b = generate_expected_bid_price(expected_state);

    const bool first_wins = bid_a > bid_b;
    const std::uint64_t expected_campaign_id = first_wins ? 10 : 20;
    const std::uint64_t expected_creative_id =
        first_wins ? creatives_a[creative_choice_a].creative_id : creatives_b[creative_choice_b].creative_id;
    const double expected_bid_price = first_wins ? bid_a : bid_b;

    if (!decision.has_bid ||
        decision.no_bid_reason != NoBidReason::kNone ||
        decision.campaign_id != expected_campaign_id ||
        decision.creative_id != expected_creative_id ||
        std::fabs(decision.bid_price - expected_bid_price) > 1e-12 ||
        decision.model_score != 0.0) {
        return 1;
    }

    if (decision.bid_price < rtb::config::kMinBidPrice ||
        decision.bid_price > rtb::config::kMaxBidPrice) {
        return 1;
    }

    const CreativeRecord empty_creatives[] = {
        CreativeRecord {.creative_id = 301},
        CreativeRecord {.creative_id = 302},
    };
    const CampaignRecord empty_creative_campaign {.campaign_id = 30, .creative_offset = 0, .creative_count = 0, .active = true};
    const CampaignView empty_candidate {
        .campaign = &empty_creative_campaign,
        .creatives = std::span<const CreativeRecord>(empty_creatives, 0),
    };
    WorkerRng empty_creative_rng;
    empty_creative_rng.seed(55);
    const BidDecision no_creative_decision = make_bid_decision(
        request_context,
        std::span<const CampaignView>(&empty_candidate, 1),
        empty_creative_rng
    );
    if (no_creative_decision.has_bid ||
        no_creative_decision.no_bid_reason != NoBidReason::kNoEligibleCampaign) {
        return 1;
    }

    WorkerRng empty_rng;
    empty_rng.seed(7);
    const BidDecision no_candidate_decision = make_bid_decision(
        request_context,
        std::span<const CampaignView>(),
        empty_rng
    );
    if (no_candidate_decision.has_bid ||
        no_candidate_decision.no_bid_reason != NoBidReason::kNoEligibleCampaign) {
        return 1;
    }

    return 0;
}
