#ifndef RTB_DECISION_H_
#define RTB_DECISION_H_

#include <span>

#include "rtb/engine_types.h"
#include "rtb/runtime_types.h"

namespace rtb::engine {

BidDecision make_bid_decision(
    const RequestContext& request_context,
    std::span<const CampaignView> candidates,
    WorkerRng& rng
);

}  // namespace rtb::engine

#endif  // RTB_DECISION_H_
