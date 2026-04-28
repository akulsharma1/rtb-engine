#include "rtb/handle_request.h"

#include <chrono>

#include "logs/logs.h"
#include "rtb/decision.h"
#include "rtb/normalize.h"
#include "rtb/response_builder.h"

namespace {

std::uint64_t now_ns() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count()
    );
}

void validate_request_context(rtb::engine::RequestContext& request_context) {
    if (request_context.country_key.value == 0 ||
        request_context.device_type_key == rtb::engine::DeviceTypeKey::kUnknown ||
        request_context.ad_slot_key.value == 0 ||
        request_context.auction_type == rtb::engine::AuctionType::kUnspecified) {
        
        rtb::logger::LOG_ERROR("Validate Request Context: setting no bid reason to invalid request");
        request_context.processing_status = rtb::engine::RequestProcessingStatus::kInvalid;
        request_context.no_bid_reason = rtb::engine::NoBidReason::kInvalidRequest;
        return;
    }

    if (request_context.processing_status != rtb::engine::RequestProcessingStatus::kOk) {
        if (request_context.no_bid_reason == rtb::engine::NoBidReason::kNone) {
            rtb::logger::LOG_ERROR("Validate Request Context: setting no bid reason to invalid request");
            request_context.no_bid_reason = rtb::engine::NoBidReason::kInvalidRequest;
        }
    }
}

}  // namespace

namespace rtb::engine {

HandleRequestResult handle_request(
    const ParsedMessage& parsed_message,
    std::uint64_t received_at_ns,
    const CampaignStoreSnapshot& campaign_store,
    WorkerRng& rng,
    HandleRequestTimingBreakdown* timing
) {
    HandleRequestResult result;

    const std::uint64_t normalize_start = now_ns();
    result.context = build_request_context(parsed_message, received_at_ns);
    validate_request_context(result.context);
    const std::uint64_t normalize_end = now_ns();
    if (timing != nullptr) {
        timing->normalize_validate_ns = normalize_end - normalize_start;
    }

    if (result.context.processing_status != RequestProcessingStatus::kOk &&
        result.context.no_bid_reason == NoBidReason::kInvalidRequest) {
        result.status = HandleRequestStatus::kDropConnection;
    }

    if (!result.context.is_eligible()) {
        result.decision = BidDecision {
            .has_bid = false,
            .no_bid_reason = result.context.no_bid_reason,
        };
        const std::uint64_t response_build_start = now_ns();
        result.response = build_bid_response(result.context, result.decision);
        const std::uint64_t response_build_end = now_ns();
        if (timing != nullptr) {
            timing->response_build_ns = response_build_end - response_build_start;
        }
        return result;
    }

    const std::uint64_t retrieve_start = now_ns();
    const std::vector<CampaignView> candidates = campaign_store.retrieve_candidates(result.context);
    const std::uint64_t retrieve_end = now_ns();
    if (timing != nullptr) {
        timing->retrieve_ns = retrieve_end - retrieve_start;
    }

    const std::uint64_t decide_start = now_ns();
    result.decision = make_bid_decision(result.context, candidates, rng);
    const std::uint64_t decide_end = now_ns();
    if (timing != nullptr) {
        timing->decide_ns = decide_end - decide_start;
    }

    const std::uint64_t response_build_start = now_ns();
    result.response = build_bid_response(result.context, result.decision);
    const std::uint64_t response_build_end = now_ns();
    if (timing != nullptr) {
        timing->response_build_ns = response_build_end - response_build_start;
    }
    return result;
}

}  // namespace rtb::engine
