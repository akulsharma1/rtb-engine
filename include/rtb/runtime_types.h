#ifndef RTB_RUNTIME_TYPES_H_
#define RTB_RUNTIME_TYPES_H_

#include <cstddef>
#include <cstdint>
#include <vector>

namespace rtb::engine {

struct ReusableBuffer {
    std::vector<std::byte> storage;
    std::size_t read_offset = 0;
    std::size_t write_offset = 0;
    std::uint32_t frame_length = 0;
    bool frame_ready = false;

    [[nodiscard]] std::size_t readable_bytes() const noexcept {
        return write_offset - read_offset;
    }

    [[nodiscard]] std::size_t writable_bytes() const noexcept {
        return storage.size() - write_offset;
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
