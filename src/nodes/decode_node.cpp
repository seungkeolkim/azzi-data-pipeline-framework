#include "stream_pipeline/nodes/decode_node.hpp"

#include "stream_pipeline/utilities/time_utilities.hpp"

#include <chrono>
#include <thread>

namespace stream_pipeline {

DecodeNode::DecodeNode(
    ChannelState& channel_state,
    FrameMetadataStoreInterface& frame_metadata_store,
    FrameBufferStoreInterface& frame_buffer_store,
    BoundedPointerQueue<FrameMetadata*>& frame_metadata_queue,
    QueueOverflowPolicy overflow_policy,
    const Configuration& configuration)
    : channel_state_(channel_state),
      frame_metadata_store_(frame_metadata_store),
      frame_buffer_store_(frame_buffer_store),
      frame_metadata_queue_(frame_metadata_queue),
      overflow_policy_(overflow_policy),
      configuration_(configuration) {}

DecodeNode::~DecodeNode() {
    stop();
}

const char* DecodeNode::node_name() const {
    return "DecodeNode";
}

NodeInterface::StartResult DecodeNode::start() {
    if (running_.load()) {
        return StartResult::AlreadyRunning;
    }

    stop_requested_.store(false);
    running_.store(true);

    worker_thread_ = std::thread(&DecodeNode::thread_entry_, this);
    return StartResult::Success;
}

void DecodeNode::stop() {
    if (!running_.load()) {
        return;
    }

    stop_requested_.store(true);

    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }

    running_.store(false);
}

void DecodeNode::thread_entry_() {
    // Stage 0: synthetic frame pacing
    const int target_fps = (configuration_.synthetic_frames_per_second <= 0) ? 15 : configuration_.synthetic_frames_per_second;
    const auto frame_interval = std::chrono::milliseconds(1000 / target_fps);

    std::uint64_t local_frame_counter = 0;

    while (!stop_requested_.load()) {
        ++local_frame_counter;

        // throttling (N프레임 중 1프레임만 처리)
        const int n = (configuration_.process_every_n_frames <= 0) ? 1 : configuration_.process_every_n_frames;
        if ((local_frame_counter % static_cast<std::uint64_t>(n)) != 0ULL) {
            std::this_thread::sleep_for(frame_interval);
            continue;
        }

        // 1) FrameMetadata acquire
        auto metadata_outcome = frame_metadata_store_.acquire(channel_state_.channel_identifier);
        if (metadata_outcome.result != FrameMetadataStoreInterface::AcquireResult::Success ||
            metadata_outcome.frame_metadata_pointer == nullptr) {

            // Stage 0 정책: 실패는 프레임 drop으로 격리, 파이프라인은 계속.
            std::this_thread::sleep_for(frame_interval);
            continue;
        }

        FrameMetadata* frame_metadata_pointer = metadata_outcome.frame_metadata_pointer;

        // channel_identifier는 store acquire에서 이미 설정했지만, 명시적으로 다시 적어도 된다.
        frame_metadata_pointer->channel_identifier = channel_state_.channel_identifier;

        // frame_identifier: Stage 0에서는 decoded_frame_count를 활용해 증가값을 만든다.
        // 더 엄격한 정책(별도 카운터)은 Stage 1에서 결정해도 된다.
        const std::uint64_t frame_identifier = channel_state_.decoded_frame_count.fetch_add(1) + 1;
        frame_metadata_pointer->frame_identifier = frame_identifier;

        // 2) FrameBuffer acquire
        FrameBufferDescription description{};
        description.width = configuration_.frame_width;
        description.height = configuration_.frame_height;
        description.pixel_format = configuration_.pixel_format;
        description.memory_location = MemoryLocation::HostMemory;

        auto buffer_outcome = frame_buffer_store_.acquire(description);
        if (buffer_outcome.result != FrameBufferStoreInterface::AcquireResult::Success ||
            buffer_outcome.frame_buffer_handle == 0) {

            // 버퍼 확보 실패: 메타를 반환하고 프레임 drop
            frame_metadata_store_.release(frame_metadata_pointer);
            std::this_thread::sleep_for(frame_interval);
            continue;
        }

        frame_metadata_pointer->frame_buffer_handle = buffer_outcome.frame_buffer_handle;

        // 3) view + fill
        FrameBufferView buffer_view{};
        if (!frame_buffer_store_.view(frame_metadata_pointer->frame_buffer_handle, buffer_view) ||
            buffer_view.data_pointer == nullptr) {

            // view 실패: 버퍼와 메타를 반환하고 drop
            frame_buffer_store_.release(frame_metadata_pointer->frame_buffer_handle);
            frame_metadata_store_.release(frame_metadata_pointer);
            std::this_thread::sleep_for(frame_interval);
            continue;
        }

        // Stage 0: synthetic RGB24 fill
        fill_synthetic_rgb24_frame_(
            static_cast<std::uint8_t*>(buffer_view.data_pointer),
            buffer_view.width,
            buffer_view.height,
            buffer_view.stride_bytes,
            frame_identifier);

        // 4) 필수 필드 정리/세팅
        frame_metadata_pointer->stream_time_base = StreamTimeBase::Realtime;
        frame_metadata_pointer->presentation_timestamp = -1;  // Stage 0: RTSP PTS 미사용
        frame_metadata_pointer->object_handles.clear();
        frame_metadata_pointer->dropped = false;
        frame_metadata_pointer->drop_reason = nullptr;

        // TODO: “프레임 수신 시점”과 “decode 완료 시점” 분리 기록이 이상적이다.
        // Stage 0에서는 단순화를 위해 decode 완료 시점(=fill 완료 직후)에만 찍는다.
        frame_metadata_pointer->decode_completed_timestamp = now_timestamp_pair();

        // 5) queue push
        auto push_outcome = frame_metadata_queue_.push(frame_metadata_pointer);

        // Queue closed: 파이프라인 종료 중. 방금 만든 프레임은 정리하고 drop.
        if (push_outcome.result == BoundedPointerQueue<FrameMetadata*>::PushResult::QueueClosed) {
            frame_buffer_store_.release(frame_metadata_pointer->frame_buffer_handle);
            frame_metadata_store_.release(frame_metadata_pointer);
            break;
        }

        // DropOldestItem 정책에서 반환된 dropped pointer 정리
        if (push_outcome.dropped_old_pointer != nullptr) {
            FrameMetadata* dropped_frame_metadata_pointer = push_outcome.dropped_old_pointer;

            // drop 표시는 “통계/디버그 목적”으로 남길 수 있다.
            dropped_frame_metadata_pointer->dropped = true;
            dropped_frame_metadata_pointer->drop_reason = "frame_queue_full_drop_oldest";

            // Stage 0 단순화 계약:
            // - dropped 프레임은 아직 DummyDetectionNode가 object를 attach하기 전일 수 있으므로,
            //   object_handles는 비어있다고 가정한다.
            // - Stage 1 이후에는 이 계약을 재검토할 수 있다.
            frame_buffer_store_.release(dropped_frame_metadata_pointer->frame_buffer_handle);
            frame_metadata_store_.release(dropped_frame_metadata_pointer);

            channel_state_.dropped_frame_count.fetch_add(1);
        }

        std::this_thread::sleep_for(frame_interval);
    }
}

void DecodeNode::fill_synthetic_rgb24_frame_(
    std::uint8_t* rgb_data_pointer,
    std::int32_t width,
    std::int32_t height,
    std::int32_t stride_bytes,
    std::uint64_t frame_identifier) {

    // 목적:
    // - Stage 0에서 “실제로 프레임이 흘러간다”를 눈으로 확인할 수 있어야 한다.
    // - 외부 decode 없이도 output overlay/파일 저장이 검증 가능해야 한다.

    if (rgb_data_pointer == nullptr || width <= 0 || height <= 0 || stride_bytes <= 0) {
        return;
    }

    // 간단한 패턴:
    // - frame_identifier에 따라 색이 변화하도록 만든다.
    const std::uint8_t base = static_cast<std::uint8_t>(frame_identifier % 255);

    for (int y = 0; y < height; ++y) {
        std::uint8_t* row = rgb_data_pointer + y * stride_bytes;
        for (int x = 0; x < width; ++x) {
            const std::uint8_t r = static_cast<std::uint8_t>((base + x) % 255);
            const std::uint8_t g = static_cast<std::uint8_t>((base + y) % 255);
            const std::uint8_t b = static_cast<std::uint8_t>((base + x + y) % 255);

            row[x * 3 + 0] = r;
            row[x * 3 + 1] = g;
            row[x * 3 + 2] = b;
        }
    }
}

}  // namespace stream_pipeline
