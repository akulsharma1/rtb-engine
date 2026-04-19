#include "rtb/retrieve.h"

#include <algorithm>

namespace {

std::span<const rtb::engine::CreativeRecord> campaign_creatives(
    const rtb::engine::CampaignStoreSnapshot& campaign_store,
    const rtb::engine::CampaignRecord& campaign
) {
    return std::span<const rtb::engine::CreativeRecord>(
        campaign_store.creatives.data() + campaign.creative_offset,
        campaign.creative_count
    );
}

bool has_active_creative(
    const rtb::engine::CampaignStoreSnapshot& campaign_store,
    const rtb::engine::CampaignRecord& campaign
) {
    const auto creatives = campaign_creatives(campaign_store, campaign);
    return std::any_of(
        creatives.begin(),
        creatives.end(),
        [](const rtb::engine::CreativeRecord& creative) {
            return creative.active;
        }
    );
}

bool matches_request(
    const rtb::engine::CampaignStoreSnapshot& campaign_store,
    const rtb::engine::RequestContext& request_context,
    const rtb::engine::CampaignRecord& campaign
) {
    if (!campaign.active ||
        campaign.country_key.value != request_context.country_key.value ||
        campaign.device_type_key != request_context.device_type_key ||
        campaign.ad_slot_key.value != request_context.ad_slot_key.value) {
        return false;
    }

    return has_active_creative(campaign_store, campaign);
}

const std::vector<std::uint32_t>* choose_seed_bucket(
    const rtb::engine::CampaignStoreSnapshot& campaign_store,
    const rtb::engine::RequestContext& request_context
) {
    const auto country_it = campaign_store.campaigns_by_country.find(request_context.country_key.value);
    const auto slot_it = campaign_store.campaigns_by_ad_slot.find(request_context.ad_slot_key.value);

    if (country_it == campaign_store.campaigns_by_country.end() ||
        slot_it == campaign_store.campaigns_by_ad_slot.end()) {
        return nullptr;
    }

    // HFT note: this is the candidate-source seam where ANN or another approximate
    // prefilter can plug in later. The downstream exact-match filtering can stay unchanged.
    return country_it->second.size() <= slot_it->second.size()
               ? &country_it->second
               : &slot_it->second;
}

}  // namespace

namespace rtb::engine {

std::vector<CampaignView> CampaignStoreSnapshot::retrieve_candidates(
    const RequestContext& request_context
) const {
    return rtb::engine::retrieve_candidates(*this, request_context);
}

std::vector<CampaignView> retrieve_candidates(
    const CampaignStoreSnapshot& campaign_store,
    const RequestContext& request_context
) {
    std::vector<CampaignView> candidates;
    if (!request_context.is_eligible()) {
        return candidates;
    }

    // initial filtering return the smaller of country index and ad slot
    const auto* seed_bucket = choose_seed_bucket(campaign_store, request_context);
    if (seed_bucket == nullptr) {
        return candidates;
    }

    // HFT note: this per-request vector allocation is acceptable for v1 correctness,
    // but a worker-local scratch vector or fixed-capacity small buffer would reduce churn.
    candidates.reserve(seed_bucket->size());

    for (const std::uint32_t campaign_index : *seed_bucket) {
        if (campaign_index >= campaign_store.campaigns.size()) {
            continue;
        }

        const CampaignRecord& campaign = campaign_store.campaigns[campaign_index];
        if (!matches_request(campaign_store, request_context, campaign)) {
            continue;
        }

        candidates.push_back(CampaignView {
            .campaign = &campaign,
            .creatives = campaign_creatives(campaign_store, campaign),
        });
    }

    return candidates;
}

}  // namespace rtb::engine
