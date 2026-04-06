#include "rtb/engine_types.h"
#include "rtb/runtime_types.h"

int main() {
    using namespace rtb::engine;

    ReusableBuffer buffer {
        .storage = std::vector<std::byte>(4096),
        .read_offset = 0,
        .write_offset = 128,
    };

    ConnectionState connection {
        .fd = 7,
        .connection_id = 11,
        .request_sequence = 3,
        .read_buffer = std::move(buffer),
        .write_buffer = ReusableBuffer {.storage = std::vector<std::byte>(2048)},
    };

    ParsedMessage parsed {
        .request_id = "req-1",
        .tmax_ms = 100,
        .country = "US",
        .device_type = "desktop",
        .ad_slot = "homepage-top",
        .auction_type = AuctionType::kFirstPrice,
    };

    RequestContext context {
        .request_id = parsed.request_id,
        .country_key = {1},
        .device_type_key = DeviceTypeKey::kDesktop,
        .ad_slot_key = {1001},
        .auction_type = parsed.auction_type,
        .received_at_ns = 1'000,
        .deadline_ns = 100'000,
    };

    CampaignRecord campaign {
        .campaign_id = 42,
        .country_key = context.country_key,
        .device_type_key = context.device_type_key,
        .ad_slot_key = context.ad_slot_key,
        .value_per_action = 2.5,
        .aggressiveness = 1.2,
        .creative_offset = 0,
        .creative_count = 1,
        .active = true,
    };

    CreativeRecord creative {
        .creative_id = 77,
        .ad_slot_key = context.ad_slot_key,
        .active = true,
    };

    CampaignStoreSnapshot snapshot;
    snapshot.campaigns.push_back(campaign);
    snapshot.creatives.push_back(creative);
    snapshot.campaigns_by_country[context.country_key.value].push_back(0);
    snapshot.campaigns_by_ad_slot[context.ad_slot_key.value].push_back(0);

    CampaignView view {
        .campaign = &snapshot.campaigns.front(),
        .creatives = std::span<const CreativeRecord>(snapshot.creatives.data(), snapshot.creatives.size()),
    };

    BidDecision decision {
        .has_bid = true,
        .campaign_id = view.campaign->campaign_id,
        .creative_id = view.creatives.front().creative_id,
        .model_score = 0.8,
        .bid_price = 2.4,
    };

    return connection.read_buffer.readable_bytes() == 128 &&
                   context.is_eligible() &&
                   view.valid() &&
                   decision.has_bid
               ? 0
               : 1;
}
