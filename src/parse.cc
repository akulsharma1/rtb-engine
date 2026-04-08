#include "rtb/parse.h"
#include "rtb.pb.h"
#include "rtb/engine_types.h"

#include <limits>

namespace {

rtb::engine::AuctionType map_auction_type(rtb::v1::AuctionType auction_type) {
    if (auction_type == rtb::v1::AUCTION_TYPE_FIRST_PRICE) {
        return rtb::engine::AuctionType::kFirstPrice;
    }
    if (auction_type == rtb::v1::AUCTION_TYPE_SECOND_PRICE) {
        return rtb::engine::AuctionType::kSecondPrice;
    }
    return rtb::engine::AuctionType::kUnspecified;
}

}  // namespace

namespace rtb::engine {

ParseStatus parse_bid_request(
    std::span<const std::byte> frame,
    ParseScratch& scratch,
    ParsedMessage& out
) {
    out = {};
    scratch.request.Clear();

    if (frame.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return ParseStatus::kMalformedProto;
    }

    if (!scratch.request.ParseFromArray(frame.data(), static_cast<int>(frame.size()))) {
        return ParseStatus::kMalformedProto;
    }

    if (scratch.request.request_id().empty()) {
        return ParseStatus::kMissingRequestId;
    }
    if (scratch.request.country().empty()) {
        return ParseStatus::kMissingCountry;
    }
    if (scratch.request.device_type().empty()) {
        return ParseStatus::kMissingDeviceType;
    }
    if (scratch.request.ad_slot().empty()) {
        return ParseStatus::kMissingAdSlot;
    }

    const AuctionType auction_type = map_auction_type(scratch.request.auction_type());
    if (auction_type == AuctionType::kUnspecified) {
        return ParseStatus::kInvalidAuctionType;
    }

    out.request_id = scratch.request.request_id();
    out.tmax_ms = scratch.request.tmax_ms();
    out.country = scratch.request.country();
    out.device_type = scratch.request.device_type();
    out.ad_slot = scratch.request.ad_slot();
    out.auction_type = auction_type;
    out.processing_status = RequestProcessingStatus::kOk;
    out.no_bid_reason = NoBidReason::kNone;

    return ParseStatus::kOk;
}

}  // namespace rtb::engine
