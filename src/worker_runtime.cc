#include "rtb/worker_runtime.h"
#include <array>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "logs/logs.h"
#include "rtb/frame.h"
#include "rtb/handle_request.h"
#include "rtb/engine_types.h"
#include "rtb/parse.h"
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

}  // namespace

namespace rtb::engine {

bool initialize_worker_runtime(WorkerRuntime& runtime) {
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

                        for (;;) {
                            FrameView frame;
                            const FrameStatus status = connection.read_buffer.try_extract_frame(runtime.config.max_frame_size, frame);

                            if (status == FrameStatus::kNeedMoreData) {
                                break;
                            }

                            if (status == FrameStatus::kInvalidLength) {
                                should_close = true;
                                break;
                            }

                            ParsedMessage parsed_message;
                            const ParseStatus parse_status =
                                parse_bid_request(frame.payload, runtime.parse_scratch, parsed_message);
                            if (parse_status != ParseStatus::kOk) {
                                logger::LOG_ERROR("parse_bid_request failed for fd %d with status %d",
                                                  client_fd,
                                                  static_cast<int>(parse_status));
                                should_close = true;
                                break;
                            }

                            const HandleRequestResult request_result =
                                handle_request(parsed_message, now_ns());
                            if (request_result.status == HandleRequestStatus::kDropConnection) {
                                should_close = true;
                                break;
                            }

                            connection.read_buffer.consume_frame(frame.total_frame_bytes);
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
