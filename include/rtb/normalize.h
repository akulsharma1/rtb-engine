#ifndef RTB_NORMALIZE_H_
#define RTB_NORMALIZE_H_

#include <cstdint>
#include <string_view>

#include "rtb/engine_types.h"

namespace rtb::engine {

CountryKey normalize_country_key(std::string_view country);
DeviceTypeKey normalize_device_type_key(std::string_view device_type);
AdSlotKey normalize_ad_slot_key(std::string_view ad_slot);
std::uint64_t compute_deadline_ns(std::uint64_t received_at_ns, std::uint32_t tmax_ms);

RequestContext build_request_context(const ParsedMessage& parsed_message, std::uint64_t received_at_ns);

}  // namespace rtb::engine

#endif  // RTB_NORMALIZE_H_
