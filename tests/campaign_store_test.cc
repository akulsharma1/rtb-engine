#include "rtb/campaign_store.h"
#include "rtb/normalize.h"

int main() {
    using namespace rtb::engine;

    const auto snapshot = load_campaign_store_snapshot(RTB_PROJECT_SOURCE_DIR "/data/sample_campaigns.csv");
    if (snapshot == nullptr) {
        return 1;
    }

    if (snapshot->campaigns.size() != 4 || snapshot->creatives.size() != 6) {
        return 1;
    }

    const auto us_it = snapshot->campaigns_by_country.find(normalize_country_key("US").value);
    const auto homepage_it = snapshot->campaigns_by_ad_slot.find(normalize_ad_slot_key("homepage-top").value);
    const auto sports_it = snapshot->campaigns_by_ad_slot.find(normalize_ad_slot_key("sports-sidebar").value);

    if (us_it == snapshot->campaigns_by_country.end() ||
        homepage_it == snapshot->campaigns_by_ad_slot.end() ||
        sports_it == snapshot->campaigns_by_ad_slot.end()) {
        return 1;
    }

    if (us_it->second.size() != 3 || homepage_it->second.size() != 3 || sports_it->second.size() != 1) {
        return 1;
    }

    const CampaignRecord& first = snapshot->campaigns.front();
    if (first.campaign_id != 1 || first.creative_count != 2 || !first.active) {
        return 1;
    }

    return 0;
}
