#pragma once

#include "stream_pipeline/concurrency/bounded_pointer_queue.hpp"
#include "stream_pipeline/metadata/channel_runtime_meta_store_interface.hpp"
#include "stream_pipeline/metadata/frame_metadata_store_interface.hpp"
#include "stream_pipeline/metadata/object_metadata_store_interface.hpp"
#include "stream_pipeline/metadata/node_runtime_state_store_interface.hpp"
#include "stream_pipeline/metadata/channel_state.hpp"
#include "stream_pipeline/memory/frame_buffer_store_interface.hpp"
#include "stream_pipeline/runtime/node_interface.hpp"

#include <atomic>
#include <thread>

namespace stream_pipeline {

/*
 * 더미 탐지 노드
 * -------------
 * - 입력: 프레임 메타데이터 포인터(디코드 노드가 생성하여 큐로 전달)
 * - 처리: 프레임마다 중앙 고정 바운딩 박스 1개 생성
 * - 출력: 같은 프레임 메타데이터 포인터를 다음 큐로 전달
 *
 * 합의:
 * - 더미라도 정식 객체 메타데이터 계약을 사용한다.
 * - 결과는 객체 메타데이터 스토어에 생성하고, 프레임 메타데이터에는 객체 핸들만 붙인다.
 */
class DummyDetectionNode final : public NodeInterface {
public:
    struct Configuration {
        // 바운딩 박스 크기 비율 (0~1 사이 권장)
        float bbox_width_ratio{0.3f};
        float bbox_height_ratio{0.3f};

        // 고정 클래스 식별자 및 신뢰도
        std::int32_t class_identifier{0};
        float confidence_score{1.0f};
    };

    DummyDetectionNode(
        ChannelState& channel_state,
        ObjectMetadataStoreInterface& object_metadata_store,
        FrameMetadataStoreInterface& frame_metadata_store,
        FrameBufferStoreInterface& frame_buffer_store,
        ChannelRuntimeMetaStoreInterface& channel_runtime_meta_store,
        NodeRuntimeStateStoreInterface& node_runtime_state_store,
        BoundedPointerQueue<FrameMetadata*>& input_queue,
        BoundedPointerQueue<FrameMetadata*>& output_queue,
        const Configuration& configuration);

    ~DummyDetectionNode() override;

    const char* node_name() const override;
    StartResult start() override;
    void stop() override;

private:
    void thread_entry_();

    ChannelState& channel_state_;
    ObjectMetadataStoreInterface& object_metadata_store_;
    FrameMetadataStoreInterface& frame_metadata_store_;
    FrameBufferStoreInterface& frame_buffer_store_;
    ChannelRuntimeMetaStoreInterface& channel_runtime_meta_store_;
    NodeRuntimeStateStoreInterface& node_runtime_state_store_;

    BoundedPointerQueue<FrameMetadata*>& input_queue_;
    BoundedPointerQueue<FrameMetadata*>& output_queue_;

    Configuration configuration_;

    NodeInstanceIdentifier node_instance_identifier_{0};

    std::atomic<bool> running_{false};
    std::atomic<bool> stop_requested_{false};
    std::thread worker_thread_;
};

}
