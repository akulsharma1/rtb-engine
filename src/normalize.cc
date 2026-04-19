#include "rtb/normalize.h"
#include "rtb/config.h"
#include "rtb/constants.h"

#include <algorithm>
#include <cctype>
#include <string_view>

namespace {

// we dont use std::to_upper because it has unnecessary locality checks.
// since we know the data is protocol defined we don't need it and can save overhead.
constexpr unsigned char to_upper_ascii(unsigned char character) {
    return (character >= 'a' && character <= 'z')
        ? static_cast<unsigned char>(character - 32)
        : character;
}

// we assume that we can only accept ascii characters
constexpr rtb::engine::CountryKey normalize_country(std::string_view country) {
    if (country.size() != 2) {
        return {};
    }


    const auto first = to_upper_ascii(static_cast<unsigned char>(country[0]));
    const auto second = to_upper_ascii(static_cast<unsigned char>(country[1]));

    return {
        static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(first) << 8) |
            static_cast<std::uint16_t>(second)
        )
    };
}

rtb::engine::DeviceTypeKey normalize_device_type(std::string_view device_type) {
    if (device_type == rtb::constants::DESKTOP) {
        return rtb::engine::DeviceTypeKey::kDesktop;
    }
    if (device_type == rtb::constants::MOBILE) {
        return rtb::engine::DeviceTypeKey::kMobile;
    }
    if (device_type == rtb::constants::TABLET) {
        return rtb::engine::DeviceTypeKey::kTablet;
    }
    if (rtb::constants::is_connected_tv(device_type)) {
        return rtb::engine::DeviceTypeKey::kConnectedTv;
    }
    return rtb::engine::DeviceTypeKey::kUnknown;
}

rtb::engine::AdSlotKey normalize_ad_slot(std::string_view ad_slot) {
    // currently we just hash the ad slot for a quick search
    // future improvements would be to map the ad slot to some inmemory db for less collisions / wider search space

    std::uint32_t value = 2166136261u;
    for (unsigned char character : ad_slot) {
        value ^= static_cast<std::uint32_t>(character);
        value *= 16777619u;
    }
    return {value};
}

std::uint64_t compute_deadline_ns_impl(std::uint64_t received_at_ns, std::uint32_t tmax_ms) {
    const std::uint64_t raw_budget_ns = static_cast<std::uint64_t>(tmax_ms) * rtb::config::kNsPerMs;
    if (raw_budget_ns <= rtb::config::kResponseSafetyMarginNs) {
        return received_at_ns;
    }
    return received_at_ns + (raw_budget_ns - rtb::config::kResponseSafetyMarginNs);
}

}  // namespace

namespace rtb::engine {

CountryKey normalize_country_key(std::string_view country) {
    return normalize_country(country);
}

DeviceTypeKey normalize_device_type_key(std::string_view device_type) {
    return normalize_device_type(device_type);
}

AdSlotKey normalize_ad_slot_key(std::string_view ad_slot) {
    return normalize_ad_slot(ad_slot);
}

std::uint64_t compute_deadline_ns(std::uint64_t received_at_ns, std::uint32_t tmax_ms) {
    return compute_deadline_ns_impl(received_at_ns, tmax_ms);
}

RequestContext build_request_context(const ParsedMessage& parsed_message, std::uint64_t received_at_ns) {
    RequestContext context {
        .request_id = parsed_message.request_id,
        .country_key = normalize_country_key(parsed_message.country),
        .device_type_key = normalize_device_type_key(parsed_message.device_type),
        .ad_slot_key = normalize_ad_slot_key(parsed_message.ad_slot),
        .auction_type = parsed_message.auction_type,
        .received_at_ns = received_at_ns,
        .deadline_ns = compute_deadline_ns(received_at_ns, parsed_message.tmax_ms),
        .processing_status = parsed_message.processing_status,
        // .no_bid_reason = parsed_message.no_bid_reason,
    };

    if (context.deadline_ns <= context.received_at_ns) {
        context.processing_status = RequestProcessingStatus::kExpired;
        context.no_bid_reason = NoBidReason::kDeadlineExceeded;
    }

    return context;
}

}  // namespace rtb::engine
