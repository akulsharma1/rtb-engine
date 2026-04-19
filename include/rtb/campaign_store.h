#ifndef RTB_CAMPAIGN_STORE_H_
#define RTB_CAMPAIGN_STORE_H_

#include <memory>
#include <string>

#include "rtb/engine_types.h"

namespace rtb::engine {

std::shared_ptr<const CampaignStoreSnapshot> load_campaign_store_snapshot(const std::string& path);

}  // namespace rtb::engine

#endif  // RTB_CAMPAIGN_STORE_H_
