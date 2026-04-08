#ifndef RTB_PARSE_H_
#define RTB_PARSE_H_

#include <cstddef>
#include <cstdint>
#include <span>

#include "rtb/engine_types.h"
#include "rtb.pb.h"

namespace rtb::engine {

enum class ParseStatus : std::uint8_t {
    kOk = 0,
    kMalformedProto,
    kMissingRequestId,
    kMissingCountry,
    kMissingDeviceType,
    kMissingAdSlot,
    kInvalidAuctionType,
};

struct ParseScratch {
    rtb::v1::BidRequest request;
};

ParseStatus parse_bid_request(
    std::span<const std::byte> frame,
    ParseScratch& scratch,
    ParsedMessage& out
);

}  // namespace rtb::engine

#endif  // RTB_PARSE_H_
