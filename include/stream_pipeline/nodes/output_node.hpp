#pragma once

#include "stream_pipeline/concurrency/bounded_pointer_queue.hpp"
#include "stream_pipeline/metadata/channel_state.hpp"
#include "stream_pipeline/metadata/frame_metadata_store_interface.hpp"
#include "stream_pipeline/metadata/object_metadata_store_interface.hpp"
#include "stream_pipeline/memory/frame_buffer_store_interface.hpp"
#include "stream_pipeline/runtime/node_interface.hpp"

#include <atomic>
#include <fstream>
#include <string>
#include <thread>

namespace stream_pipeline {

/*
 * OutputNode (Stage 0)
 * -------------------
 * - 입력: FrameMetadata* (DummyDetectionNode가 전달)
 * - 처리:
 *   1) ObjectMetadataStore에서 object_handle들을 read하여 JSONL 출력
 *   2) FrameBufferStore view로 버퍼를 얻어 PPM(옵션) 저장 + 간단 overlay(옵션)
 *   3) release chain 수행:
 *      - object_metadata_store.release(object_handle...)
 *      - frame_buffer_store.release(frame_buffer_handle)
 *      - frame_metadata_store.release(frame_metadata_pointer)
 *
 * 합의:
 * - Sink 실패 시 drop. 즉, 파일 쓰기 실패가 파이프라인 중단을 유발하면 안 된다.
 */
class OutputNode final : public NodeInterface {
public:
    struct Configuration {
        std::string output_directory_path{"output"};
        std::string jsonl_file_name{"frames.jsonl"};

        // 이미지 출력 옵션 (PPM)
        bool enable_ppm_output{true};
        std::int32_t write_image_every_n_frames{5};

        // overlay 옵션(아주 단순한 사각형 테두리)
        bool enable_bbox_overlay{true};
    };

    OutputNode(
        ChannelState& channel_state,
        FrameMetadataStoreInterface& frame_metadata_store,
        FrameBufferStoreInterface& frame_buffer_store,
        ObjectMetadataStoreInterface& object_metadata_store,
        BoundedPointerQueue<FrameMetadata*>& input_queue,
        const Configuration& configuration);

    ~OutputNode() override;

    const char* node_name() const override;
    StartResult start() override;
    void stop() override;

private:
    void thread_entry_();

    void overlay_bounding_box_rgb24_(
        std::uint8_t* rgb_data_pointer,
        std::int32_t width,
        std::int32_t height,
        std::int32_t stride_bytes,
        const BoundingBox& bounding_box);

    void write_jsonl_record_(
        std::ofstream& jsonl_stream,
        const FrameMetadata& frame_metadata,
        const std::vector<ObjectMetadata>& objects);

    ChannelState& channel_state_;
    FrameMetadataStoreInterface& frame_metadata_store_;
    FrameBufferStoreInterface& frame_buffer_store_;
    ObjectMetadataStoreInterface& object_metadata_store_;
    BoundedPointerQueue<FrameMetadata*>& input_queue_;

    Configuration configuration_;

    std::atomic<bool> running_{false};
    std::atomic<bool> stop_requested_{false};
    std::thread worker_thread_;
};

}  // namespace stream_pipeline
