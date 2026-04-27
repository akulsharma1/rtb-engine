#include "rtb/handle_request.h"

#include "logs/logs.h"
#include "rtb/decision.h"
#include "rtb/normalize.h"
#include "rtb/response_builder.h"

namespace {

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
    WorkerRng& rng
) {
    HandleRequestResult result;
    result.context = build_request_context(parsed_message, received_at_ns);

    validate_request_context(result.context);

    if (result.context.processing_status != RequestProcessingStatus::kOk &&
        result.context.no_bid_reason == NoBidReason::kInvalidRequest) {
        result.status = HandleRequestStatus::kDropConnection;
    }

    if (!result.context.is_eligible()) {
        result.decision = BidDecision {
            .has_bid = false,
            .no_bid_reason = result.context.no_bid_reason,
        };
        result.response = build_bid_response(result.context, result.decision);
        return result;
    }

    const std::vector<CampaignView> candidates = campaign_store.retrieve_candidates(result.context);

    result.decision = make_bid_decision(result.context, candidates, rng);
    result.response = build_bid_response(result.context, result.decision);
    return result;
}

}  // namespace rtb::engine
