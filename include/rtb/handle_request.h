#ifndef RTB_HANDLE_REQUEST_H_
#define RTB_HANDLE_REQUEST_H_

#include <cstdint>

#include "rtb/engine_types.h"
#include "rtb.pb.h"

namespace rtb::engine {

enum class HandleRequestStatus : std::uint8_t {
    kResponseReady = 0,
    kDropConnection = 1,
};

struct HandleRequestResult {
    HandleRequestStatus status = HandleRequestStatus::kResponseReady;
    RequestContext context {};
    BidDecision decision {};
    rtb::v1::BidResponse response;
};

HandleRequestResult handle_request(
    const ParsedMessage& parsed_message,
    std::uint64_t received_at_ns
);

}  // namespace rtb::engine

#endif  // RTB_HANDLE_REQUEST_H_
