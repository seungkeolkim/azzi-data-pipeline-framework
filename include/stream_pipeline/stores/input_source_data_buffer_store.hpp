#pragma once

#include "stream_pipeline/memory/input_source_data_buffer_store_interface.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace stream_pipeline {

/*
 * 입력 소스 데이터 버퍼 스토어
 * ----------------------------
 * - 입력 소스에서 생성되는 원시 또는 인코딩 데이터를 담는 전용 스토어.
 *
 * 절대 불변 원칙:
 * - 복사 없는 전달은 절대 불변 원칙이다.
 *
 * 확장 여지:
 * - 현재는 호스트 메모리만 지원한다.
 * - 향후 장치 메모리, 하드웨어 디코더, 직접 메모리 접근 확장이 가능하다.
 *
 * 주의:
 * - 이 스토어는 입력 소스 전용이며 디코드 완료 프레임용이 아니다.
 */
class InputSourceDataBufferStore final : public InputSourceDataBufferStoreInterface {
public:
    explicit InputSourceDataBufferStore(std::size_t capacity);
    ~InputSourceDataBufferStore() override = default;

    AcquireOutcome acquire(std::size_t size_bytes) override;
    bool view(InputSourceDataBufferHandle buffer_handle, InputSourceDataBufferView& out_view) override;
    void release(InputSourceDataBufferHandle buffer_handle) override;
    void close() override;

private:
    struct BufferRecord {
        InputSourceDataBufferHandle buffer_handle{0};
        std::size_t size_bytes{0};
        MemoryLocation memory_location{MemoryLocation::HostMemory};
        std::vector<std::uint8_t> host_memory_bytes;
        bool in_use{false};
    };

    InputSourceDataBufferHandle allocate_new_handle_unsafe();

    std::mutex mutex_;
    std::vector<BufferRecord> buffers_;
    std::unordered_map<InputSourceDataBufferHandle, std::size_t> handle_to_index_;

    std::atomic<bool> closed_{false};
    InputSourceDataBufferHandle next_handle_{1};
};

}
