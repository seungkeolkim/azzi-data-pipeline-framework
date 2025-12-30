#include "stream_pipeline/nodes/output_node.hpp"

#include "stream_pipeline/utilities/file_utilities.hpp"
#include "stream_pipeline/utilities/time_utilities.hpp"
#include "stream_pipeline/utilities/logger.hpp"

#include <algorithm>
#include <filesystem>
#include <sstream>

namespace stream_pipeline {

OutputNode::OutputNode(
    ChannelState& channel_state,
    FrameMetadataStoreInterface& frame_metadata_store,
    FrameBufferStoreInterface& frame_buffer_store,
    ObjectMetadataStoreInterface& object_metadata_store,
    ChannelRuntimeMetaStoreInterface& channel_runtime_meta_store,
    NodeRuntimeStateStoreInterface& node_runtime_state_store,
    BoundedPointerQueue<FrameMetadata*>& input_queue,
    const Configuration& configuration)
    : channel_state_(channel_state),
      frame_metadata_store_(frame_metadata_store),
      frame_buffer_store_(frame_buffer_store),
      object_metadata_store_(object_metadata_store),
      channel_runtime_meta_store_(channel_runtime_meta_store),
      node_runtime_state_store_(node_runtime_state_store),
      input_queue_(input_queue),
      configuration_(configuration) {
    node_instance_identifier_ = 4;
}

OutputNode::~OutputNode() {
    stop();
}

const char* OutputNode::node_name() const {
    return "OutputNode";
}

NodeInterface::StartResult OutputNode::start() {
    if (running_.load()) {
        return StartResult::AlreadyRunning;
    }
    stop_requested_.store(false);
    running_.store(true);
    Logger::instance().log_with_node(Logger::Level::Info, node_name(), "start requested");
    worker_thread_ = std::thread(&OutputNode::thread_entry_, this);
    return StartResult::Success;
}

void OutputNode::stop() {
    if (!running_.load()) {
        return;
    }
    stop_requested_.store(true);
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
    running_.store(false);

    Logger::instance().log_with_node(Logger::Level::Info, node_name(), "stopped");
}

void OutputNode::thread_entry_() {
    // 출력 디렉터리 준비
    ensure_directory_exists(configuration_.output_directory_path);

    const std::string jsonl_path =
        (std::filesystem::path(configuration_.output_directory_path) / configuration_.jsonl_file_name).string();

    std::ofstream jsonl_stream(jsonl_path, std::ios::out | std::ios::app);
    // 출력 스트림이 실패하더라도 파이프라인을 멈추면 안 된다.
    // 아래에서는 스트림 상태를 확인하며 실패 시 기록만 생략한다.

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

        // 1) 객체 핸들로 객체 메타데이터 읽기
        std::vector<ObjectMetadata> objects;
        objects.reserve(frame_metadata_pointer->object_handles.size());

        for (const ObjectHandle object_handle : frame_metadata_pointer->object_handles) {
            ObjectMetadata object{};
            if (object_metadata_store_.read(object_handle, object)) {
                objects.push_back(object);
            }
        }

        // 2) 줄 단위 제이슨 기록
        if (jsonl_stream.good()) {
            write_jsonl_record_(jsonl_stream, *frame_metadata_pointer, objects);
            jsonl_stream.flush();
        }

        TimestampPair output_timestamp = now_timestamp_pair();

        bool vehicle_present_now = false;
        channel_runtime_meta_store_.read(frame_metadata_pointer->channel_identifier, [&](const ChannelRuntimeMeta& meta) {
            vehicle_present_now = meta.is_vehicle_present(output_timestamp.monotonic_time_nanoseconds);
        });

        if ((frame_metadata_pointer->frame_identifier % 10ULL) == 0ULL) {
            Logger::instance().log_with_node(
                Logger::Level::Info,
                node_name(),
                vehicle_present_now
                    ? "vehicle presence detected in TTL window"
                    : "vehicle not observed within TTL window");
        }

        // 3) 이미지 출력(선택)
        FrameBufferView view{};
        const bool can_view = frame_buffer_store_.view(frame_metadata_pointer->frame_buffer_handle, view) &&
                              view.data_pointer != nullptr &&
                              view.pixel_format == PixelFormat::RGB24;

        if (configuration_.enable_ppm_output && can_view) {
            const int n = (configuration_.write_image_every_n_frames <= 0) ? 1 : configuration_.write_image_every_n_frames;
            if ((frame_metadata_pointer->frame_identifier % static_cast<std::uint64_t>(n)) == 0ULL) {
                // 오버레이(선택)
                if (configuration_.enable_bbox_overlay) {
                    for (const auto& object : objects) {
                        overlay_bounding_box_rgb24_(
                            static_cast<std::uint8_t*>(view.data_pointer),
                            view.width,
                            view.height,
                            view.stride_bytes,
                            object.bounding_box);
                    }
                }

                std::ostringstream file_name_stream;
                file_name_stream << "frame_" << frame_metadata_pointer->frame_identifier << ".ppm";

                const std::string ppm_path =
                    (std::filesystem::path(configuration_.output_directory_path) / file_name_stream.str()).string();

                // 파일 쓰기 실패는 드롭 처리(즉, 파이프라인 중단 금지)
                write_ppm_rgb24(
                    ppm_path,
                    static_cast<const std::uint8_t*>(view.data_pointer),
                    view.width,
                    view.height);
            }
        }

        // 4) 출력 타임스탬프 기록
        frame_metadata_pointer->output_completed_timestamp = output_timestamp;
        channel_state_.output_frame_count.fetch_add(1);

        node_runtime_state_store_.write(node_instance_identifier_, [](NodeRuntimeState& state) {
            state.output_count.fetch_add(1);
        });

        // 5) 해제 체인(0 단계에서 가장 중요)
        // 5-1) 객체 해제
        for (const ObjectHandle object_handle : frame_metadata_pointer->object_handles) {
            object_metadata_store_.release(object_handle);
        }
        frame_metadata_pointer->object_handles.clear();

        // 5-2) 프레임 버퍼 해제
        frame_buffer_store_.release(frame_metadata_pointer->frame_buffer_handle);
        frame_metadata_pointer->frame_buffer_handle = 0;

        // 5-3) 프레임 메타데이터 해제
        frame_metadata_store_.release(frame_metadata_pointer);
    }
}

void OutputNode::overlay_bounding_box_rgb24_(
    std::uint8_t* rgb_data_pointer,
    std::int32_t width,
    std::int32_t height,
    std::int32_t stride_bytes,
    const BoundingBox& bounding_box) {

    // 매우 단순한 오버레이:
    // - 사각형 테두리만 그린다.
    // - 색은 고정(예: 빨강). 0 단계에서는 가독성 우선.

    if (rgb_data_pointer == nullptr) {
        return;
    }

    const int left = std::max(0, static_cast<int>(bounding_box.left));
    const int top = std::max(0, static_cast<int>(bounding_box.top));
    const int right = std::min(width - 1, static_cast<int>(bounding_box.left + bounding_box.width));
    const int bottom = std::min(height - 1, static_cast<int>(bounding_box.top + bounding_box.height));

    auto set_pixel = [&](int x, int y) {
        std::uint8_t* row = rgb_data_pointer + y * stride_bytes;
        std::uint8_t* pixel = row + x * 3;
        pixel[0] = 255;  // 빨강
        pixel[1] = 0;    // 초록
        pixel[2] = 0;    // 파랑
    };

    // 상단과 하단 선
    for (int x = left; x <= right; ++x) {
        set_pixel(x, top);
        set_pixel(x, bottom);
    }

    // 좌측과 우측 선
    for (int y = top; y <= bottom; ++y) {
        set_pixel(left, y);
        set_pixel(right, y);
    }
}

void OutputNode::write_jsonl_record_(
    std::ofstream& jsonl_stream,
    const FrameMetadata& frame_metadata,
    const std::vector<ObjectMetadata>& objects) {

    // 0 단계 줄 단위 제이슨 스키마(간단):
    // - 채널 식별자
    // - 프레임 식별자
    // - 타임스탬프(디코드, 탐지, 출력의 벽시계와 단조 시각)
    // - 객체 목록: [{클래스 식별자, 신뢰도, 박스{좌,상,폭,높}}...]

    // 제이슨 문자열을 수동으로 조립한다(외부 라이브러리 회피).
    // 운영용으로는 이스케이프 처리와 정밀도 등을 정교하게 해야 하지만 0 단계에서는 단순화한다.

    jsonl_stream << "{";
    jsonl_stream << "\"channel_identifier\":" << frame_metadata.channel_identifier << ",";
    jsonl_stream << "\"frame_identifier\":" << frame_metadata.frame_identifier << ",";

    auto write_timestamp_pair = [&](const char* key, const TimestampPair& ts) {
        jsonl_stream << "\"" << key << "\":{"
                     << "\"wall_clock_time_nanoseconds\":" << ts.wall_clock_time_nanoseconds << ","
                     << "\"monotonic_time_nanoseconds\":" << ts.monotonic_time_nanoseconds
                     << "}";
    };

    write_timestamp_pair("decode_completed_timestamp", frame_metadata.decode_completed_timestamp);
    jsonl_stream << ",";
    write_timestamp_pair("detection_completed_timestamp", frame_metadata.detection_completed_timestamp);
    jsonl_stream << ",";
    write_timestamp_pair("output_completed_timestamp", frame_metadata.output_completed_timestamp);
    jsonl_stream << ",";

    jsonl_stream << "\"objects\":[";
    for (std::size_t i = 0; i < objects.size(); ++i) {
        const auto& obj = objects[i];
        jsonl_stream << "{";
        jsonl_stream << "\"class_identifier\":" << obj.class_identifier << ",";
        jsonl_stream << "\"confidence_score\":" << obj.confidence_score << ",";
        jsonl_stream << "\"bbox\":{"
                     << "\"left\":" << obj.bounding_box.left << ","
                     << "\"top\":" << obj.bounding_box.top << ","
                     << "\"width\":" << obj.bounding_box.width << ","
                     << "\"height\":" << obj.bounding_box.height
                     << "}";
        jsonl_stream << "}";
        if (i + 1 < objects.size()) {
            jsonl_stream << ",";
        }
    }
    jsonl_stream << "]";

    jsonl_stream << "}\n";
}

}
