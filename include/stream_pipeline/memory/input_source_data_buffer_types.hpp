#pragma once

#include "stream_pipeline/memory/frame_buffer_types.hpp"

#include <cstddef>
#include <cstdint>

namespace stream_pipeline {

/*
 * 입력 소스 데이터 버퍼 핸들
 * -------------------------
 * - 입력 소스 데이터 버퍼를 가리키는 불투명 핸들.
 * - 데이터의 유일 소유자는 스토어이며, 노드와 큐는 핸들만 전달한다.
 */
using InputSourceDataBufferHandle = std::uint64_t;

/*
 * 입력 소스 데이터 버퍼 뷰
 * ------------------------
 * - 핸들이 가리키는 메모리를 임시 참조로 노출한다.
 * - 소유권은 없으며, 핸들 반환 이후에는 무효가 될 수 있다.
 */
struct InputSourceDataBufferView {
    MemoryLocation memory_location{MemoryLocation::HostMemory};
    void* data_pointer{nullptr};
    std::size_t size_bytes{0};
    std::int32_t device_identifier{-1};
};

}
