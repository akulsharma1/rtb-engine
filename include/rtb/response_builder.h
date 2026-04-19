#ifndef RTB_RESPONSE_BUILDER_H_
#define RTB_RESPONSE_BUILDER_H_

#include "rtb/engine_types.h"
#include "rtb.pb.h"

namespace rtb::engine {

rtb::v1::BidResponse build_bid_response(
    const RequestContext& request_context,
    const BidDecision& decision
);

}  // namespace rtb::engine

#endif  // RTB_RESPONSE_BUILDER_H_
