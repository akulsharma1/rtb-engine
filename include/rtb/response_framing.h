#ifndef RTB_RESPONSE_FRAMING_H_
#define RTB_RESPONSE_FRAMING_H_

#include "rtb/runtime_types.h"
#include "rtb.pb.h"

namespace rtb::engine {

bool stage_response_frame(const rtb::v1::BidResponse& response, ReusableBuffer& buffer);

}  // namespace rtb::engine

#endif  // RTB_RESPONSE_FRAMING_H_
