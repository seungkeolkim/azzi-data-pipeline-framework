#include "stream_pipeline/concurrency/bounded_pointer_queue.hpp"
#include "stream_pipeline/metadata/channel_state.hpp"
#include "stream_pipeline/runtime/pipeline_runner.hpp"
#include "stream_pipeline/stores/input_source_data_buffer_store.hpp"
#include "stream_pipeline/stores/simple_frame_buffer_store.hpp"
#include "stream_pipeline/stores/simple_frame_metadata_store.hpp"
#include "stream_pipeline/stores/simple_object_metadata_store.hpp"
#include "stream_pipeline/stores/simple_channel_runtime_meta_store.hpp"
#include "stream_pipeline/stores/simple_node_runtime_state_store.hpp"
#include "stream_pipeline/nodes/dummy_input_source_node.hpp"
#include "stream_pipeline/nodes/decode_node.hpp"
#include "stream_pipeline/nodes/dummy_detection_node.hpp"
#include "stream_pipeline/nodes/output_node.hpp"
#include "stream_pipeline/utilities/logger.hpp"

#include <chrono>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

using namespace stream_pipeline;

int main() {
    // 0.6 단계: 단일 채널
    ChannelState channel_state{};
    channel_state.channel_identifier = 1;
    channel_state.channel_name = "channel_1";
    channel_state.configuration.frame_queue_capacity = 16;
    channel_state.configuration.target_frames_per_second = 15;
    channel_state.configuration.output_every_n_frames = 1;

    // 스토어 구성
    SimpleFrameMetadataStore frame_metadata_store(/*capacity*/ 64);
    SimpleFrameBufferStore frame_buffer_store(/*capacity*/ 32);
    InputSourceDataBufferStore input_source_data_buffer_store(/*capacity*/ 32);
    SimpleObjectMetadataStore object_metadata_store;
    SimpleChannelRuntimeMetaStore channel_runtime_meta_store;
    SimpleNodeRuntimeStateStore node_runtime_state_store;

    node_runtime_state_store.register_node(1, "DummyInputSourceNode");
    node_runtime_state_store.register_node(2, "DecodeNode");
    node_runtime_state_store.register_node(3, "DummyDetectionNode");
    node_runtime_state_store.register_node(4, "OutputNode");

    // 큐 구성
    BoundedPointerQueue<InputSourceQueueItem*> input_source_to_decode_queue(
        channel_state.configuration.frame_queue_capacity,
        QueueOverflowPolicy::DropOldestItem);

    BoundedPointerQueue<FrameMetadata*> decode_to_detection_queue(
        channel_state.configuration.frame_queue_capacity,
        QueueOverflowPolicy::DropOldestItem);

    BoundedPointerQueue<FrameMetadata*> detection_to_output_queue(
        channel_state.configuration.frame_queue_capacity,
        QueueOverflowPolicy::DropOldestItem);

    // 노드 설정
    DummyInputSourceNode::Configuration input_source_configuration{};
    input_source_configuration.size_bytes_per_item = 1024;
    input_source_configuration.items_per_second = channel_state.configuration.target_frames_per_second;

    DecodeNode::Configuration decode_configuration{};
    decode_configuration.frame_width = 640;
    decode_configuration.frame_height = 360;
    decode_configuration.pixel_format = PixelFormat::RGB24;
    decode_configuration.process_every_n_frames = 1;

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

    // 노드 구성
    std::vector<std::unique_ptr<NodeInterface>> nodes;
    nodes.push_back(std::make_unique<DummyInputSourceNode>(
        channel_state,
        input_source_data_buffer_store,
        node_runtime_state_store,
        input_source_to_decode_queue,
        input_source_configuration,
        InputSourceNode::InputSourceMode::Pull));

    nodes.push_back(std::make_unique<DecodeNode>(
        channel_state,
        frame_metadata_store,
        frame_buffer_store,
        input_source_data_buffer_store,
        node_runtime_state_store,
        input_source_to_decode_queue,
        decode_to_detection_queue,
        QueueOverflowPolicy::DropOldestItem,
        decode_configuration));

    nodes.push_back(std::make_unique<DummyDetectionNode>(
        channel_state,
        object_metadata_store,
        frame_metadata_store,
        frame_buffer_store,
        channel_runtime_meta_store,
        node_runtime_state_store,
        decode_to_detection_queue,
        detection_to_output_queue,
        detection_configuration));

    nodes.push_back(std::make_unique<OutputNode>(
        channel_state,
        frame_metadata_store,
        frame_buffer_store,
        object_metadata_store,
        channel_runtime_meta_store,
        node_runtime_state_store,
        detection_to_output_queue,
        output_configuration));

    PipelineRunner runner;
    runner.set_nodes(std::move(nodes));
    runner.set_input_source_queues_to_close({&input_source_to_decode_queue});
    runner.set_queues_to_close({&decode_to_detection_queue, &detection_to_output_queue});

    runner.start_all_nodes();

    std::cout << "0.6 단계 파이프라인 시작. 10초 동안 실행합니다.\n";
    std::this_thread::sleep_for(std::chrono::seconds(10));

    std::cout << "파이프라인 종료 중...\n";
    runner.stop_all_nodes();

    // 스토어 종료
    object_metadata_store.close();
    frame_buffer_store.close();
    frame_metadata_store.close();
    input_source_data_buffer_store.close();

    std::cout << "완료.\n";
    return 0;
}
