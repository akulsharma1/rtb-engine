#include "rtb/campaign_store.h"

#include <charconv>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "logs/logs.h"
#include "rtb/constants.h"
#include "rtb/normalize.h"

namespace {

// remove leading/trailing spaces
std::string trim_copy(std::string_view value) {
    std::size_t start = 0;
    while (start < value.size() && (value[start] == ' ' || value[start] == '\t' || value[start] == '\r')) {
        ++start;
    }

    std::size_t end = value.size();
    while (end > start &&
           (value[end - 1] == ' ' || value[end - 1] == '\t' || value[end - 1] == '\r')) {
        --end;
    }

    return std::string(value.substr(start, end - start));
}

// split line on commas and trim each field
std::vector<std::string> split_csv_line(std::string_view line) {
    std::vector<std::string> fields;
    std::size_t start = 0;

    while (start <= line.size()) {
        const std::size_t delimiter = line.find(rtb::constants::kCsvDelimiter, start);
        if (delimiter == std::string_view::npos) {
            fields.push_back(trim_copy(line.substr(start)));
            break;
        }

        fields.push_back(trim_copy(line.substr(start, delimiter - start)));
        start = delimiter + 1;
    }

    return fields;
}

// parse unsigned integers
bool parse_u64(std::string_view value, std::uint64_t& out) {
    const char* begin = value.data();
    const char* end = value.data() + value.size();
    const auto [ptr, ec] = std::from_chars(begin, end, out);
    return ec == std::errc {} && ptr == end;
}


bool parse_double_value(std::string_view value, double& out) {
    std::stringstream stream {std::string(value)};
    stream >> out;
    return !stream.fail() && stream.rdbuf()->in_avail() == 0;
}

bool parse_bool_value(std::string_view value, bool& out) {
    if (value == "1" || value == "true" || value == "TRUE") {
        out = true;
        return true;
    }
    if (value == "0" || value == "false" || value == "FALSE") {
        out = false;
        return true;
    }
    return false;
}

bool parse_creative_specs(
    std::string_view value,
    rtb::engine::AdSlotKey ad_slot_key,
    std::vector<rtb::engine::CreativeRecord>& creatives_out,
    std::uint32_t& creative_offset_out,
    std::uint32_t& creative_count_out
) {
    creative_offset_out = static_cast<std::uint32_t>(creatives_out.size());
    creative_count_out = 0;

    std::size_t start = 0;
    while (start < value.size()) {
        const std::size_t delimiter = value.find(rtb::constants::kCreativeDelimiter, start);
        const std::string_view token = delimiter == std::string_view::npos
                                           ? value.substr(start)
                                           : value.substr(start, delimiter - start);

        if (!token.empty()) {
            std::uint64_t creative_id = 0;
            if (!parse_u64(token, creative_id)) {
                return false;
            }

            creatives_out.push_back(rtb::engine::CreativeRecord {
                .creative_id = creative_id,
                .ad_slot_key = ad_slot_key,
            });
            ++creative_count_out;
        }

        if (delimiter == std::string_view::npos) {
            break;
        }
        start = delimiter + 1;
    }

    return creative_count_out > 0;
}

/**
 * @brief Takes a line from the campaign CSV and creates an @ref CampaignRecord.
 * 
 * It also creates indexes for all the campaigns by record and all the campaigns by ad slot for quicker references
 */
bool append_campaign_line(
    const std::vector<std::string>& fields,
    rtb::engine::CampaignStoreSnapshot& snapshot
) {
    if (fields.size() != 8) {
        return false;
    }

    std::uint64_t campaign_id = 0;
    double value_per_action = 0.0;
    double aggressiveness = 0.0;
    bool active = false;
    if (!parse_u64(fields[0], campaign_id) ||
        !parse_double_value(fields[4], value_per_action) ||
        !parse_double_value(fields[5], aggressiveness) ||
        !parse_bool_value(fields[6], active)) {
        return false;
    }

    const auto country_key = rtb::engine::normalize_country_key(fields[1]);
    const auto device_type_key = rtb::engine::normalize_device_type_key(fields[2]);
    const auto ad_slot_key = rtb::engine::normalize_ad_slot_key(fields[3]);
    if (country_key.value == 0 ||
        device_type_key == rtb::engine::DeviceTypeKey::kUnknown ||
        ad_slot_key.value == 0) {
        return false;
    }

    std::uint32_t creative_offset = 0;
    std::uint32_t creative_count = 0;
    if (!parse_creative_specs(fields[7], ad_slot_key, snapshot.creatives, creative_offset, creative_count)) {
        return false;
    }

    const std::uint32_t campaign_index = static_cast<std::uint32_t>(snapshot.campaigns.size());
    snapshot.campaigns.push_back(rtb::engine::CampaignRecord {
        .campaign_id = campaign_id,
        .country_key = country_key,
        .device_type_key = device_type_key,
        .ad_slot_key = ad_slot_key,
        .value_per_action = value_per_action,
        .aggressiveness = aggressiveness,
        .creative_offset = creative_offset,
        .creative_count = creative_count,
        .active = active,
    });

    snapshot.campaigns_by_country[country_key.value].push_back(campaign_index);
    snapshot.campaigns_by_ad_slot[ad_slot_key.value].push_back(campaign_index);
    return true;
}

}  // namespace

namespace rtb::engine {

std::shared_ptr<const CampaignStoreSnapshot> load_campaign_store_snapshot(const std::string& path) {
    std::ifstream input(path);
    if (!input.is_open()) {
        rtb::logger::LOG_ERROR("Failed opening campaign store file: %s", path.c_str());
        return nullptr;
    }

    auto snapshot = std::make_shared<CampaignStoreSnapshot>();
    std::string line;
    std::size_t line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        if (line.empty() || line[0] == '#') {
            continue;
        }

        if (line_number == 1 && line.rfind("campaign_id,", 0) == 0) {
            continue;
        }

        if (!append_campaign_line(split_csv_line(line), *snapshot)) {
            rtb::logger::LOG_ERROR("Failed parsing campaign store line %zu from %s", line_number, path.c_str());
            return nullptr;
        }
    }

    return snapshot;
}

}  // namespace rtb::engine
