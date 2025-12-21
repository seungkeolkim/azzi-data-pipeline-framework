#include "stream_pipeline/concurrency/bounded_pointer_queue.hpp"
#include "stream_pipeline/metadata/channel_state.hpp"
#include "stream_pipeline/runtime/pipeline_runner.hpp"
#include "stream_pipeline/stores/simple_frame_buffer_store.hpp"
#include "stream_pipeline/stores/simple_frame_metadata_store.hpp"
#include "stream_pipeline/stores/simple_object_metadata_store.hpp"
#include "stream_pipeline/nodes/decode_node.hpp"
#include "stream_pipeline/nodes/dummy_detection_node.hpp"
#include "stream_pipeline/nodes/output_node.hpp"

#include <chrono>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

using namespace stream_pipeline;

int main() {
    // Stage 0: 단일 채널
    ChannelState channel_state{};
    channel_state.channel_identifier = 1;
    channel_state.channel_name = "channel_1";
    channel_state.configuration.frame_queue_capacity = 16;
    channel_state.configuration.target_frames_per_second = 15;
    channel_state.configuration.output_every_n_frames = 1;

    // Stores (Stage 0 simple implementations)
    SimpleFrameMetadataStore frame_metadata_store(/*capacity*/ 64);
    SimpleFrameBufferStore frame_buffer_store(/*capacity*/ 32);
    SimpleObjectMetadataStore object_metadata_store;

    // Queues:
    // - DecodeNode -> DummyDetectionNode
    // - DummyDetectionNode -> OutputNode
    BoundedPointerQueue<FrameMetadata*> decode_to_detection_queue(
        channel_state.configuration.frame_queue_capacity,
        QueueOverflowPolicy::DropOldestItem);

    BoundedPointerQueue<FrameMetadata*> detection_to_output_queue(
        channel_state.configuration.frame_queue_capacity,
        QueueOverflowPolicy::DropOldestItem);

    // Node configurations
    DecodeNode::Configuration decode_configuration{};
    decode_configuration.frame_width = 640;
    decode_configuration.frame_height = 360;
    decode_configuration.pixel_format = PixelFormat::RGB24;
    decode_configuration.process_every_n_frames = 1;
    decode_configuration.synthetic_frames_per_second = channel_state.configuration.target_frames_per_second;

    DummyDetectionNode::Configuration detection_configuration{};
    detection_configuration.bbox_width_ratio = 0.3f;
    detection_configuration.bbox_height_ratio = 0.3f;
    detection_configuration.class_identifier = 0;
    detection_configuration.confidence_score = 1.0f;

    OutputNode::Configuration output_configuration{};
    output_configuration.output_directory_path = "output";
    output_configuration.jsonl_file_name = "frames.jsonl";
    output_configuration.enable_ppm_output = true;
    output_configuration.write_image_every_n_frames = 5;
    output_configuration.enable_bbox_overlay = true;

    // Nodes
    std::vector<std::unique_ptr<NodeInterface>> nodes;
    nodes.push_back(std::make_unique<DecodeNode>(
        channel_state,
        frame_metadata_store,
        frame_buffer_store,
        decode_to_detection_queue,
        QueueOverflowPolicy::DropOldestItem,
        decode_configuration));

    nodes.push_back(std::make_unique<DummyDetectionNode>(
        object_metadata_store,
        frame_buffer_store,
        decode_to_detection_queue,
        detection_to_output_queue,
        detection_configuration));

    nodes.push_back(std::make_unique<OutputNode>(
        channel_state,
        frame_metadata_store,
        frame_buffer_store,
        object_metadata_store,
        detection_to_output_queue,
        output_configuration));

    // Runner
    PipelineRunner runner;
    runner.set_nodes(std::move(nodes));
    runner.set_queues_to_close({&decode_to_detection_queue, &detection_to_output_queue});

    // Run
    runner.start_all_nodes();

    // Stage 0: 일정 시간만 실행 후 종료 (운영에서는 signal/command로 제어)
    std::cout << "Stage 0 pipeline started. Running for 10 seconds...\n";
    std::this_thread::sleep_for(std::chrono::seconds(10));

    std::cout << "Stopping pipeline...\n";
    runner.stop_all_nodes();

    // Stores close (선택)
    object_metadata_store.close();
    frame_buffer_store.close();
    frame_metadata_store.close();

    std::cout << "Done.\n";
    return 0;
}
