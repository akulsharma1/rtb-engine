#include "rtb/handle_request.h"

#include "rtb/normalize.h"
#include "rtb/response_builder.h"

namespace {

void validate_request_context(rtb::engine::RequestContext& request_context) {
    if (request_context.processing_status != rtb::engine::RequestProcessingStatus::kOk) {
        if (request_context.no_bid_reason == rtb::engine::NoBidReason::kNone) {
            request_context.no_bid_reason = rtb::engine::NoBidReason::kInvalidRequest;
        }
    }
}

rtb::engine::BidDecision make_placeholder_decision(const rtb::engine::RequestContext& request_context) {
    rtb::engine::BidDecision decision;
    decision.has_bid = false;
    decision.no_bid_reason = request_context.is_eligible()
                                 ? rtb::engine::NoBidReason::kNoEligibleCampaign
                                 : request_context.no_bid_reason;
    return decision;
}

}  // namespace

namespace rtb::engine {

HandleRequestResult handle_request(
    const ParsedMessage& parsed_message,
    std::uint64_t received_at_ns
) {
    HandleRequestResult result;
    result.context = build_request_context(parsed_message, received_at_ns);

    validate_request_context(result.context);

    if (result.context.processing_status != RequestProcessingStatus::kOk &&
        result.context.no_bid_reason == NoBidReason::kInvalidRequest) {
        result.status = HandleRequestStatus::kDropConnection;
    }

    result.decision = make_placeholder_decision(result.context);
    result.response = build_bid_response(result.context, result.decision);
    return result;
}

}  // namespace rtb::engine
