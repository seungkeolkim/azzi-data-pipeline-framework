#include "stream_pipeline/nodes/dummy_detection_node.hpp"

#include "stream_pipeline/metadata/object_metadata.hpp"
#include "stream_pipeline/utilities/time_utilities.hpp"

namespace stream_pipeline {

DummyDetectionNode::DummyDetectionNode(
    ObjectMetadataStoreInterface& object_metadata_store,
    FrameBufferStoreInterface& frame_buffer_store,
    BoundedPointerQueue<FrameMetadata*>& input_queue,
    BoundedPointerQueue<FrameMetadata*>& output_queue,
    const Configuration& configuration)
    : object_metadata_store_(object_metadata_store),
      frame_buffer_store_(frame_buffer_store),
      input_queue_(input_queue),
      output_queue_(output_queue),
      configuration_(configuration) {}

DummyDetectionNode::~DummyDetectionNode() {
    stop();
}

const char* DummyDetectionNode::node_name() const {
    return "DummyDetectionNode";
}

NodeInterface::StartResult DummyDetectionNode::start() {
    if (running_.load()) {
        return StartResult::AlreadyRunning;
    }
    stop_requested_.store(false);
    running_.store(true);
    worker_thread_ = std::thread(&DummyDetectionNode::thread_entry_, this);
    return StartResult::Success;
}

void DummyDetectionNode::stop() {
    if (!running_.load()) {
        return;
    }

    stop_requested_.store(true);

    // input_queue_가 blocking pop이므로, 종료를 원활하게 하려면 close가 필요하다.
    // 이 close 책임은 상위 runner에서 수행하는 것이 더 명확하다.
    // 여기서는 join만 수행한다.
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
    running_.store(false);
}

void DummyDetectionNode::thread_entry_() {
    while (!stop_requested_.load()) {
        auto pop_outcome = input_queue_.pop_blocking();
        if (pop_outcome.result == BoundedPointerQueue<FrameMetadata*>::PopResult::QueueClosedAndEmpty) {
            break;
        }

        FrameMetadata* frame_metadata_pointer = pop_outcome.dequeued_pointer;
        if (frame_metadata_pointer == nullptr) {
            continue;
        }

        // dropped 프레임이라면, Stage 0에서는 그대로 다음으로 넘기지 않고
        // OutputNode에서 정리하도록 하는 편이 단순하다.
        // 다만 Stage 0에서는 drop이 대부분 DecodeNode에서 정리되어 release 되었을 가능성이 높다.
        if (frame_metadata_pointer->dropped) {
            // 그래도 흐름을 유지하기 위해 output_queue로 전달한다(정리 책임을 OutputNode로 일원화).
            output_queue_.push(frame_metadata_pointer);
            continue;
        }

        // frame 크기를 알기 위해 buffer view를 얻는다.
        FrameBufferView view{};
        if (!frame_buffer_store_.view(frame_metadata_pointer->frame_buffer_handle, view) ||
            view.width <= 0 || view.height <= 0) {

            // view 실패 시: detection을 생략하고 output으로 넘긴다.
            // (Stage 0 정책: 실패는 해당 프레임 기능만 drop, 파이프라인은 계속)
            output_queue_.push(frame_metadata_pointer);
            continue;
        }

        // 중앙 고정 bbox 1개 생성
        const float bbox_width = static_cast<float>(view.width) * configuration_.bbox_width_ratio;
        const float bbox_height = static_cast<float>(view.height) * configuration_.bbox_height_ratio;

        const float left = (static_cast<float>(view.width) - bbox_width) * 0.5f;
        const float top = (static_cast<float>(view.height) - bbox_height) * 0.5f;

        ObjectMetadata object_metadata{};
        object_metadata.object_identifier = static_cast<ObjectIdentifier>(frame_metadata_pointer->frame_identifier);  // Stage0: 임시 매핑
        object_metadata.class_identifier = configuration_.class_identifier;
        object_metadata.confidence_score = configuration_.confidence_score;
        object_metadata.bounding_box = BoundingBox{left, top, bbox_width, bbox_height};
        object_metadata.tracking_identifier = -1;

        // store에 등록하고 handle을 frame에 attach
        auto create_outcome = object_metadata_store_.create(object_metadata);
        if (create_outcome.result == ObjectMetadataStoreInterface::CreateResult::Success &&
            create_outcome.object_handle != 0) {

            frame_metadata_pointer->object_handles.push_back(create_outcome.object_handle);
        }

        // stage timestamp 기록
        frame_metadata_pointer->detection_completed_timestamp = now_timestamp_pair();

        // 다음 단계로 전달
        output_queue_.push(frame_metadata_pointer);
    }
}

}  // namespace stream_pipeline
