#ifndef RTB_BENCHMARK_H_
#define RTB_BENCHMARK_H_

#include <cstdint>
#include <iosfwd>
#include <mutex>
#include <memory>
#include <string>
#include <vector>

namespace rtb::engine {

struct HandleRequestTimingBreakdown {
    std::uint64_t normalize_validate_ns = 0;
    std::uint64_t retrieve_ns = 0;
    std::uint64_t decide_ns = 0;
    std::uint64_t response_build_ns = 0;
};

struct RequestTimingBreakdown {
    std::uint64_t parse_ns = 0;
    std::uint64_t normalize_validate_ns = 0;
    std::uint64_t retrieve_ns = 0;
    std::uint64_t decide_ns = 0;
    std::uint64_t response_build_ns = 0;
    std::uint64_t response_stage_ns = 0;
    std::uint64_t total_internal_ns = 0;
};

struct BenchmarkPhaseSummary {
    std::uint64_t count = 0;
    std::uint64_t min_ns = 0;
    std::uint64_t max_ns = 0;
    double avg_ns = 0.0;
    std::uint64_t p50_ns = 0;
    std::uint64_t p95_ns = 0;
    std::uint64_t p99_ns = 0;
};

struct BenchmarkSummary {
    std::uint64_t request_count = 0;
    BenchmarkPhaseSummary parse {};
    BenchmarkPhaseSummary normalize_validate {};
    BenchmarkPhaseSummary retrieve {};
    BenchmarkPhaseSummary decide {};
    BenchmarkPhaseSummary response_build {};
    BenchmarkPhaseSummary response_stage {};
    BenchmarkPhaseSummary total_internal {};
};

struct BenchmarkMetadata {
    std::string timestamp_utc;
    std::uint16_t port = 0;
    int workers = 0;
    std::uint64_t request_limit = 0;
    std::uint64_t duration_ms = 0;
    std::uint64_t completed_requests = 0;
    std::string campaign_data_path;
};

class BenchmarkRecorder {
public:
    explicit BenchmarkRecorder(std::uint64_t request_limit = 0);

    void record_request(const RequestTimingBreakdown& timing);
    [[nodiscard]] std::uint64_t completed_requests() const;
    [[nodiscard]] bool request_limit_reached() const;
    [[nodiscard]] BenchmarkSummary summarize() const;
    void print_summary(std::ostream& os) const;
    bool write_json(const std::string& path, const BenchmarkMetadata& metadata) const;

private:
    std::shared_ptr<std::vector<RequestTimingBreakdown>> snapshot_requests() const;

    std::uint64_t request_limit_ = 0;
    std::shared_ptr<std::vector<RequestTimingBreakdown>> requests_;
    mutable std::shared_ptr<std::mutex> mutex_;
};

}  // namespace rtb::engine

#endif  // RTB_BENCHMARK_H_
