#ifndef RTB_RETRIEVE_H_
#define RTB_RETRIEVE_H_

#include <vector>

#include "rtb/engine_types.h"

namespace rtb::engine {

std::vector<CampaignView> retrieve_candidates(
    const CampaignStoreSnapshot& campaign_store,
    const RequestContext& request_context
);

}  // namespace rtb::engine

#endif  // RTB_RETRIEVE_H_
