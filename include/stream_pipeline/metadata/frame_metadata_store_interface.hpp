#pragma once

#include "stream_pipeline/metadata/frame_metadata.hpp"
#include <cstdint>

namespace stream_pipeline {

/*
 * FrameMetadataStoreInterface
 * ---------------------------
 * - FrameMetadata 객체 메모리의 lifecycle을 관리한다.
 * - 큐에는 FrameMetadata*만 흐르므로, drop 발생 시에도 반드시 이 store로 반환되어야 한다.
 *
 * Stage 0:
 * - 단순 pool 구현으로 충분.
 *
 * Stage 1/2:
 * - lock-free pool, arena, diagnostics 등을 내부 구현으로 발전 가능.
 * - 외부 계약은 유지하는 것이 목표.
 */
class FrameMetadataStoreInterface {
public:
    virtual ~FrameMetadataStoreInterface() = default;

    enum class AcquireResult : std::uint8_t {
        Success = 0,
        StoreClosed,
        OutOfMemory,
        InternalError
    };

    /*
     * AcquireOutcome
     * -------------
     * - field 이름은 frame_metadata_pointer로 명확히.
     */
    struct AcquireOutcome {
        AcquireResult result{AcquireResult::InternalError};
        FrameMetadata* frame_metadata_pointer{nullptr};
    };

    /*
     * acquire()
     * ---------
     * - channel_identifier에 대한 FrameMetadata 객체를 확보한다.
     * - frame_id/timestamps/handles 등은 Node가 채운다(책임 분리).
     */
    virtual AcquireOutcome acquire(ChannelIdentifier channel_identifier) = 0;

    /*
     * release()
     * ---------
     * - FrameMetadata 객체를 store로 반환한다.
     * - Stage 0 권장 정리 순서:
     *   1) ObjectMetadataStoreInterface::release(object_handle...)  (OutputNode에서)
     *   2) FrameBufferStoreInterface::release(frame_buffer_handle)  (OutputNode에서)
     *   3) FrameMetadataStoreInterface::release(frame_metadata_pointer)
     */
    virtual void release(FrameMetadata* frame_metadata_pointer) = 0;

    virtual void close() = 0;
};

}  // namespace stream_pipeline
