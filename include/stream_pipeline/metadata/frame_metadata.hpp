#pragma once

#include "stream_pipeline/common/identifiers.hpp"
#include "stream_pipeline/common/time_types.hpp"
#include "stream_pipeline/memory/frame_buffer_types.hpp"
#include "stream_pipeline/metadata/object_metadata_store_interface.hpp"
#include <cstdint>
#include <vector>

namespace stream_pipeline {

/*
 * FrameMetadata
 * -------------
 * - 프레임 단위 Anchor.
 * - 큐를 통해 흐르는 핵심 메시지는 FrameMetadata* 포인터이다.
 *
 * Ownership / Lifetime (핵심 계약):
 * - FrameMetadata 객체: FrameMetadataStoreInterface가 소유/관리
 * - 프레임 버퍼: FrameBufferStoreInterface가 소유/관리 (frame_buffer_handle만 보관)
 * - 객체 메타: ObjectMetadataStoreInterface가 소유/관리 (object_handles만 보관)
 *
 * Stage 0 파이프라인:
 * - DecodeNode가 FrameMetadata를 acquire하여 frame_id, timestamps, buffer handle 설정 후 큐에 push
 * - DummyDetectionNode가 object 생성 후 object_handle을 frame.object_handles에 attach
 * - OutputNode가 view/read로 데이터 접근, 결과 저장 후 object/buffer/frame 순서로 release
 */
struct FrameMetadata {
    ChannelIdentifier channel_identifier{0};
    FrameIdentifier frame_identifier{0};

    // 스트림 PTS(있으면). Stage 0에서 -1 허용.
    std::int64_t presentation_timestamp{-1};

    // realtime/playback/external 구분
    StreamTimeBase stream_time_base{StreamTimeBase::Realtime};

    // stage별 완료 시각
    TimestampPair decode_completed_timestamp{};
    TimestampPair detection_completed_timestamp{};
    TimestampPair output_completed_timestamp{};

    // FrameBufferStoreInterface 소유 버퍼 핸들
    FrameBufferHandle frame_buffer_handle{0};

    // ObjectMetadataStoreInterface 소유 객체 핸들들
    std::vector<ObjectHandle> object_handles;

    // drop 여부 (queue overflow 정책 등으로 drop된 경우)
    bool dropped{false};

    // drop_reason은 Stage 0에서는 string literal 사용 권장(동적 할당/formatting 금지)
    const char* drop_reason{nullptr};
};

}  // namespace stream_pipeline
