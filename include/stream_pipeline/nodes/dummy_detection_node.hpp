#pragma once

#include "stream_pipeline/concurrency/bounded_pointer_queue.hpp"
#include "stream_pipeline/metadata/frame_metadata_store_interface.hpp"
#include "stream_pipeline/metadata/object_metadata_store_interface.hpp"
#include "stream_pipeline/runtime/node_interface.hpp"

#include <atomic>
#include <thread>

namespace stream_pipeline {

/*
 * DummyDetectionNode (Stage 0)
 * ----------------------------
 * - 입력: FrameMetadata* (DecodeNode가 생성하여 큐로 전달)
 * - 처리: 프레임마다 중앙 고정 bbox 1개 생성
 * - 출력: 같은 FrameMetadata*를 다음 큐로 전달
 *
 * 합의:
 * - Dummy라도 정식 ObjectMetadata 계약을 사용한다.
 * - 결과는 ObjectMetadataStoreInterface에 create하고, FrameMetadata에는 object_handle만 attach한다.
 */
class DummyDetectionNode final : public NodeInterface {
public:
    struct Configuration {
        // bbox 크기 비율 (0~1 사이 권장)
        float bbox_width_ratio{0.3f};
        float bbox_height_ratio{0.3f};

        // 고정 class_id 및 confidence
        std::int32_t class_identifier{0};
        float confidence_score{1.0f};
    };

    DummyDetectionNode(
        ObjectMetadataStoreInterface& object_metadata_store,
        FrameBufferStoreInterface& frame_buffer_store,
        BoundedPointerQueue<FrameMetadata*>& input_queue,
        BoundedPointerQueue<FrameMetadata*>& output_queue,
        const Configuration& configuration);

    ~DummyDetectionNode() override;

    const char* node_name() const override;
    StartResult start() override;
    void stop() override;

private:
    void thread_entry_();

    ObjectMetadataStoreInterface& object_metadata_store_;
    FrameBufferStoreInterface& frame_buffer_store_;

    BoundedPointerQueue<FrameMetadata*>& input_queue_;
    BoundedPointerQueue<FrameMetadata*>& output_queue_;

    Configuration configuration_;

    std::atomic<bool> running_{false};
    std::atomic<bool> stop_requested_{false};
    std::thread worker_thread_;
};

}  // namespace stream_pipeline
