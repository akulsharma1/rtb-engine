#include "rtb/benchmark.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string_view>

namespace {

std::uint64_t percentile_value(std::vector<std::uint64_t> values, double percentile) {
    if (values.empty()) {
        return 0;
    }

    std::sort(values.begin(), values.end());
    const std::size_t index = static_cast<std::size_t>(
        percentile * static_cast<double>(values.size() - 1)
    );
    return values[index];
}

template <typename Accessor>
rtb::engine::BenchmarkPhaseSummary summarize_phase(
    const std::vector<rtb::engine::RequestTimingBreakdown>& requests,
    Accessor accessor
) {
    rtb::engine::BenchmarkPhaseSummary summary;
    if (requests.empty()) {
        return summary;
    }

    std::vector<std::uint64_t> values;
    values.reserve(requests.size());

    std::uint64_t min_value = accessor(requests.front());
    std::uint64_t max_value = min_value;
    long double total = 0.0;

    for (const auto& request : requests) {
        const std::uint64_t value = accessor(request);
        values.push_back(value);
        min_value = std::min(min_value, value);
        max_value = std::max(max_value, value);
        total += static_cast<long double>(value);
    }

    summary.count = static_cast<std::uint64_t>(values.size());
    summary.min_ns = min_value;
    summary.max_ns = max_value;
    summary.avg_ns = static_cast<double>(total / static_cast<long double>(values.size()));
    summary.p50_ns = percentile_value(values, 0.50);
    summary.p95_ns = percentile_value(values, 0.95);
    summary.p99_ns = percentile_value(values, 0.99);
    return summary;
}

std::string phase_json(std::string_view name, const rtb::engine::BenchmarkPhaseSummary& phase) {
    std::ostringstream os;
    os << "    \"" << name << "\": {\n"
       << "      \"count\": " << phase.count << ",\n"
       << "      \"min_ns\": " << phase.min_ns << ",\n"
       << "      \"max_ns\": " << phase.max_ns << ",\n"
       << "      \"avg_ns\": " << std::fixed << std::setprecision(2) << phase.avg_ns << ",\n"
       << "      \"p50_ns\": " << phase.p50_ns << ",\n"
       << "      \"p95_ns\": " << phase.p95_ns << ",\n"
       << "      \"p99_ns\": " << phase.p99_ns << "\n"
       << "    }";
    return os.str();
}

std::string timestamp_now_utc() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm tm_utc {};
#if defined(_WIN32)
    gmtime_s(&tm_utc, &now_time);
#else
    gmtime_r(&now_time, &tm_utc);
#endif

    std::ostringstream os;
    os << std::put_time(&tm_utc, "%Y-%m-%dT%H:%M:%SZ");
    return os.str();
}

}  // namespace

namespace rtb::engine {

BenchmarkRecorder::BenchmarkRecorder(std::uint64_t request_limit)
    : request_limit_(request_limit),
      requests_(std::make_shared<std::vector<RequestTimingBreakdown>>()),
      mutex_(std::make_shared<std::mutex>()) {}

void BenchmarkRecorder::record_request(const RequestTimingBreakdown& timing) {
    std::lock_guard<std::mutex> lock(*mutex_);
    requests_->push_back(timing);
}

std::uint64_t BenchmarkRecorder::completed_requests() const {
    std::lock_guard<std::mutex> lock(*mutex_);
    return static_cast<std::uint64_t>(requests_->size());
}

bool BenchmarkRecorder::request_limit_reached() const {
    return request_limit_ != 0 && completed_requests() >= request_limit_;
}

std::shared_ptr<std::vector<RequestTimingBreakdown>> BenchmarkRecorder::snapshot_requests() const {
    std::lock_guard<std::mutex> lock(*mutex_);
    return std::make_shared<std::vector<RequestTimingBreakdown>>(*requests_);
}

BenchmarkSummary BenchmarkRecorder::summarize() const {
    const auto requests = snapshot_requests();

    BenchmarkSummary summary;
    summary.request_count = static_cast<std::uint64_t>(requests->size());
    summary.parse = summarize_phase(*requests, [](const RequestTimingBreakdown& value) {
        return value.parse_ns;
    });
    summary.normalize_validate = summarize_phase(*requests, [](const RequestTimingBreakdown& value) {
        return value.normalize_validate_ns;
    });
    summary.retrieve = summarize_phase(*requests, [](const RequestTimingBreakdown& value) {
        return value.retrieve_ns;
    });
    summary.decide = summarize_phase(*requests, [](const RequestTimingBreakdown& value) {
        return value.decide_ns;
    });
    summary.response_build = summarize_phase(*requests, [](const RequestTimingBreakdown& value) {
        return value.response_build_ns;
    });
    summary.response_stage = summarize_phase(*requests, [](const RequestTimingBreakdown& value) {
        return value.response_stage_ns;
    });
    summary.total_internal = summarize_phase(*requests, [](const RequestTimingBreakdown& value) {
        return value.total_internal_ns;
    });
    return summary;
}

void BenchmarkRecorder::print_summary(std::ostream& os) const {
    const BenchmarkSummary summary = summarize();

    auto print_phase = [&os](std::string_view name, const BenchmarkPhaseSummary& phase) {
        os << "  " << name
           << ": avg=" << std::fixed << std::setprecision(2) << phase.avg_ns
           << "ns p50=" << phase.p50_ns
           << "ns p95=" << phase.p95_ns
           << "ns p99=" << phase.p99_ns
           << "ns max=" << phase.max_ns
           << "ns\n";
    };

    os << "Engine internal benchmark summary\n";
    os << "requests=" << summary.request_count << '\n';
    print_phase("parse", summary.parse);
    print_phase("normalize_validate", summary.normalize_validate);
    print_phase("retrieve", summary.retrieve);
    print_phase("decide", summary.decide);
    print_phase("response_build", summary.response_build);
    print_phase("response_stage", summary.response_stage);
    print_phase("total_internal", summary.total_internal);
}

bool BenchmarkRecorder::write_json(const std::string& path, const BenchmarkMetadata& metadata) const {
    std::ofstream output(path, std::ios::out | std::ios::trunc);
    if (!output.is_open()) {
        return false;
    }

    const BenchmarkSummary summary = summarize();
    const BenchmarkMetadata final_metadata {
        .timestamp_utc = metadata.timestamp_utc.empty() ? timestamp_now_utc() : metadata.timestamp_utc,
        .port = metadata.port,
        .workers = metadata.workers,
        .request_limit = metadata.request_limit,
        .duration_ms = metadata.duration_ms,
        .completed_requests = summary.request_count,
        .campaign_data_path = metadata.campaign_data_path,
    };

    output << "{\n";
    output << "  \"metadata\": {\n";
    output << "    \"timestamp_utc\": \"" << final_metadata.timestamp_utc << "\",\n";
    output << "    \"port\": " << final_metadata.port << ",\n";
    output << "    \"workers\": " << final_metadata.workers << ",\n";
    output << "    \"request_limit\": " << final_metadata.request_limit << ",\n";
    output << "    \"duration_ms\": " << final_metadata.duration_ms << ",\n";
    output << "    \"completed_requests\": " << final_metadata.completed_requests << ",\n";
    output << "    \"campaign_data_path\": \"" << final_metadata.campaign_data_path << "\"\n";
    output << "  },\n";
    output << "  \"phases\": {\n";
    output << phase_json("parse", summary.parse) << ",\n";
    output << phase_json("normalize_validate", summary.normalize_validate) << ",\n";
    output << phase_json("retrieve", summary.retrieve) << ",\n";
    output << phase_json("decide", summary.decide) << ",\n";
    output << phase_json("response_build", summary.response_build) << ",\n";
    output << phase_json("response_stage", summary.response_stage) << ",\n";
    output << phase_json("total_internal", summary.total_internal) << '\n';
    output << "  }\n";
    output << "}\n";
    return true;
}

}  // namespace rtb::engine
