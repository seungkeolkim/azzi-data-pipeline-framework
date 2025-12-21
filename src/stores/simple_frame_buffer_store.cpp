#include "stream_pipeline/stores/simple_frame_buffer_store.hpp"

namespace stream_pipeline {

SimpleFrameBufferStore::SimpleFrameBufferStore(std::size_t capacity) {
    buffers_.resize(capacity);
}

FrameBufferHandle SimpleFrameBufferStore::allocate_new_handle_unsafe() {
    // handle=0은 “invalid”로 남겨두기 위해 1부터 시작.
    return next_handle_++;
}

FrameBufferStoreInterface::AcquireOutcome
SimpleFrameBufferStore::acquire(const FrameBufferDescription& frame_buffer_description) {
    AcquireOutcome outcome{};
    if (closed_.load()) {
        outcome.result = AcquireResult::StoreClosed;
        return outcome;
    }

    // Stage 0 단순화: HostMemory + RGB24만 지원
    if (frame_buffer_description.memory_location != MemoryLocation::HostMemory) {
        outcome.result = AcquireResult::InvalidArgument;
        return outcome;
    }
    if (frame_buffer_description.pixel_format != PixelFormat::RGB24) {
        outcome.result = AcquireResult::UnsupportedFormat;
        return outcome;
    }
    if (frame_buffer_description.width <= 0 || frame_buffer_description.height <= 0) {
        outcome.result = AcquireResult::InvalidArgument;
        return outcome;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // 사용 가능한 record를 찾는다.
    for (std::size_t i = 0; i < buffers_.size(); ++i) {
        BufferRecord& record = buffers_[i];
        if (record.in_use) {
            continue;
        }

        record.in_use = true;
        record.width = frame_buffer_description.width;
        record.height = frame_buffer_description.height;
        record.pixel_format = frame_buffer_description.pixel_format;
        record.memory_location = frame_buffer_description.memory_location;

        // RGB24: 3 bytes per pixel
        record.stride_bytes = record.width * 3;

        const std::size_t needed_bytes =
            static_cast<std::size_t>(record.height) * static_cast<std::size_t>(record.stride_bytes);

        record.host_memory_bytes.resize(needed_bytes);

        // handle 발급 및 map 등록
        record.frame_buffer_handle = allocate_new_handle_unsafe();
        handle_to_index_[record.frame_buffer_handle] = i;

        outcome.result = AcquireResult::Success;
        outcome.frame_buffer_handle = record.frame_buffer_handle;
        return outcome;
    }

    outcome.result = AcquireResult::OutOfMemory;
    return outcome;
}

bool SimpleFrameBufferStore::view(FrameBufferHandle frame_buffer_handle, FrameBufferView& out_frame_buffer_view) {
    if (frame_buffer_handle == 0) {
        return false;
    }
    if (closed_.load()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = handle_to_index_.find(frame_buffer_handle);
    if (it == handle_to_index_.end()) {
        return false;
    }

    BufferRecord& record = buffers_[it->second];
    if (!record.in_use) {
        // 이미 release된 handle일 수 있다.
        return false;
    }

    out_frame_buffer_view = FrameBufferView{};
    out_frame_buffer_view.memory_location = record.memory_location;
    out_frame_buffer_view.data_pointer = record.host_memory_bytes.data();
    out_frame_buffer_view.width = record.width;
    out_frame_buffer_view.height = record.height;
    out_frame_buffer_view.stride_bytes = record.stride_bytes;
    out_frame_buffer_view.pixel_format = record.pixel_format;
    out_frame_buffer_view.device_identifier = -1;

    return true;
}

void SimpleFrameBufferStore::release(FrameBufferHandle frame_buffer_handle) {
    if (frame_buffer_handle == 0) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = handle_to_index_.find(frame_buffer_handle);
    if (it == handle_to_index_.end()) {
        return;
    }

    BufferRecord& record = buffers_[it->second];

    // “release는 idempotent하게” 설계하는 편이 운영에서 안전하다.
    // - 중복 release를 조용히 무시할지, assert/log할지는 나중에 정책으로 결정 가능.
    record.in_use = false;
    record.frame_buffer_handle = 0;
    record.host_memory_bytes.clear();

    handle_to_index_.erase(it);
}

void SimpleFrameBufferStore::close() {
    closed_.store(true);
}

}  // namespace stream_pipeline
