#include "stream_pipeline/nodes/dummy_detection_node.hpp"

#include "stream_pipeline/metadata/object_metadata.hpp"
#include "stream_pipeline/utilities/time_utilities.hpp"
#include "stream_pipeline/utilities/logger.hpp"

namespace stream_pipeline {

DummyDetectionNode::DummyDetectionNode(
    ChannelState& channel_state,
    ObjectMetadataStoreInterface& object_metadata_store,
    FrameMetadataStoreInterface& frame_metadata_store,
    FrameBufferStoreInterface& frame_buffer_store,
    ChannelRuntimeMetaStoreInterface& channel_runtime_meta_store,
    NodeRuntimeStateStoreInterface& node_runtime_state_store,
    BoundedPointerQueue<FrameMetadata*>& input_queue,
    BoundedPointerQueue<FrameMetadata*>& output_queue,
    const Configuration& configuration)
    : channel_state_(channel_state),
      object_metadata_store_(object_metadata_store),
      frame_metadata_store_(frame_metadata_store),
      frame_buffer_store_(frame_buffer_store),
      channel_runtime_meta_store_(channel_runtime_meta_store),
      node_runtime_state_store_(node_runtime_state_store),
      input_queue_(input_queue),
      output_queue_(output_queue),
      configuration_(configuration) {
    node_instance_identifier_ = 3;
}

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
    Logger::instance().log_with_node(Logger::Level::Info, node_name(), "start requested");
    worker_thread_ = std::thread(&DummyDetectionNode::thread_entry_, this);
    return StartResult::Success;
}

void DummyDetectionNode::stop() {
    if (!running_.load()) {
        return;
    }

    stop_requested_.store(true);

    // 입력 큐가 블로킹 팝이므로, 종료를 원활하게 하려면 닫기가 필요하다.
    // 이 닫기 책임은 상위 실행 관리자에서 수행하는 것이 더 명확하다.
    // 여기서는 조인만 수행한다.
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
    running_.store(false);

    Logger::instance().log_with_node(Logger::Level::Info, node_name(), "stopped");
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

        node_runtime_state_store_.write(node_instance_identifier_, [](NodeRuntimeState& state) {
            state.input_count.fetch_add(1);
        });

        // 드롭된 프레임이라면, 0 단계에서는 그대로 다음으로 넘기지 않고
        // 출력 노드에서 정리하도록 하는 편이 단순하다.
        // 다만 0 단계에서는 드롭이 대부분 디코드 노드에서 정리되어 해제되었을 가능성이 높다.
        if (frame_metadata_pointer->dropped) {
            // 그래도 흐름을 유지하기 위해 출력 큐로 전달한다(정리 책임을 출력 노드로 일원화).
            output_queue_.push(frame_metadata_pointer);
            continue;
        }

        // 프레임 크기를 알기 위해 버퍼 뷰를 얻는다.
        FrameBufferView view{};
        if (!frame_buffer_store_.view(frame_metadata_pointer->frame_buffer_handle, view) ||
            view.width <= 0 || view.height <= 0) {

            // 뷰 실패 시: 탐지를 생략하고 출력으로 넘긴다.
            // (0 단계 정책: 실패는 해당 프레임 기능만 드롭, 파이프라인은 계속)
            output_queue_.push(frame_metadata_pointer);
            continue;
        }

        // 중앙 고정 바운딩 박스 1개 생성
        const float bbox_width = static_cast<float>(view.width) * configuration_.bbox_width_ratio;
        const float bbox_height = static_cast<float>(view.height) * configuration_.bbox_height_ratio;

        const float left = (static_cast<float>(view.width) - bbox_width) * 0.5f;
        const float top = (static_cast<float>(view.height) - bbox_height) * 0.5f;

        ObjectMetadata object_metadata{};
        object_metadata.object_identifier = static_cast<ObjectIdentifier>(frame_metadata_pointer->frame_identifier);  // 0 단계: 임시 매핑
        object_metadata.class_identifier = configuration_.class_identifier;
        object_metadata.confidence_score = configuration_.confidence_score;
        object_metadata.bounding_box = BoundingBox{left, top, bbox_width, bbox_height};
        object_metadata.tracking_identifier = -1;

        // 스토어에 등록하고 핸들을 프레임에 붙인다
        auto create_outcome = object_metadata_store_.create(object_metadata);
        if (create_outcome.result == ObjectMetadataStoreInterface::CreateResult::Success &&
            create_outcome.object_handle != 0) {

            frame_metadata_pointer->object_handles.push_back(create_outcome.object_handle);
        }

        // 탐지 완료 시각 기록
        const TimestampPair detection_timestamp = now_timestamp_pair();
        frame_metadata_pointer->detection_completed_timestamp = detection_timestamp;

        // 채널 런타임 메타 업데이트(간단한 패턴으로 차량 관측 기록)
        const bool vehicle_observed = (frame_metadata_pointer->frame_identifier % 2ULL) == 0ULL;
        if (vehicle_observed) {
            channel_runtime_meta_store_.write(channel_state_.channel_identifier, [&](ChannelRuntimeMeta& meta) {
                meta.last_seen_vehicle_monotonic_ns = detection_timestamp.monotonic_time_nanoseconds;
            });
        }

        // 다음 단계로 전달
        auto push_outcome = output_queue_.push(frame_metadata_pointer);

        if (push_outcome.result == BoundedPointerQueue<FrameMetadata*>::PushResult::QueueClosed) {
            for (const ObjectHandle object_handle : frame_metadata_pointer->object_handles) {
                object_metadata_store_.release(object_handle);
            }
            frame_metadata_pointer->object_handles.clear();
            frame_buffer_store_.release(frame_metadata_pointer->frame_buffer_handle);
            frame_metadata_store_.release(frame_metadata_pointer);

            node_runtime_state_store_.write(node_instance_identifier_, [](NodeRuntimeState& state) {
                state.dropped_count.fetch_add(1);
            });
            break;
        }

        if (push_outcome.dropped_old_pointer != nullptr) {
            FrameMetadata* dropped_frame_metadata_pointer = push_outcome.dropped_old_pointer;
            dropped_frame_metadata_pointer->dropped = true;
            dropped_frame_metadata_pointer->drop_reason = "detection_output_queue_full_drop_oldest";

            for (const ObjectHandle object_handle : dropped_frame_metadata_pointer->object_handles) {
                object_metadata_store_.release(object_handle);
            }
            dropped_frame_metadata_pointer->object_handles.clear();
            frame_buffer_store_.release(dropped_frame_metadata_pointer->frame_buffer_handle);
            frame_metadata_store_.release(dropped_frame_metadata_pointer);

            channel_state_.dropped_frame_count.fetch_add(1);
            node_runtime_state_store_.write(node_instance_identifier_, [](NodeRuntimeState& state) {
                state.dropped_count.fetch_add(1);
            });

            Logger::instance().log_with_node(
                Logger::Level::Warning,
                node_name(),
                "dropped oldest frame in detection->output queue");
        }

        node_runtime_state_store_.write(node_instance_identifier_, [](NodeRuntimeState& state) {
            state.output_count.fetch_add(1);
        });
    }
}

}
