#include "rtb/response_framing.h"

#include <arpa/inet.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#include "rtb/config.h"

namespace {

bool test_no_bid_frame() {
    rtb::engine::ReusableBuffer buffer;

    rtb::v1::BidResponse response;
    response.set_request_id("req-no-bid");
    response.set_status(rtb::v1::BID_STATUS_NO_BID);

    if (!rtb::engine::stage_response_frame(response, buffer)) {
        return false;
    }

    std::uint32_t network_length = 0;
    std::memcpy(&network_length, buffer.storage.data(), sizeof(network_length));
    const std::uint32_t payload_length = ntohl(network_length);
    if (payload_length != response.ByteSizeLong() ||
        buffer.read_offset != 0 ||
        buffer.write_offset != sizeof(std::uint32_t) + payload_length) {
        return false;
    }

    rtb::v1::BidResponse decoded;
    return decoded.ParseFromArray(
               buffer.storage.data() + sizeof(std::uint32_t),
               static_cast<int>(payload_length)) &&
           decoded.request_id() == "req-no-bid" &&
           decoded.status() == rtb::v1::BID_STATUS_NO_BID;
}

bool test_bid_frame() {
    rtb::engine::ReusableBuffer buffer;

    rtb::v1::BidResponse response;
    response.set_request_id("req-bid");
    response.set_status(rtb::v1::BID_STATUS_BID);
    response.set_campaign_id("42");
    response.set_creative_id("77");
    response.set_price(1.25);

    if (!rtb::engine::stage_response_frame(response, buffer)) {
        return false;
    }

    std::uint32_t network_length = 0;
    std::memcpy(&network_length, buffer.storage.data(), sizeof(network_length));
    const std::uint32_t payload_length = ntohl(network_length);

    rtb::v1::BidResponse decoded;
    return decoded.ParseFromArray(
               buffer.storage.data() + sizeof(std::uint32_t),
               static_cast<int>(payload_length)) &&
           decoded.request_id() == "req-bid" &&
           decoded.status() == rtb::v1::BID_STATUS_BID &&
           decoded.campaign_id() == "42" &&
           decoded.creative_id() == "77" &&
           decoded.price() == 1.25;
}

bool test_rejects_oversized_frame() {
    rtb::engine::ReusableBuffer buffer;

    rtb::v1::BidResponse response;
    response.set_request_id(std::string(rtb::config::kMaxOutboundFrameSize + 1, 'x'));
    response.set_status(rtb::v1::BID_STATUS_NO_BID);

    return !rtb::engine::stage_response_frame(response, buffer);
}

bool test_rejects_buffer_with_pending_bytes() {
    rtb::engine::ReusableBuffer buffer;
    buffer.reserve(64);
    buffer.write_offset = 8;

    rtb::v1::BidResponse response;
    response.set_request_id("req-pending");
    response.set_status(rtb::v1::BID_STATUS_NO_BID);

    return !rtb::engine::stage_response_frame(response, buffer);
}

}  // namespace

int main() {
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    const bool ok =
        test_no_bid_frame() &&
        test_bid_frame() &&
        test_rejects_oversized_frame() &&
        test_rejects_buffer_with_pending_bytes();

    google::protobuf::ShutdownProtobufLibrary();
    return ok ? 0 : 1;
}
