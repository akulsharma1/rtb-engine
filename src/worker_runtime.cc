#include "rtb/worker_runtime.h"
#include <array>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "logs/logs.h"
#include "rtb/campaign_store.h"
#include "rtb/config.h"
#include "rtb/frame.h"
#include "rtb/handle_request.h"
#include "rtb/engine_types.h"
#include "rtb/parse.h"
#include "rtb/response_framing.h"
#include "rtb/runtime_types.h"

namespace {

std::uint64_t now_ns() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count()
    );
}

void close_connection(rtb::engine::WorkerRuntime& runtime, int client_fd) {
    if (client_fd < 0) {
        return;
    }

    epoll_ctl(runtime.epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr);
    close(client_fd);
    runtime.connections.erase(client_fd);
}

bool update_client_events(rtb::engine::WorkerRuntime& runtime, int client_fd, bool enable_write) {
    epoll_event event {};
    const std::uint32_t write_event = enable_write ? static_cast<std::uint32_t>(EPOLLOUT) : 0U;
    event.events = static_cast<std::uint32_t>(EPOLLIN | EPOLLRDHUP) | write_event;
    event.data.fd = client_fd;
    return epoll_ctl(runtime.epoll_fd, EPOLL_CTL_MOD, client_fd, &event) != -1;
}

bool flush_pending_write(rtb::engine::WorkerRuntime& runtime, rtb::engine::ConnectionState& connection) {
    while (connection.write_buffer.readable_bytes() > 0) {
        const auto readable = std::span<const std::byte>(
            connection.write_buffer.storage.data() + connection.write_buffer.read_offset,
            connection.write_buffer.readable_bytes()
        );

        const ssize_t bytes_written = write(connection.fd, readable.data(), readable.size());
        if (bytes_written > 0) {
            connection.write_buffer.consume_frame(static_cast<std::size_t>(bytes_written));
            continue;
        }

        if (bytes_written == 0) {
            return false;
        }

        if (errno == EINTR) {
            continue;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            break;
        }

        return false;
    }

    connection.has_pending_write = connection.write_buffer.readable_bytes() != 0;
    if (!update_client_events(runtime, connection.fd, connection.has_pending_write)) {
        return false;
    }

    return true;
}

enum class ProcessFramesOutcome : std::uint8_t {
    kContinue = 0,
    kPauseForWrite,
    kCloseConnection,
};

ProcessFramesOutcome process_ready_frames(
    rtb::engine::WorkerRuntime& runtime,
    rtb::engine::ConnectionState& connection
) {
    for (;;) {
        rtb::engine::FrameView frame;
        const rtb::engine::FrameStatus status =
            connection.read_buffer.try_extract_frame(runtime.config.max_frame_size, frame);

        if (status == rtb::engine::FrameStatus::kNeedMoreData) {
            return ProcessFramesOutcome::kContinue;
        }

        if (status == rtb::engine::FrameStatus::kInvalidLength) {
            return ProcessFramesOutcome::kCloseConnection;
        }

        rtb::engine::ParsedMessage parsed_message;
        const rtb::engine::ParseStatus parse_status =
            rtb::engine::parse_bid_request(frame.payload, runtime.parse_scratch, parsed_message);
        if (parse_status != rtb::engine::ParseStatus::kOk) {
            rtb::logger::LOG_ERROR(
                "parse_bid_request failed for fd %d with status %d",
                connection.fd,
                static_cast<int>(parse_status)
            );
            return ProcessFramesOutcome::kCloseConnection;
        }

        const rtb::engine::HandleRequestResult request_result =
            rtb::engine::handle_request(parsed_message, now_ns(), *runtime.campaign_store, runtime.rng);
        if (request_result.status == rtb::engine::HandleRequestStatus::kDropConnection) {
            return ProcessFramesOutcome::kCloseConnection;
        }

        if (!rtb::engine::stage_response_frame(request_result.response, connection.write_buffer)) {
            rtb::logger::LOG_ERROR("Failed staging response for fd %d", connection.fd);
            return ProcessFramesOutcome::kCloseConnection;
        }

        connection.read_buffer.consume_frame(frame.total_frame_bytes);
        connection.has_pending_write = true;
        if (!update_client_events(runtime, connection.fd, true)) {
            return ProcessFramesOutcome::kCloseConnection;
        }

        return ProcessFramesOutcome::kPauseForWrite;
    }
}

}  // namespace

namespace rtb::engine {

bool initialize_worker_runtime(WorkerRuntime& runtime) {
    runtime.rng.seed(
        static_cast<std::uint64_t>(runtime.config.worker_id + 1) ^
        now_ns() ^
        rtb::config::kDefaultRngSeed
    );

    if (runtime.campaign_store == nullptr) {
        runtime.campaign_store = load_campaign_store_snapshot(runtime.config.campaign_data_path);
        if (runtime.campaign_store == nullptr) {
            rtb::logger::LOG_ERROR(
                "Failed loading campaign store snapshot from %s",
                runtime.config.campaign_data_path.c_str()
            );
            return false;
        }
    }

    runtime.listen_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (runtime.listen_fd == -1) {
        return false;
    }

    int optval = 1;
    if (setsockopt(runtime.listen_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
        rtb::logger::LOG_ERROR("%s", "Error setting SO_REUSEADDR");
        shutdown_worker_runtime(runtime);
        return false;
    }

    if (setsockopt(runtime.listen_fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval)) == -1) {
        rtb::logger::LOG_ERROR("%s", "Error setting SO_REUSEPORT");
        shutdown_worker_runtime(runtime);
        return false;
    }

    runtime.next_connection_id = 1;
    runtime.connections.clear();

    sockaddr_in server_addr {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(runtime.config.port);

    if (bind(runtime.listen_fd, reinterpret_cast<const sockaddr*>(&server_addr), sizeof(server_addr)) == -1) {
        rtb::logger::LOG_ERROR("Error binding to port %u", runtime.config.port);
        shutdown_worker_runtime(runtime);
        return false;
    }

    if (listen(runtime.listen_fd, SOMAXCONN) == -1) {
        rtb::logger::LOG_ERROR("%s", "Error starting socket listen");
        shutdown_worker_runtime(runtime);
        return false;
    }

    runtime.epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (runtime.epoll_fd == -1) {
        rtb::logger::LOG_ERROR("%s", "Error creating epoll fd");
        shutdown_worker_runtime(runtime);
        return false;
    }

    epoll_event event {};
    event.events = EPOLLIN;
    event.data.fd = runtime.listen_fd;

    if (epoll_ctl(runtime.epoll_fd, EPOLL_CTL_ADD, runtime.listen_fd, &event) == -1) {
        rtb::logger::LOG_ERROR("%s", "Error registering listen fd with epoll");
        shutdown_worker_runtime(runtime);
        return false;
    }

    if (runtime.config.shutdown_fd >= 0) {
        epoll_event shutdown_event {};
        shutdown_event.events = EPOLLIN;
        shutdown_event.data.fd = runtime.config.shutdown_fd;

        if (epoll_ctl(runtime.epoll_fd, EPOLL_CTL_ADD, runtime.config.shutdown_fd, &shutdown_event) == -1) {
            rtb::logger::LOG_ERROR("%s", "Error registering shutdown fd with epoll");
            shutdown_worker_runtime(runtime);
            return false;
        }
    }

    return true;
}

void shutdown_worker_runtime(WorkerRuntime& runtime) {
    for (auto& [fd, connection] : runtime.connections) {
        if (connection.fd >= 0) {
            close(connection.fd);
            connection.fd = -1;
        }
    }
    runtime.connections.clear();

    if (runtime.listen_fd >= 0) {
        close(runtime.listen_fd);
        runtime.listen_fd = -1;
    }
    if (runtime.epoll_fd >= 0) {
        close(runtime.epoll_fd);
        runtime.epoll_fd = -1;
    }
}

int run_worker_loop(WorkerRuntime& runtime) {
    std::vector<epoll_event> events(static_cast<std::size_t>(runtime.config.epoll_max_events));
    for (;;) {
        const int nfds = epoll_wait(runtime.epoll_fd, events.data(), runtime.config.epoll_max_events, -1);

        if (nfds == -1) {
            if (errno == EINTR) {
                continue;
            }
            rtb::logger::LOG_ERROR("%s", "Error with epoll_wait");
            continue;
        }

        for (int i = 0; i < nfds; i++) {
            if (events[i].data.fd == runtime.config.shutdown_fd) {
                return 0;
            }

            if (events[i].data.fd == runtime.listen_fd) {
                for (;;) {
                    const int client_fd = accept4(runtime.listen_fd, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
                    if (client_fd == -1) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            break;
                        }

                        rtb::logger::LOG_ERROR("%s", "accept4 failed for incoming client");
                        break;
                    }

                    ConnectionState connection {};
                    connection.fd = client_fd;
                    connection.connection_id = runtime.next_connection_id++;

                    /*
                    Currently we allocate buffers in the initial TCP connection. We do this for v1 implementation ease
                    However, this is slow as we can instead preallocate buffers at startup and instead hand them out as connections are accepted
                    Thereforee, TODO: create a buffer pool allocator
                    */
                    connection.read_buffer.reserve(runtime.config.connection_buffer_capacity);
                    connection.write_buffer.reserve(runtime.config.connection_buffer_capacity);

                    runtime.connections.emplace(client_fd, std::move(connection));

                    epoll_event event {};
                    event.events = EPOLLIN | EPOLLRDHUP;
                    event.data.fd = client_fd;

                    if (epoll_ctl(runtime.epoll_fd, EPOLL_CTL_ADD, client_fd, &event) == -1) {
                        close(client_fd);
                        runtime.connections.erase(client_fd);
                        logger::LOG_ERROR("%s", "epoll_ctl failed for client fd");
                        continue;
                    }
                }
            } else {
                const int client_fd = events[i].data.fd;
                auto it = runtime.connections.find(client_fd);
                if (it == runtime.connections.end()) {
                    logger::LOG_ERROR("Error finding fd %d in runtime.connections()", client_fd);
                    continue;
                }

                if ((events[i].events & EPOLLERR) != 0 ||
                    (events[i].events & EPOLLHUP) != 0 ||
                    (events[i].events & EPOLLRDHUP) != 0) {
                    close_connection(runtime, client_fd);
                    continue;
                }

                ConnectionState& connection = it->second;

                if (connection.has_pending_write) {
                    if ((events[i].events & EPOLLOUT) != 0 && !flush_pending_write(runtime, connection)) {
                        close_connection(runtime, client_fd);
                        continue;
                    }

                    if (connection.has_pending_write) {
                        continue;
                    }
                }

                const ProcessFramesOutcome buffered_frame_outcome = process_ready_frames(runtime, connection);
                if (buffered_frame_outcome == ProcessFramesOutcome::kCloseConnection) {
                    close_connection(runtime, client_fd);
                    continue;
                }
                if (buffered_frame_outcome == ProcessFramesOutcome::kPauseForWrite) {
                    continue;
                }

                bool should_close = false;
                for (;;) {
                    auto writable = connection.read_buffer.writable_region();
                    if (writable.empty()) {
                        /*
                        TODO: in the future maybe grow? 
                        however we should probably just design buffers such that they never actually fill up
                        we should know the general worst case size of a message so we should allocate 
                        such that we never need another realloc call on the hot path.
                        */
                        connection.read_buffer.compact();
                        writable = connection.read_buffer.writable_region();
                        if (writable.empty()) {
                            logger::LOG_ERROR("Connection read buffer is full for fd %d", client_fd);
                            should_close = true;
                            break;
                        }
                    }

                    const ssize_t bytes_read = read(client_fd, writable.data(), writable.size());

                    if (bytes_read > 0) {
                        connection.read_buffer.commit_write(static_cast<size_t>(bytes_read));

                        const ProcessFramesOutcome process_outcome = process_ready_frames(runtime, connection);
                        if (process_outcome == ProcessFramesOutcome::kCloseConnection) {
                            should_close = true;
                            break;
                        }
                        if (process_outcome == ProcessFramesOutcome::kPauseForWrite) {
                            break;
                        }

                        if (should_close) {
                            break;
                        }
                        continue;
                    } else if (bytes_read == 0) {
                        should_close = true;
                        break;
                    }

                    if (errno == EINTR) {
                        continue;
                    }
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        break;
                    }

                    logger::LOG_ERROR("read failed for fd %d", client_fd);
                    should_close = true;
                    break;
                }

                if (should_close) {
                    close_connection(runtime, client_fd);
                }
            }
        }
    }
    return 0;
}

}  // namespace rtb::engine
