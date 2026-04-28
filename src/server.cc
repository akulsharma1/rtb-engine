#include "rtb/server.h"

#include <atomic>
#include <charconv>
#include <csignal>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <mutex>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include <sys/eventfd.h>
#include <unistd.h>

#include "logs/logs.h"
#include "rtb/benchmark.h"
#include "rtb/campaign_store.h"
#include "rtb/worker_runtime.h"

namespace {

#ifndef RTB_PROJECT_SOURCE_DIR
#define RTB_PROJECT_SOURCE_DIR "."
#endif

volatile std::sig_atomic_t g_shutdown_requested = 0;
int g_shutdown_event_fd = -1;

std::string default_campaign_data_path() {
    return RTB_PROJECT_SOURCE_DIR "/data/sample_campaigns.csv";
}

extern "C" void handle_shutdown_signal(int) {
    g_shutdown_requested = 1;

    if (g_shutdown_event_fd < 0) {
        return;
    }

    const std::uint64_t wake_value = 1;
    const ssize_t ignored_result = write(g_shutdown_event_fd, &wake_value, sizeof(wake_value));
    (void)ignored_result;
}

bool parse_int_arg(std::string_view text, int& out) {
    int value = 0;
    const auto* begin = text.data();
    const auto* end = text.data() + text.size();
    const auto result = std::from_chars(begin, end, value);
    if (result.ec != std::errc {} || result.ptr != end) {
        return false;
    }

    out = value;
    return true;
}

bool parse_u64_arg(std::string_view text, std::uint64_t& out) {
    std::uint64_t value = 0;
    const auto* begin = text.data();
    const auto* end = text.data() + text.size();
    const auto result = std::from_chars(begin, end, value);
    if (result.ec != std::errc {} || result.ptr != end) {
        return false;
    }

    out = value;
    return true;
}

bool parse_port_arg(std::string_view text, std::uint16_t& out) {
    int value = 0;
    if (!parse_int_arg(text, value) || value <= 0 || value > 65535) {
        return false;
    }

    out = static_cast<std::uint16_t>(value);
    return true;
}

struct SignalHandlerScope {
    struct sigaction previous_sigint {};
    struct sigaction previous_sigterm {};
    int previous_event_fd = -1;
    bool installed = false;

    ~SignalHandlerScope() {
        if (!installed) {
            return;
        }

        sigaction(SIGINT, &previous_sigint, nullptr);
        sigaction(SIGTERM, &previous_sigterm, nullptr);
        g_shutdown_event_fd = previous_event_fd;
    }
};

bool install_signal_handlers(int shutdown_event_fd, SignalHandlerScope& scope) {
    struct sigaction action {};
    action.sa_handler = handle_shutdown_signal;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    scope.previous_event_fd = g_shutdown_event_fd;
    g_shutdown_event_fd = shutdown_event_fd;

    if (sigaction(SIGINT, &action, &scope.previous_sigint) == -1) {
        g_shutdown_event_fd = scope.previous_event_fd;
        return false;
    }

    if (sigaction(SIGTERM, &action, &scope.previous_sigterm) == -1) {
        sigaction(SIGINT, &scope.previous_sigint, nullptr);
        g_shutdown_event_fd = scope.previous_event_fd;
        return false;
    }

    scope.installed = true;
    return true;
}

void request_shutdown(int shutdown_event_fd) {
    g_shutdown_requested = 1;

    if (shutdown_event_fd < 0) {
        return;
    }

    const std::uint64_t wake_value = 1;
    const ssize_t ignored_result = write(shutdown_event_fd, &wake_value, sizeof(wake_value));
    (void)ignored_result;
}

}  // namespace

namespace rtb::engine {

ParseServerConfigStatus parse_server_config(
    std::span<const std::string_view> args,
    ServerConfig& out,
    std::string& error
) {
    out = ServerConfig {};
    out.campaign_data_path = default_campaign_data_path();
    error.clear();

    for (std::size_t index = 0; index < args.size(); ++index) {
        const std::string_view arg = args[index];
        if (arg == "--help") {
            return ParseServerConfigStatus::kHelpRequested;
        }

        if (arg == "--benchmark") {
            out.benchmark = true;
            continue;
        }

        if (arg == "--port") {
            if (index + 1 >= args.size()) {
                error = "missing value for --port";
                return ParseServerConfigStatus::kInvalidArgument;
            }

            if (!parse_port_arg(args[++index], out.port)) {
                error = "invalid value for --port";
                return ParseServerConfigStatus::kInvalidArgument;
            }
            continue;
        }

        if (arg == "--workers") {
            if (index + 1 >= args.size()) {
                error = "missing value for --workers";
                return ParseServerConfigStatus::kInvalidArgument;
            }

            if (!parse_int_arg(args[++index], out.workers) || out.workers <= 0) {
                error = "invalid value for --workers";
                return ParseServerConfigStatus::kInvalidArgument;
            }
            continue;
        }

        if (arg == "--campaign-data-path") {
            if (index + 1 >= args.size()) {
                error = "missing value for --campaign-data-path";
                return ParseServerConfigStatus::kInvalidArgument;
            }

            out.campaign_data_path = std::string(args[++index]);
            continue;
        }

        if (arg == "--benchmark-json") {
            if (index + 1 >= args.size()) {
                error = "missing value for --benchmark-json";
                return ParseServerConfigStatus::kInvalidArgument;
            }

            out.benchmark = true;
            out.benchmark_json_path = std::string(args[++index]);
            continue;
        }

        if (arg == "--benchmark-requests") {
            if (index + 1 >= args.size()) {
                error = "missing value for --benchmark-requests";
                return ParseServerConfigStatus::kInvalidArgument;
            }

            if (!parse_u64_arg(args[++index], out.benchmark_requests) || out.benchmark_requests == 0) {
                error = "invalid value for --benchmark-requests";
                return ParseServerConfigStatus::kInvalidArgument;
            }
            out.benchmark = true;
            continue;
        }

        if (arg == "--benchmark-duration-ms") {
            if (index + 1 >= args.size()) {
                error = "missing value for --benchmark-duration-ms";
                return ParseServerConfigStatus::kInvalidArgument;
            }

            if (!parse_u64_arg(args[++index], out.benchmark_duration_ms) || out.benchmark_duration_ms == 0) {
                error = "invalid value for --benchmark-duration-ms";
                return ParseServerConfigStatus::kInvalidArgument;
            }
            out.benchmark = true;
            continue;
        }

        error = "unknown argument: " + std::string(arg);
        return ParseServerConfigStatus::kInvalidArgument;
    }

    return ParseServerConfigStatus::kOk;
}

void print_server_usage(std::ostream& os, std::string_view program_name) {
    os << "Usage: " << program_name << " [--port PORT] [--workers COUNT] "
       << "[--campaign-data-path PATH] [--benchmark] [--benchmark-json PATH] "
       << "[--benchmark-requests COUNT] [--benchmark-duration-ms MS]\n";
}

int run_server(const ServerConfig& config) {
    if (config.port == 0) {
        rtb::logger::LOG_ERROR("%s", "Server port must be non-zero");
        return 1;
    }
    if (config.workers <= 0) {
        rtb::logger::LOG_ERROR("%s", "Worker count must be positive");
        return 1;
    }

    const std::shared_ptr<const CampaignStoreSnapshot> campaign_store =
        load_campaign_store_snapshot(config.campaign_data_path);
    if (campaign_store == nullptr) {
        rtb::logger::LOG_ERROR(
            "Failed loading campaign store snapshot from %s",
            config.campaign_data_path.c_str()
        );
        return 1;
    }

    const int shutdown_event_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (shutdown_event_fd == -1) {
        rtb::logger::LOG_ERROR("%s", "Failed creating shutdown eventfd");
        return 1;
    }

    SignalHandlerScope signal_scope;
    if (!install_signal_handlers(shutdown_event_fd, signal_scope)) {
        rtb::logger::LOG_ERROR("%s", "Failed installing signal handlers");
        g_shutdown_event_fd = -1;
        close(shutdown_event_fd);
        return 1;
    }

    g_shutdown_requested = 0;

    const std::shared_ptr<BenchmarkRecorder> benchmark_recorder =
        config.benchmark ? std::make_shared<BenchmarkRecorder>(config.benchmark_requests) : nullptr;

    std::vector<std::unique_ptr<WorkerRuntime>> runtimes;
    runtimes.reserve(static_cast<std::size_t>(config.workers));

    for (int worker_id = 0; worker_id < config.workers; ++worker_id) {
        auto runtime = std::make_unique<WorkerRuntime>();
        runtime->config.worker_id = worker_id;
        runtime->config.port = config.port;
        runtime->config.shutdown_fd = shutdown_event_fd;
        runtime->config.campaign_data_path = config.campaign_data_path;
        runtime->benchmark_recorder = benchmark_recorder;
        runtime->campaign_store = campaign_store;

        if (!initialize_worker_runtime(*runtime)) {
            for (auto& initialized_runtime : runtimes) {
                shutdown_worker_runtime(*initialized_runtime);
            }
            g_shutdown_event_fd = -1;
            close(shutdown_event_fd);
            return 1;
        }

        runtimes.push_back(std::move(runtime));
    }

    std::cout << "rtb_engine listening on port "
              << config.port
              << " with "
              << config.workers
              << " worker(s)\n";

    std::atomic<int> exit_code {0};
    std::vector<std::thread> worker_threads;
    worker_threads.reserve(static_cast<std::size_t>(config.workers));
    std::thread benchmark_timer_thread;
    std::mutex benchmark_timer_mutex;
    std::condition_variable benchmark_timer_cv;
    bool benchmark_timer_cancelled = false;

    if (config.benchmark && config.benchmark_duration_ms != 0) {
        benchmark_timer_thread = std::thread([
            shutdown_event_fd,
            duration_ms = config.benchmark_duration_ms,
            &benchmark_timer_mutex,
            &benchmark_timer_cv,
            &benchmark_timer_cancelled
        ]() {
            std::unique_lock<std::mutex> lock(benchmark_timer_mutex);
            const bool cancelled = benchmark_timer_cv.wait_for(
                lock,
                std::chrono::milliseconds(duration_ms),
                [&benchmark_timer_cancelled]() { return benchmark_timer_cancelled; }
            );
            lock.unlock();

            if (!cancelled) {
                request_shutdown(shutdown_event_fd);
            }
        });
    }

    for (auto& runtime : runtimes) {
        worker_threads.emplace_back([&exit_code, shutdown_event_fd, runtime = runtime.get()]() {
            const int worker_result = run_worker_loop(*runtime);
            if (worker_result != 0) {
                exit_code.store(worker_result);
            }

            request_shutdown(shutdown_event_fd);
        });
    }

    for (auto& worker_thread : worker_threads) {
        if (worker_thread.joinable()) {
            worker_thread.join();
        }
    }

    if (benchmark_timer_thread.joinable()) {
        {
            std::lock_guard<std::mutex> lock(benchmark_timer_mutex);
            benchmark_timer_cancelled = true;
        }
        benchmark_timer_cv.notify_all();
        benchmark_timer_thread.join();
    }

    for (auto& runtime : runtimes) {
        shutdown_worker_runtime(*runtime);
    }

    if (benchmark_recorder != nullptr) {
        benchmark_recorder->print_summary(std::cout);

        if (!config.benchmark_json_path.empty()) {
            const BenchmarkMetadata metadata {
                .timestamp_utc = {},
                .port = config.port,
                .workers = config.workers,
                .request_limit = config.benchmark_requests,
                .duration_ms = config.benchmark_duration_ms,
                .completed_requests = benchmark_recorder->completed_requests(),
                .campaign_data_path = config.campaign_data_path,
            };

            if (!benchmark_recorder->write_json(config.benchmark_json_path, metadata)) {
                rtb::logger::LOG_ERROR(
                    "Failed writing benchmark JSON to %s",
                    config.benchmark_json_path.c_str()
                );
            }
        }
    }

    g_shutdown_event_fd = -1;
    close(shutdown_event_fd);
    return exit_code.load();
}

}  // namespace rtb::engine
