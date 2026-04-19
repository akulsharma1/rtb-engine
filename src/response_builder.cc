#include "rtb/response_builder.h"

namespace {

rtb::v1::BidStatus to_proto_status(const rtb::engine::BidDecision& decision) {
    return decision.has_bid ? rtb::v1::BID_STATUS_BID : rtb::v1::BID_STATUS_NO_BID;
}

}  // namespace

namespace rtb::engine {

rtb::v1::BidResponse build_bid_response(
    const RequestContext& request_context,
    const BidDecision& decision
) {
    rtb::v1::BidResponse response;
    response.set_request_id(request_context.request_id.data(), static_cast<int>(request_context.request_id.size()));
    response.set_status(to_proto_status(decision));

    if (decision.has_bid) {
        response.set_campaign_id(std::to_string(decision.campaign_id));
        response.set_creative_id(std::to_string(decision.creative_id));
        response.set_price(decision.bid_price);
    }

    return response;
}

}  // namespace rtb::engine
