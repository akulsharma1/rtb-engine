#include "rtb/benchmark.h"

#include <string>

int main() {
    using namespace rtb::engine;

    BenchmarkRecorder recorder(0);
    recorder.record_request(RequestTimingBreakdown {
        .parse_ns = 10,
        .normalize_validate_ns = 20,
        .retrieve_ns = 30,
        .decide_ns = 40,
        .response_build_ns = 50,
        .response_stage_ns = 60,
        .total_internal_ns = 210,
    });
    recorder.record_request(RequestTimingBreakdown {
        .parse_ns = 15,
        .normalize_validate_ns = 25,
        .retrieve_ns = 35,
        .decide_ns = 45,
        .response_build_ns = 55,
        .response_stage_ns = 65,
        .total_internal_ns = 240,
    });

    const BenchmarkSummary summary = recorder.summarize();
    if (summary.request_count != 2 ||
        summary.parse.count != 2 ||
        summary.parse.min_ns != 10 ||
        summary.parse.max_ns != 15 ||
        summary.response_stage.min_ns != 60 ||
        summary.total_internal.max_ns != 240 ||
        summary.total_internal.avg_ns < 210.0 ||
        summary.total_internal.avg_ns > 240.0) {
        return 1;
    }

    const std::string json_path = "/tmp/rtb_benchmark_test.json";
    const BenchmarkMetadata metadata {
        .timestamp_utc = "2026-04-27T00:00:00Z",
        .port = 8080,
        .workers = 1,
        .request_limit = 2,
        .duration_ms = 0,
        .completed_requests = 2,
        .campaign_data_path = "data/sample_campaigns.csv",
    };

    if (!recorder.write_json(json_path, metadata)) {
        return 1;
    }

    return 0;
}
