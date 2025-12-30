#include "stream_pipeline/stores/input_source_data_buffer_store.hpp"

namespace stream_pipeline {

InputSourceDataBufferStore::InputSourceDataBufferStore(std::size_t capacity) {
    buffers_.resize(capacity);
}

InputSourceDataBufferHandle InputSourceDataBufferStore::allocate_new_handle_unsafe() {
    // 핸들 0은 무효 값으로 남겨 두기 위해 1부터 시작한다.
    return next_handle_++;
}

InputSourceDataBufferStoreInterface::AcquireOutcome
InputSourceDataBufferStore::acquire(std::size_t size_bytes) {
    AcquireOutcome outcome{};
    if (closed_.load()) {
        outcome.result = AcquireResult::StoreClosed;
        return outcome;
    }

    if (size_bytes == 0) {
        outcome.result = AcquireResult::InvalidArgument;
        return outcome;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    for (std::size_t i = 0; i < buffers_.size(); ++i) {
        BufferRecord& record = buffers_[i];
        if (record.in_use) {
            continue;
        }

        record.in_use = true;
        record.size_bytes = size_bytes;
        record.memory_location = MemoryLocation::HostMemory;
        record.host_memory_bytes.resize(size_bytes);

        record.buffer_handle = allocate_new_handle_unsafe();
        handle_to_index_[record.buffer_handle] = i;

        outcome.result = AcquireResult::Success;
        outcome.buffer_handle = record.buffer_handle;
        return outcome;
    }

    outcome.result = AcquireResult::OutOfMemory;
    return outcome;
}

bool InputSourceDataBufferStore::view(
    InputSourceDataBufferHandle buffer_handle,
    InputSourceDataBufferView& out_view) {

    if (buffer_handle == 0) {
        return false;
    }
    if (closed_.load()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = handle_to_index_.find(buffer_handle);
    if (it == handle_to_index_.end()) {
        return false;
    }

    BufferRecord& record = buffers_[it->second];
    if (!record.in_use) {
        return false;
    }

    out_view = InputSourceDataBufferView{};
    out_view.memory_location = record.memory_location;
    out_view.data_pointer = record.host_memory_bytes.data();
    out_view.size_bytes = record.size_bytes;
    out_view.device_identifier = -1;

    return true;
}

void InputSourceDataBufferStore::release(InputSourceDataBufferHandle buffer_handle) {
    if (buffer_handle == 0) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = handle_to_index_.find(buffer_handle);
    if (it == handle_to_index_.end()) {
        return;
    }

    BufferRecord& record = buffers_[it->second];
    record.in_use = false;
    record.buffer_handle = 0;
    record.size_bytes = 0;
    record.host_memory_bytes.clear();

    handle_to_index_.erase(it);
}

void InputSourceDataBufferStore::close() {
    closed_.store(true);
}

}
