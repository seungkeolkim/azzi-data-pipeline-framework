#include "stream_pipeline/nodes/decode_node.hpp"

#include "stream_pipeline/utilities/time_utilities.hpp"
#include "stream_pipeline/utilities/logger.hpp"

#include <chrono>
#include <exception>
#include <thread>

namespace stream_pipeline {

DecodeNode::DecodeNode(
    ChannelState& channel_state,
    FrameMetadataStoreInterface& frame_metadata_store,
    FrameBufferStoreInterface& frame_buffer_store,
    InputSourceDataBufferStoreInterface& input_source_data_buffer_store,
    NodeRuntimeStateStoreInterface& node_runtime_state_store,
    BoundedPointerQueue<InputSourceQueueItem*>& input_queue,
    BoundedPointerQueue<FrameMetadata*>& frame_metadata_queue,
    QueueOverflowPolicy overflow_policy,
    const Configuration& configuration)
    : channel_state_(channel_state),
      frame_metadata_store_(frame_metadata_store),
      frame_buffer_store_(frame_buffer_store),
      input_source_data_buffer_store_(input_source_data_buffer_store),
      node_runtime_state_store_(node_runtime_state_store),
      input_queue_(input_queue),
      frame_metadata_queue_(frame_metadata_queue),
      overflow_policy_(overflow_policy),
      configuration_(configuration) {
    node_instance_identifier_ = 2;
}

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

    Logger::instance().log_with_node(Logger::Level::Info, node_name(), "start requested");

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

    Logger::instance().log_with_node(Logger::Level::Info, node_name(), "stopped");
}

void DecodeNode::thread_entry_() {
    std::uint64_t local_frame_counter = 0;

    while (!stop_requested_.load()) {
        InputSourceQueueItem* item_pointer = nullptr;
        try {
            auto pop_outcome = input_queue_.pop_blocking();
            if (pop_outcome.result == BoundedPointerQueue<InputSourceQueueItem*>::PopResult::QueueClosedAndEmpty) {
                break;
            }

            item_pointer = pop_outcome.dequeued_pointer;
            if (item_pointer == nullptr) {
                continue;
            }

            node_runtime_state_store_.write(node_instance_identifier_, [](NodeRuntimeState& state) {
                state.input_count.fetch_add(1);
            });

            ++local_frame_counter;
            const int process_every_n = (configuration_.process_every_n_frames <= 0) ? 1 : configuration_.process_every_n_frames;
            if ((local_frame_counter % static_cast<std::uint64_t>(process_every_n)) != 0ULL) {
                release_input_item_(item_pointer);
                item_pointer = nullptr;
                node_runtime_state_store_.write(node_instance_identifier_, [](NodeRuntimeState& state) {
                    state.dropped_count.fetch_add(1);
                });
                continue;
            }

            auto metadata_outcome = frame_metadata_store_.acquire(channel_state_.channel_identifier);
            if (metadata_outcome.result != FrameMetadataStoreInterface::AcquireResult::Success ||
                metadata_outcome.frame_metadata_pointer == nullptr) {

                release_input_item_(item_pointer);
                item_pointer = nullptr;
                node_runtime_state_store_.write(node_instance_identifier_, [](NodeRuntimeState& state) {
                    state.error_count.fetch_add(1);
                });
                continue;
            }

            FrameMetadata* frame_metadata_pointer = metadata_outcome.frame_metadata_pointer;
            frame_metadata_pointer->channel_identifier = channel_state_.channel_identifier;

            const std::uint64_t frame_identifier = channel_state_.decoded_frame_count.fetch_add(1) + 1;
            frame_metadata_pointer->frame_identifier = frame_identifier;

            FrameBufferDescription description{};
            description.width = configuration_.frame_width;
            description.height = configuration_.frame_height;
            description.pixel_format = configuration_.pixel_format;
            description.memory_location = MemoryLocation::HostMemory;

            auto buffer_outcome = frame_buffer_store_.acquire(description);
            if (buffer_outcome.result != FrameBufferStoreInterface::AcquireResult::Success ||
                buffer_outcome.frame_buffer_handle == 0) {

                frame_metadata_store_.release(frame_metadata_pointer);
                release_input_item_(item_pointer);
                item_pointer = nullptr;
                node_runtime_state_store_.write(node_instance_identifier_, [](NodeRuntimeState& state) {
                    state.error_count.fetch_add(1);
                });
                continue;
            }

            frame_metadata_pointer->frame_buffer_handle = buffer_outcome.frame_buffer_handle;

            FrameBufferView buffer_view{};
            if (!frame_buffer_store_.view(frame_metadata_pointer->frame_buffer_handle, buffer_view) ||
                buffer_view.data_pointer == nullptr) {

                frame_buffer_store_.release(frame_metadata_pointer->frame_buffer_handle);
                frame_metadata_store_.release(frame_metadata_pointer);
                release_input_item_(item_pointer);
                item_pointer = nullptr;
                node_runtime_state_store_.write(node_instance_identifier_, [](NodeRuntimeState& state) {
                    state.error_count.fetch_add(1);
                });
                continue;
            }

            fill_synthetic_rgb24_frame_(
                static_cast<std::uint8_t*>(buffer_view.data_pointer),
                buffer_view.width,
                buffer_view.height,
                buffer_view.stride_bytes,
                frame_identifier);

            frame_metadata_pointer->stream_time_base = StreamTimeBase::Realtime;
            if (item_pointer->presentation_timestamp.has_value()) {
                frame_metadata_pointer->presentation_timestamp = item_pointer->presentation_timestamp.value();
            } else {
                frame_metadata_pointer->presentation_timestamp = -1;
            }
            frame_metadata_pointer->object_handles.clear();
            frame_metadata_pointer->dropped = false;
            frame_metadata_pointer->drop_reason = nullptr;
            frame_metadata_pointer->decode_completed_timestamp = now_timestamp_pair();

            release_input_item_(item_pointer);
            item_pointer = nullptr;

            auto push_outcome = frame_metadata_queue_.push(frame_metadata_pointer);

            if (push_outcome.result == BoundedPointerQueue<FrameMetadata*>::PushResult::QueueClosed) {
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
                dropped_frame_metadata_pointer->drop_reason = "frame_queue_full_drop_oldest";

                frame_buffer_store_.release(dropped_frame_metadata_pointer->frame_buffer_handle);
                frame_metadata_store_.release(dropped_frame_metadata_pointer);

                channel_state_.dropped_frame_count.fetch_add(1);

                node_runtime_state_store_.write(node_instance_identifier_, [](NodeRuntimeState& state) {
                    state.dropped_count.fetch_add(1);
                });

                Logger::instance().log_with_node(
                    Logger::Level::Warning,
                    node_name(),
                    "dropped oldest frame due to full decode queue");
            }

            node_runtime_state_store_.write(node_instance_identifier_, [](NodeRuntimeState& state) {
                state.output_count.fetch_add(1);
            });
        } catch (const std::exception&) {
            if (item_pointer != nullptr) {
                release_input_item_(item_pointer);
            }
            node_runtime_state_store_.write(node_instance_identifier_, [](NodeRuntimeState& state) {
                state.error_count.fetch_add(1);
            });
            Logger::instance().log_with_node(Logger::Level::Error, node_name(), "decode thread exception");
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        } catch (...) {
            if (item_pointer != nullptr) {
                release_input_item_(item_pointer);
            }
            node_runtime_state_store_.write(node_instance_identifier_, [](NodeRuntimeState& state) {
                state.error_count.fetch_add(1);
            });
            Logger::instance().log_with_node(Logger::Level::Error, node_name(), "decode thread unknown exception");
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

void DecodeNode::release_input_item_(InputSourceQueueItem* item_pointer) {
    if (item_pointer == nullptr) {
        return;
    }

    if (item_pointer->buffer_handle != 0) {
        input_source_data_buffer_store_.release(item_pointer->buffer_handle);
    }
    delete item_pointer;
}

void DecodeNode::fill_synthetic_rgb24_frame_(
    std::uint8_t* rgb_data_pointer,
    std::int32_t width,
    std::int32_t height,
    std::int32_t stride_bytes,
    std::uint64_t frame_identifier) {

    // 목적:
    // - 0.6 단계에서 프레임이 흐르는 것을 눈으로 확인할 수 있어야 한다.
    // - 외부 디코더 없이도 출력 저장이 검증 가능해야 한다.

    if (rgb_data_pointer == nullptr || width <= 0 || height <= 0 || stride_bytes <= 0) {
        return;
    }

    // 간단한 패턴:
    // - 프레임 식별자에 따라 색이 변화하도록 만든다.
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

}
