#pragma once

#include "stream_pipeline/concurrency/bounded_pointer_queue.hpp"
#include "stream_pipeline/metadata/channel_state.hpp"
#include "stream_pipeline/metadata/frame_metadata_store_interface.hpp"
#include "stream_pipeline/metadata/input_source_queue_item.hpp"
#include "stream_pipeline/metadata/node_runtime_state_store_interface.hpp"
#include "stream_pipeline/memory/frame_buffer_store_interface.hpp"
#include "stream_pipeline/memory/input_source_data_buffer_store_interface.hpp"
#include "stream_pipeline/runtime/node_interface.hpp"

#include <atomic>
#include <cstdint>
#include <thread>

namespace stream_pipeline {

/*
 * 디코드 노드
 * -----------
 * - 입력 소스 노드와 물리적으로 분리된 디코드 전용 노드다.
 * - 입력은 입력 소스 큐 아이템이며, 출력은 프레임 메타데이터다.
 *
 * 필수 구조 원칙:
 * - 입력 소스와 디코드는 반드시 분리한다.
 * - 향후 메모리 이동 노드 삽입 가능성을 남긴다.
 * - 하드웨어 디코더와 장치 메모리 대응 구조를 유지한다.
 */
class DecodeNode final : public NodeInterface {
public:
    struct Configuration {
        std::int32_t frame_width{640};
        std::int32_t frame_height{360};

        // 0.6 단계 단순화: 픽셀 포맷을 하나로 고정
        PixelFormat pixel_format{PixelFormat::RGB24};

        // 디코드 단계에서의 샘플링 비율
        std::int32_t process_every_n_frames{1};
    };

    DecodeNode(
        ChannelState& channel_state,
        FrameMetadataStoreInterface& frame_metadata_store,
        FrameBufferStoreInterface& frame_buffer_store,
        InputSourceDataBufferStoreInterface& input_source_data_buffer_store,
        NodeRuntimeStateStoreInterface& node_runtime_state_store,
        BoundedPointerQueue<InputSourceQueueItem*>& input_queue,
        BoundedPointerQueue<FrameMetadata*>& frame_metadata_queue,
        QueueOverflowPolicy overflow_policy,
        const Configuration& configuration);

    ~DecodeNode() override;

    const char* node_name() const override;

    StartResult start() override;
    void stop() override;

private:
    void thread_entry_();
    void release_input_item_(InputSourceQueueItem* item_pointer);

    void fill_synthetic_rgb24_frame_(
        std::uint8_t* rgb_data_pointer,
        std::int32_t width,
        std::int32_t height,
        std::int32_t stride_bytes,
        std::uint64_t frame_identifier);

    ChannelState& channel_state_;
    FrameMetadataStoreInterface& frame_metadata_store_;
    FrameBufferStoreInterface& frame_buffer_store_;
    InputSourceDataBufferStoreInterface& input_source_data_buffer_store_;
    NodeRuntimeStateStoreInterface& node_runtime_state_store_;
    BoundedPointerQueue<InputSourceQueueItem*>& input_queue_;
    BoundedPointerQueue<FrameMetadata*>& frame_metadata_queue_;

    QueueOverflowPolicy overflow_policy_;
    Configuration configuration_;

    NodeInstanceIdentifier node_instance_identifier_{0};

    std::atomic<bool> running_{false};
    std::atomic<bool> stop_requested_{false};
    std::thread worker_thread_;
};

}
