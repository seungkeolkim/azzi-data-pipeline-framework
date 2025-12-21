#pragma once

#include "stream_pipeline/memory/frame_buffer_store_interface.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace stream_pipeline {

/*
 * SimpleFrameBufferStore
 * ----------------------
 * - Stage 0용 HostMemory 전용 frame buffer store.
 *
 * 중요한 의도:
 * - "FrameMetadata는 포인터가 아니라 handle만 가진다"를 실제 코드로 강제한다.
 * - handle -> 내부 버퍼 조회(view) -> release 흐름을 표준화한다.
 *
 * Stage 0 단순화:
 * - MemoryLocation::HostMemory만 지원한다.
 * - PixelFormat::RGB24만 지원한다.
 *
 * Stage 1/2 확장:
 * - NV12, RGBA, DeviceMemory 등을 지원 가능.
 * - fixed-size pool, slab allocator, red-zone 등으로 고도화 가능.
 */
class SimpleFrameBufferStore final : public FrameBufferStoreInterface {
public:
    explicit SimpleFrameBufferStore(std::size_t capacity);
    ~SimpleFrameBufferStore() override = default;

    AcquireOutcome acquire(const FrameBufferDescription& frame_buffer_description) override;
    bool view(FrameBufferHandle frame_buffer_handle, FrameBufferView& out_frame_buffer_view) override;
    void release(FrameBufferHandle frame_buffer_handle) override;
    void close() override;

private:
    struct BufferRecord {
        FrameBufferHandle frame_buffer_handle{0};

        std::int32_t width{0};
        std::int32_t height{0};
        std::int32_t stride_bytes{0};

        PixelFormat pixel_format{PixelFormat::Unknown};
        MemoryLocation memory_location{MemoryLocation::HostMemory};

        std::vector<std::uint8_t> host_memory_bytes;
        bool in_use{false};
    };

    FrameBufferHandle allocate_new_handle_unsafe();

    std::mutex mutex_;
    std::vector<BufferRecord> buffers_;
    std::unordered_map<FrameBufferHandle, std::size_t> handle_to_index_;

    std::atomic<bool> closed_{false};
    FrameBufferHandle next_handle_{1};
};

}  // namespace stream_pipeline
