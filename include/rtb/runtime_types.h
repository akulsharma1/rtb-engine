#ifndef RTB_RUNTIME_TYPES_H_
#define RTB_RUNTIME_TYPES_H_

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace rtb::engine {

enum class FrameStatus : std::uint8_t;
struct FrameView;

/*
ReusableBuffers are used when a worker is reading its TCP connection for new Protobuf messages.
Since TCP doesn't preserve message boundaries, we may receive partial protobuf messages from the SSP.
Therefore, we use ReusableBuffers to figure out when we've actually received a full message & can parse it.
*/
struct ReusableBuffer {
    std::vector<std::byte> storage;
    std::size_t read_offset = 0;
    std::size_t write_offset = 0;

    [[nodiscard]] std::size_t readable_bytes() const noexcept {
        return write_offset - read_offset;
    }

    [[nodiscard]] std::size_t writable_bytes() const noexcept {
        return storage.size() - write_offset;
    }

    // makes sure that the buffer has atleast capacity bytes of total size
    void reserve(std::size_t capacity);

    // returns the part of the buffer that is still available to write into (e.g. not already used)
    [[nodiscard]] std::span<std::byte> writable_region();

    // advances write_offset by bytes_written. use after actually writing to the buffer
    void commit_write(std::size_t bytes_written);

    // extracts the current frame from the buffer into an std::span<const std::byte> in out.
    // note that the std::span directly acts as a pointer. later on .compact() can be called and move the data.
    // therefore, be careful with the data lifespan.
    FrameStatus try_extract_frame(std::uint32_t max_frame_size, FrameView& out);

    // moves the buffer's read_offset to the next position where you should read from. 
    void consume_frame(std::size_t total_frame_bytes);

    // moves any written bytes to the the frontmost unwritten bytes in the array
    void compact();
};

struct ConnectionState {
    int fd = -1;
    std::uint64_t connection_id = 0;
    std::uint64_t request_sequence = 0;
    ReusableBuffer read_buffer;
    ReusableBuffer write_buffer;
};

}  // namespace rtb::engine

#endif  // RTB_RUNTIME_TYPES_H_
