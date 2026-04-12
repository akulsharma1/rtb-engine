#ifndef RTB_WORKER_RUNTIME_H_
#define RTB_WORKER_RUNTIME_H_

#include <cstddef>
#include <cstdint>
#include <unordered_map>

#include "rtb/parse.h"
#include "rtb/runtime_types.h"

namespace rtb::engine {

struct WorkerConfig {
    int worker_id = 0;
    std::uint16_t port = 0;
    int epoll_max_events = 256;
    std::size_t connection_buffer_capacity = 16 * 1024;
    std::uint32_t max_frame_size = 64 * 1024;
};

struct WorkerRuntime {
    WorkerConfig config;
    int epoll_fd = -1;
    int listen_fd = -1;
    std::uint64_t next_connection_id = 1;
    ParseScratch parse_scratch;
    std::unordered_map<int, ConnectionState> connections;
};

bool initialize_worker_runtime(WorkerRuntime& runtime);
void shutdown_worker_runtime(WorkerRuntime& runtime);
int run_worker_loop(WorkerRuntime& runtime);

}  // namespace rtb::engine

#endif  // RTB_WORKER_RUNTIME_H_
