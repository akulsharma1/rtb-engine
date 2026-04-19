#ifndef RTB_ENGINE_TYPES_H_
#define RTB_ENGINE_TYPES_H_

#include <cstdint>
#include <span>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace rtb::engine {

enum class AuctionType : std::uint8_t {
    kUnspecified = 0,
    kFirstPrice = 1,
    kSecondPrice = 2,
};

enum class DeviceTypeKey : std::uint8_t {
    kUnknown = 0,
    kDesktop = 1,
    kMobile = 2,
    kTablet = 3,
    kConnectedTv = 4,
};

struct CountryKey {
    std::uint16_t value = 0;
};

struct AdSlotKey {
    std::uint32_t value = 0;
};

enum class NoBidReason : std::uint8_t {
    kNone = 0,
    kInvalidRequest = 1,
    kDeadlineExceeded = 2,
    kNoEligibleCampaign = 3,
    kNoEligibleCreative = 4,
    kFiltered = 5,
};

enum class RequestProcessingStatus : std::uint8_t {
    kOk = 0,
    kInvalid = 1,
    kExpired = 2,
};

struct ParsedMessage {
    std::string_view request_id;
    std::uint32_t tmax_ms = 0;
    std::string_view country;
    std::string_view device_type;
    std::string_view ad_slot;
    AuctionType auction_type = AuctionType::kUnspecified;
    RequestProcessingStatus processing_status = RequestProcessingStatus::kOk;
    // NoBidReason no_bid_reason = NoBidReason::kNone;
};

struct RequestContext {
    std::string_view request_id;
    CountryKey country_key {};
    DeviceTypeKey device_type_key = DeviceTypeKey::kUnknown;
    AdSlotKey ad_slot_key {};
    AuctionType auction_type = AuctionType::kUnspecified;
    std::uint64_t received_at_ns = 0;
    std::uint64_t deadline_ns = 0;
    RequestProcessingStatus processing_status = RequestProcessingStatus::kOk;
    NoBidReason no_bid_reason = NoBidReason::kNone;

    [[nodiscard]] bool is_eligible() const noexcept {
        return processing_status == RequestProcessingStatus::kOk &&
               no_bid_reason == NoBidReason::kNone;
    }
};

struct CampaignRecord {
    std::uint64_t campaign_id = 0;
    CountryKey country_key {};
    DeviceTypeKey device_type_key = DeviceTypeKey::kUnknown;
    AdSlotKey ad_slot_key {};
    double value_per_action = 0.0;
    double aggressiveness = 1.0;
    std::uint32_t creative_offset = 0;
    std::uint32_t creative_count = 0;
    bool active = false;
};

struct CreativeRecord {
    std::uint64_t creative_id = 0;
    AdSlotKey ad_slot_key {};
    bool active = false;
};

struct CampaignStoreSnapshot {
    std::vector<CampaignRecord> campaigns;
    std::vector<CreativeRecord> creatives;
    std::unordered_map<std::uint16_t, std::vector<std::uint32_t>> campaigns_by_country;
    std::unordered_map<std::uint32_t, std::vector<std::uint32_t>> campaigns_by_ad_slot;
};

struct CampaignView {
    const CampaignRecord* campaign = nullptr;
    std::span<const CreativeRecord> creatives {};

    [[nodiscard]] bool valid() const noexcept {
        return campaign != nullptr;
    }
};

struct BidDecision {
    bool has_bid = false;
    NoBidReason no_bid_reason = NoBidReason::kNone;
    std::uint64_t campaign_id = 0;
    std::uint64_t creative_id = 0;
    double model_score = 0.0;
    double bid_price = 0.0;
};

}  // namespace rtb::engine

#endif  // RTB_ENGINE_TYPES_H_
