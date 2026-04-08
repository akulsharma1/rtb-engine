#include <iostream>

#include "rtb/engine_types.h"
#include "rtb.pb.h"

int main() {
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    rtb::engine::ParsedMessage parsed_message {
        .request_id = "bootstrap-request",
        .tmax_ms = 120,
        .country = "US",
        .device_type = "desktop",
        .ad_slot = "homepage-top",
        .auction_type = rtb::engine::AuctionType::kFirstPrice,
    };


    rtb::engine::RequestContext request_context {
        .request_id = parsed_message.request_id,
        .country_key = {1},
        .device_type_key = rtb::engine::DeviceTypeKey::kDesktop,
        .ad_slot_key = {1001},
        .auction_type = parsed_message.auction_type,
        .received_at_ns = 1'000,
        .deadline_ns = 111'000,
    };

    rtb::v1::BidRequest request;
    request.set_request_id(parsed_message.request_id.data());
    request.set_tmax_ms(parsed_message.tmax_ms);
    request.set_country(parsed_message.country.data());
    request.set_device_type(parsed_message.device_type.data());
    request.set_ad_slot(parsed_message.ad_slot.data());
    request.set_auction_type(rtb::v1::AUCTION_TYPE_FIRST_PRICE);

    std::cout << "rtb_engine bootstrap ready for request "
              << request.request_id()
              << " with deadline "
              << request_context.deadline_ns
              << '\n';

    google::protobuf::ShutdownProtobufLibrary();
    return 0;
}
