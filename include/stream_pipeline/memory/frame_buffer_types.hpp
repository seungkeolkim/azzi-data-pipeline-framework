#pragma once

#include <cstdint>

namespace stream_pipeline {

/*
 * PixelFormat
 * -----------
 * - 픽셀 데이터의 채널/레이아웃.
 * - Stage 0에서는 decode를 stub로 둬도 되므로 “슬롯” 수준.
 * - Stage 1/2로 갈수록 preprocess, overlay, encoder와 직접적으로 연결된다.
 */
enum class PixelFormat : std::uint8_t {
    Unknown = 0,
    NV12,
    RGB24,
    BGR24,
    RGBA
};

/*
 * MemoryLocation
 * --------------
 * - 프레임 버퍼가 어느 메모리에 존재하는지.
 * - 최종 목표(Host↔Device copy 최소화, device-resident pipeline)를 위해 Stage 0부터 포함.
 */
enum class MemoryLocation : std::uint8_t {
    HostMemory = 0,
    DeviceMemory = 1,
    SharedDmaBuffer = 2
};

/*
 * FrameBufferHandle
 * -----------------
 * - FrameBufferStoreInterface 내부 버퍼를 참조하는 “불투명(opaque) 핸들”.
 * - FrameMetadata는 이 값만 보관한다.
 *
 * 왜 포인터 대신 핸들인가?
 * - 포인터는 복사/대입/이동 시 ownership과 lifetime 문제가 쉽게 깨진다.
 * - 핸들은 copy-safe하고, 구현 교체(풀/슬랩/디버그 allocator)에도 유리하다.
 */
using FrameBufferHandle = std::uint64_t;

/*
 * FrameBufferDescription
 * ----------------------
 * - “원하는 버퍼 조건”을 기술하는 구조체.
 * - stride/pitch는 store 구현의 정책(align/pitch)으로 결정될 수 있으므로 여기서 제외.
 *
 * 사용자 요구 반영:
 * - width/height 고정 정책이 필요하면 store 구현에서 고정으로 강제한다.
 */
struct FrameBufferDescription {
    std::int32_t width{0};
    std::int32_t height{0};
    PixelFormat pixel_format{PixelFormat::Unknown};
    MemoryLocation memory_location{MemoryLocation::HostMemory};
};

/*
 * FrameBufferView
 * --------------
 * - handle이 가리키는 실제 메모리를 “임시 참조”로 제공한다.
 * - 소유권이 아니며, handle release 이후에는 무효가 될 수 있다.
 *
 * Stage 0:
 * - output(overlay/png/jpg/mp4) 작업에서 CPU ptr 접근이 필요할 때 사용.
 *
 * Stage 2:
 * - DeviceMemory ptr(CUDA device pointer)도 view로 제공 가능해야 한다.
 */
struct FrameBufferView {
    MemoryLocation memory_location{MemoryLocation::HostMemory};

    // 실제 메모리 포인터:
    // - HostMemory: CPU address
    // - DeviceMemory: CUDA device address
    void* data_pointer{nullptr};

    std::int32_t width{0};
    std::int32_t height{0};

    // row stride (pitch)
    std::int32_t stride_bytes{0};

    PixelFormat pixel_format{PixelFormat::Unknown};

    // 선택: multi-device/debug 상황에서 사용
    std::int32_t device_identifier{-1};
};

}  // namespace stream_pipeline
