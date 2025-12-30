#include "stream_pipeline/nodes/dummy_input_source_node.hpp"

#include "stream_pipeline/utilities/logger.hpp"
#include "stream_pipeline/utilities/time_utilities.hpp"

#include <chrono>
#include <cstring>
#include <exception>
#include <new>
#include <thread>

namespace stream_pipeline {

DummyInputSourceNode::DummyInputSourceNode(
    ChannelState& channel_state,
    InputSourceDataBufferStoreInterface& input_source_data_buffer_store,
    NodeRuntimeStateStoreInterface& node_runtime_state_store,
    BoundedPointerQueue<InputSourceQueueItem*>& output_queue,
    const Configuration& configuration,
    InputSourceMode input_source_mode)
    : InputSourceNode(input_source_mode),
      channel_state_(channel_state),
      input_source_data_buffer_store_(input_source_data_buffer_store),
      node_runtime_state_store_(node_runtime_state_store),
      output_queue_(output_queue),
      configuration_(configuration) {
    node_instance_identifier_ = 1;
}

const char* DummyInputSourceNode::node_name() const {
    return "DummyInputSourceNode";
}

void DummyInputSourceNode::thread_entry_() {
    if (input_source_mode() == InputSourceMode::Push) {
        set_input_source_state_(InputSourceState::Idle);
        while (!stop_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        return;
    }

    const int items_per_second = (configuration_.items_per_second <= 0) ? 15 : configuration_.items_per_second;
    const auto item_interval = std::chrono::milliseconds(1000 / items_per_second);

    while (!stop_requested()) {
        try {
            InputSourceQueueItem* item_pointer = create_queue_item_(nullptr, configuration_.size_bytes_per_item);
            if (item_pointer == nullptr) {
                set_input_source_state_(InputSourceState::Error);
                node_runtime_state_store_.write(node_instance_identifier_, [](NodeRuntimeState& state) {
                    state.error_count.fetch_add(1);
                });
                std::this_thread::sleep_for(item_interval);
                continue;
            }

            node_runtime_state_store_.write(node_instance_identifier_, [](NodeRuntimeState& state) {
                state.input_count.fetch_add(1);
            });

            auto push_outcome = output_queue_.push(item_pointer);

            if (push_outcome.result == BoundedPointerQueue<InputSourceQueueItem*>::PushResult::QueueClosed) {
                release_queue_item_(item_pointer);
                node_runtime_state_store_.write(node_instance_identifier_, [](NodeRuntimeState& state) {
                    state.dropped_count.fetch_add(1);
                });
                break;
            }

            if (push_outcome.result == BoundedPointerQueue<InputSourceQueueItem*>::PushResult::DroppedNewItem) {
                release_queue_item_(item_pointer);
                node_runtime_state_store_.write(node_instance_identifier_, [](NodeRuntimeState& state) {
                    state.dropped_count.fetch_add(1);
                });
                std::this_thread::sleep_for(item_interval);
                continue;
            }

            if (push_outcome.dropped_old_pointer != nullptr) {
                release_queue_item_(push_outcome.dropped_old_pointer);
                node_runtime_state_store_.write(node_instance_identifier_, [](NodeRuntimeState& state) {
                    state.dropped_count.fetch_add(1);
                });
                Logger::instance().log_with_node(
                    Logger::Level::Warning,
                    node_name(),
                    "dropped oldest input item due to full input queue");
            }

            node_runtime_state_store_.write(node_instance_identifier_, [](NodeRuntimeState& state) {
                state.output_count.fetch_add(1);
            });

            set_input_source_state_(InputSourceState::Running);
            std::this_thread::sleep_for(item_interval);
        } catch (const std::exception&) {
            set_input_source_state_(InputSourceState::Error);
            node_runtime_state_store_.write(node_instance_identifier_, [](NodeRuntimeState& state) {
                state.error_count.fetch_add(1);
            });
            Logger::instance().log_with_node(Logger::Level::Error, node_name(), "input thread exception");
            std::this_thread::sleep_for(item_interval);
        } catch (...) {
            set_input_source_state_(InputSourceState::Error);
            node_runtime_state_store_.write(node_instance_identifier_, [](NodeRuntimeState& state) {
                state.error_count.fetch_add(1);
            });
            Logger::instance().log_with_node(Logger::Level::Error, node_name(), "input thread unknown exception");
            std::this_thread::sleep_for(item_interval);
        }
    }
}

bool DummyInputSourceNode::handle_external_push_(const std::uint8_t* data_pointer, std::size_t size_bytes) {
    if (data_pointer == nullptr || size_bytes == 0) {
        return false;
    }

    node_runtime_state_store_.write(node_instance_identifier_, [](NodeRuntimeState& state) {
        state.input_count.fetch_add(1);
    });

    InputSourceQueueItem* item_pointer = create_queue_item_(data_pointer, size_bytes);
    if (item_pointer == nullptr) {
        set_input_source_state_(InputSourceState::Error);
        node_runtime_state_store_.write(node_instance_identifier_, [](NodeRuntimeState& state) {
            state.error_count.fetch_add(1);
        });
        return false;
    }

    auto push_outcome = output_queue_.push(item_pointer);

    if (push_outcome.result == BoundedPointerQueue<InputSourceQueueItem*>::PushResult::QueueClosed) {
        release_queue_item_(item_pointer);
        node_runtime_state_store_.write(node_instance_identifier_, [](NodeRuntimeState& state) {
            state.dropped_count.fetch_add(1);
        });
        return false;
    }

    if (push_outcome.result == BoundedPointerQueue<InputSourceQueueItem*>::PushResult::DroppedNewItem) {
        release_queue_item_(item_pointer);
        node_runtime_state_store_.write(node_instance_identifier_, [](NodeRuntimeState& state) {
            state.dropped_count.fetch_add(1);
        });
        return false;
    }

    if (push_outcome.dropped_old_pointer != nullptr) {
        release_queue_item_(push_outcome.dropped_old_pointer);
        node_runtime_state_store_.write(node_instance_identifier_, [](NodeRuntimeState& state) {
            state.dropped_count.fetch_add(1);
        });
        Logger::instance().log_with_node(
            Logger::Level::Warning,
            node_name(),
            "dropped oldest input item due to full input queue");
    }

    node_runtime_state_store_.write(node_instance_identifier_, [](NodeRuntimeState& state) {
        state.output_count.fetch_add(1);
    });

    set_input_source_state_(InputSourceState::Running);
    return true;
}

InputSourceQueueItem* DummyInputSourceNode::create_queue_item_(
    const std::uint8_t* data_pointer,
    std::size_t size_bytes) {

    if (size_bytes == 0) {
        return nullptr;
    }

    auto buffer_outcome = input_source_data_buffer_store_.acquire(size_bytes);
    if (buffer_outcome.result != InputSourceDataBufferStoreInterface::AcquireResult::Success ||
        buffer_outcome.buffer_handle == 0) {
        return nullptr;
    }

    InputSourceDataBufferView buffer_view{};
    if (!input_source_data_buffer_store_.view(buffer_outcome.buffer_handle, buffer_view) ||
        buffer_view.data_pointer == nullptr) {
        input_source_data_buffer_store_.release(buffer_outcome.buffer_handle);
        return nullptr;
    }

    const std::uint64_t sequence_number = sequence_counter_.fetch_add(1) + 1;

    if (data_pointer != nullptr) {
        std::memcpy(buffer_view.data_pointer, data_pointer, size_bytes);
    } else {
        fill_dummy_bytes_(static_cast<std::uint8_t*>(buffer_view.data_pointer), size_bytes, sequence_number);
    }

    InputSourceQueueItem* item_pointer = new (std::nothrow) InputSourceQueueItem();
    if (item_pointer == nullptr) {
        input_source_data_buffer_store_.release(buffer_outcome.buffer_handle);
        return nullptr;
    }

    item_pointer->channel_identifier = channel_state_.channel_identifier;
    item_pointer->buffer_handle = buffer_outcome.buffer_handle;
    item_pointer->size_bytes = size_bytes;
    item_pointer->sequence_number = sequence_number;
    item_pointer->received_timestamp = now_timestamp_pair();

    return item_pointer;
}

void DummyInputSourceNode::release_queue_item_(InputSourceQueueItem* item_pointer) {
    if (item_pointer == nullptr) {
        return;
    }

    if (item_pointer->buffer_handle != 0) {
        input_source_data_buffer_store_.release(item_pointer->buffer_handle);
    }
    delete item_pointer;
}

void DummyInputSourceNode::fill_dummy_bytes_(
    std::uint8_t* data_pointer,
    std::size_t size_bytes,
    std::uint64_t sequence_number) {

    if (data_pointer == nullptr || size_bytes == 0) {
        return;
    }

    const std::uint8_t base_value = static_cast<std::uint8_t>(sequence_number % 255);
    std::memset(data_pointer, base_value, size_bytes);
}

}
