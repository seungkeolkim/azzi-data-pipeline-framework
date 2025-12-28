#pragma once

#include "stream_pipeline/concurrency/bounded_pointer_queue.hpp"
#include "stream_pipeline/metadata/channel_state.hpp"
#include "stream_pipeline/metadata/frame_metadata_store_interface.hpp"
#include "stream_pipeline/metadata/node_runtime_state_store_interface.hpp"
#include "stream_pipeline/memory/frame_buffer_store_interface.hpp"
#include "stream_pipeline/runtime/node_interface.hpp"

#include <atomic>
#include <cstdint>
#include <thread>

namespace stream_pipeline {

/*
 * DecodeNode (Stage 0)
 * -------------------
 * - Stage 0에서는 InputSourceNode를 분리하지 않고,
 *   DecodeNode 내부에서 “입력 획득 + decode(또는 stub) + enqueue”까지 수행한다.
 *
 * 중요:
 * - Stage 1에서 InputSourceNode를 분리할 예정이다.
 * - 따라서 DecodeNode는 “나중에 입력부를 뽑아낼 수 있게” 구현을 과도하게 엮지 않는다.
 *
 * Stage 0 단순화:
 * - 실제 RTSP decode 대신, Synthetic frame(더미 프레임)을 생성한다.
 * - 목표는 E2E 파이프라인 완주 및 메타/큐/스토어 계약 검증이다.
 */
class DecodeNode final : public NodeInterface {
public:
    struct Configuration {
        std::int32_t frame_width{640};
        std::int32_t frame_height{360};

        // Stage 0: RGB24로 단순화
        PixelFormat pixel_format{PixelFormat::RGB24};

        // decode 단계에서의 throttling (N프레임 중 1 프레임)
        std::int32_t process_every_n_frames{1};

        // synthetic source frame rate (sleep 기반, 정확한 realtime 동기화는 Stage 0 범위 밖)
        std::int32_t synthetic_frames_per_second{15};
    };

    DecodeNode(
        ChannelState& channel_state,
        FrameMetadataStoreInterface& frame_metadata_store,
        FrameBufferStoreInterface& frame_buffer_store,
        NodeRuntimeStateStoreInterface& node_runtime_state_store,
        BoundedPointerQueue<FrameMetadata*>& frame_metadata_queue,
        QueueOverflowPolicy overflow_policy,
        const Configuration& configuration);

    ~DecodeNode() override;

    const char* node_name() const override;

    StartResult start() override;
    void stop() override;

private:
    void thread_entry_();

    void fill_synthetic_rgb24_frame_(
        std::uint8_t* rgb_data_pointer,
        std::int32_t width,
        std::int32_t height,
        std::int32_t stride_bytes,
        std::uint64_t frame_identifier);

    ChannelState& channel_state_;
    FrameMetadataStoreInterface& frame_metadata_store_;
    FrameBufferStoreInterface& frame_buffer_store_;
    NodeRuntimeStateStoreInterface& node_runtime_state_store_;
    BoundedPointerQueue<FrameMetadata*>& frame_metadata_queue_;

    QueueOverflowPolicy overflow_policy_;
    Configuration configuration_;

    NodeInstanceIdentifier node_instance_identifier_{0};

    std::atomic<bool> running_{false};
    std::atomic<bool> stop_requested_{false};
    std::thread worker_thread_;
};

}  // namespace stream_pipeline
