#pragma once

#include "stream_pipeline/concurrency/bounded_pointer_queue.hpp"
#include "stream_pipeline/metadata/channel_state.hpp"
#include "stream_pipeline/metadata/channel_runtime_meta_store_interface.hpp"
#include "stream_pipeline/metadata/frame_metadata_store_interface.hpp"
#include "stream_pipeline/metadata/object_metadata_store_interface.hpp"
#include "stream_pipeline/metadata/node_runtime_state_store_interface.hpp"
#include "stream_pipeline/memory/frame_buffer_store_interface.hpp"
#include "stream_pipeline/runtime/node_interface.hpp"

#include <atomic>
#include <fstream>
#include <string>
#include <thread>

namespace stream_pipeline {

/*
 * 출력 노드
 * ---------
 * - 입력: 프레임 메타데이터 포인터(더미 탐지 노드가 전달)
 * - 처리:
 *   1) 객체 메타데이터 스토어에서 객체 핸들을 읽어 줄 단위 제이슨 출력
 *   2) 프레임 버퍼 뷰로 버퍼를 얻어 피피엠 저장 + 간단 오버레이 처리
 *   3) 해제 체인 수행:
 *      - 객체 메타데이터 해제
 *      - 프레임 버퍼 해제
 *      - 프레임 메타데이터 해제
 *
 * 합의:
 * - 출력 실패 시 드롭 처리한다. 파일 쓰기 실패가 파이프라인 중단을 유발하면 안 된다.
 */
class OutputNode final : public NodeInterface {
public:
    struct Configuration {
        std::string output_directory_path{"output"};
        std::string jsonl_file_name{"frames.jsonl"};

        // 이미지 출력 옵션(피피엠)
        bool enable_ppm_output{true};
        std::int32_t write_image_every_n_frames{5};

        // 오버레이 옵션(아주 단순한 사각형 테두리)
        bool enable_bbox_overlay{true};
    };

    OutputNode(
        ChannelState& channel_state,
        FrameMetadataStoreInterface& frame_metadata_store,
        FrameBufferStoreInterface& frame_buffer_store,
        ObjectMetadataStoreInterface& object_metadata_store,
        ChannelRuntimeMetaStoreInterface& channel_runtime_meta_store,
        NodeRuntimeStateStoreInterface& node_runtime_state_store,
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
    ChannelRuntimeMetaStoreInterface& channel_runtime_meta_store_;
    NodeRuntimeStateStoreInterface& node_runtime_state_store_;
    BoundedPointerQueue<FrameMetadata*>& input_queue_;

    Configuration configuration_;

    NodeInstanceIdentifier node_instance_identifier_{0};

    std::atomic<bool> running_{false};
    std::atomic<bool> stop_requested_{false};
    std::thread worker_thread_;
};

}
