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

/*
WorkerRng is a way to generate random numbers with semi-determinism.
When we give a seed, it will always generate the same values in order.
It also doesn't use a syscall for true randomness.
*/
struct WorkerRng {
    std::uint64_t state = 1;

    void seed(std::uint64_t seed_value) noexcept {
        state = seed_value == 0 ? 1 : seed_value;
    }

    [[nodiscard]] std::uint64_t next_u64() noexcept {
        std::uint64_t value = state;
        value ^= value << 13;
        value ^= value >> 7;
        value ^= value << 17;
        state = value == 0 ? 1 : value;
        return state;
    }

    [[nodiscard]] double next_unit_double() noexcept {
        constexpr double kScale = 1.0 / static_cast<double>(1ULL << 53);
        return static_cast<double>(next_u64() >> 11) * kScale;
    }

    // Generates a random index between 0 and the upper bound
    [[nodiscard]] std::size_t uniform_index(std::size_t upper_bound) noexcept {
        return upper_bound == 0 ? 0 : static_cast<std::size_t>(next_u64() % upper_bound);
    }
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
