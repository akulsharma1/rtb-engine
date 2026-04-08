#include "rtb/parse.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace {

std::vector<std::byte> serialize_request(const rtb::v1::BidRequest& request) {
    const std::string encoded = request.SerializeAsString();
    std::vector<std::byte> bytes(encoded.size());
    for (std::size_t index = 0; index < encoded.size(); ++index) {
        bytes[index] = static_cast<std::byte>(encoded[index]);
    }
    return bytes;
}

bool test_valid_request() {
    rtb::engine::ParseScratch scratch;
    rtb::engine::ParsedMessage parsed;

    rtb::v1::BidRequest request;
    request.set_request_id("req-123");
    request.set_tmax_ms(120);
    request.set_country("US");
    request.set_device_type("desktop");
    request.set_ad_slot("homepage-top");
    request.set_auction_type(rtb::v1::AUCTION_TYPE_FIRST_PRICE);

    const std::vector<std::byte> encoded = serialize_request(request);
    const auto status =
        rtb::engine::parse_bid_request(std::span<const std::byte>(encoded.data(), encoded.size()), scratch, parsed);

    return status == rtb::engine::ParseStatus::kOk &&
           parsed.request_id == "req-123" &&
           parsed.tmax_ms == 120 &&
           parsed.country == "US" &&
           parsed.device_type == "desktop" &&
           parsed.ad_slot == "homepage-top" &&
           parsed.auction_type == rtb::engine::AuctionType::kFirstPrice &&
           parsed.request_id.data() == scratch.request.request_id().data();
}

bool test_malformed_proto() {
    rtb::engine::ParseScratch scratch;
    rtb::engine::ParsedMessage parsed;

    const std::vector<std::byte> encoded = {
        std::byte {0xFF},
        std::byte {0x01},
        std::byte {0xAB},
        std::byte {0x7E},
    };

    return rtb::engine::parse_bid_request(
               std::span<const std::byte>(encoded.data(), encoded.size()),
               scratch,
               parsed
           ) == rtb::engine::ParseStatus::kMalformedProto;
}

bool test_missing_request_id() {
    rtb::engine::ParseScratch scratch;
    rtb::engine::ParsedMessage parsed;

    rtb::v1::BidRequest request;
    request.set_tmax_ms(120);
    request.set_country("US");
    request.set_device_type("desktop");
    request.set_ad_slot("homepage-top");
    request.set_auction_type(rtb::v1::AUCTION_TYPE_FIRST_PRICE);

    const std::vector<std::byte> encoded = serialize_request(request);
    return rtb::engine::parse_bid_request(
               std::span<const std::byte>(encoded.data(), encoded.size()),
               scratch,
               parsed
           ) == rtb::engine::ParseStatus::kMissingRequestId;
}

bool test_missing_country() {
    rtb::engine::ParseScratch scratch;
    rtb::engine::ParsedMessage parsed;

    rtb::v1::BidRequest request;
    request.set_request_id("req-123");
    request.set_tmax_ms(120);
    request.set_device_type("desktop");
    request.set_ad_slot("homepage-top");
    request.set_auction_type(rtb::v1::AUCTION_TYPE_FIRST_PRICE);

    const std::vector<std::byte> encoded = serialize_request(request);
    return rtb::engine::parse_bid_request(
               std::span<const std::byte>(encoded.data(), encoded.size()),
               scratch,
               parsed
           ) == rtb::engine::ParseStatus::kMissingCountry;
}

bool test_missing_device_type() {
    rtb::engine::ParseScratch scratch;
    rtb::engine::ParsedMessage parsed;

    rtb::v1::BidRequest request;
    request.set_request_id("req-123");
    request.set_tmax_ms(120);
    request.set_country("US");
    request.set_ad_slot("homepage-top");
    request.set_auction_type(rtb::v1::AUCTION_TYPE_FIRST_PRICE);

    const std::vector<std::byte> encoded = serialize_request(request);
    return rtb::engine::parse_bid_request(
               std::span<const std::byte>(encoded.data(), encoded.size()),
               scratch,
               parsed
           ) == rtb::engine::ParseStatus::kMissingDeviceType;
}

bool test_missing_ad_slot() {
    rtb::engine::ParseScratch scratch;
    rtb::engine::ParsedMessage parsed;

    rtb::v1::BidRequest request;
    request.set_request_id("req-123");
    request.set_tmax_ms(120);
    request.set_country("US");
    request.set_device_type("desktop");
    request.set_auction_type(rtb::v1::AUCTION_TYPE_FIRST_PRICE);

    const std::vector<std::byte> encoded = serialize_request(request);
    return rtb::engine::parse_bid_request(
               std::span<const std::byte>(encoded.data(), encoded.size()),
               scratch,
               parsed
           ) == rtb::engine::ParseStatus::kMissingAdSlot;
}

bool test_invalid_auction_type() {
    rtb::engine::ParseScratch scratch;
    rtb::engine::ParsedMessage parsed;

    rtb::v1::BidRequest request;
    request.set_request_id("req-123");
    request.set_tmax_ms(120);
    request.set_country("US");
    request.set_device_type("desktop");
    request.set_ad_slot("homepage-top");
    request.set_auction_type(rtb::v1::AUCTION_TYPE_UNSPECIFIED);

    const std::vector<std::byte> encoded = serialize_request(request);
    return rtb::engine::parse_bid_request(
               std::span<const std::byte>(encoded.data(), encoded.size()),
               scratch,
               parsed
           ) == rtb::engine::ParseStatus::kInvalidAuctionType;
}

}  // namespace

int main() {
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    const bool ok = test_valid_request() &&
                    test_malformed_proto() &&
                    test_missing_request_id() &&
                    test_missing_country() &&
                    test_missing_device_type() &&
                    test_missing_ad_slot() &&
                    test_invalid_auction_type();

    google::protobuf::ShutdownProtobufLibrary();
    return ok ? 0 : 1;
}
