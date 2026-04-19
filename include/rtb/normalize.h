#ifndef RTB_NORMALIZE_H_
#define RTB_NORMALIZE_H_

#include <cstdint>

#include "rtb/engine_types.h"

namespace rtb::engine {

RequestContext build_request_context(const ParsedMessage& parsed_message, std::uint64_t received_at_ns);

}  // namespace rtb::engine

#endif  // RTB_NORMALIZE_H_
