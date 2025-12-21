#include "stream_pipeline/stores/simple_frame_metadata_store.hpp"

namespace stream_pipeline {

SimpleFrameMetadataStore::SimpleFrameMetadataStore(std::size_t capacity) {
    storage_.resize(capacity);
    free_list_.reserve(capacity);

    // free_list_에 미리 포인터를 채워 넣는다.
    for (std::size_t i = 0; i < capacity; ++i) {
        free_list_.push_back(&storage_[i]);
    }
}

FrameMetadataStoreInterface::AcquireOutcome
SimpleFrameMetadataStore::acquire(ChannelIdentifier channel_identifier) {
    AcquireOutcome outcome{};
    if (closed_.load()) {
        outcome.result = AcquireResult::StoreClosed;
        outcome.frame_metadata_pointer = nullptr;
        return outcome;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (free_list_.empty()) {
        outcome.result = AcquireResult::OutOfMemory;
        outcome.frame_metadata_pointer = nullptr;
        return outcome;
    }

    FrameMetadata* pointer = free_list_.back();
    free_list_.pop_back();

    // pool 재사용이므로, 이전 사용 흔적을 “확실히” 초기화한다.
    // - 이 초기화는 Stage 0에서 비용보다 안전성이 중요하다.
    *pointer = FrameMetadata{};
    pointer->channel_identifier = channel_identifier;

    outcome.result = AcquireResult::Success;
    outcome.frame_metadata_pointer = pointer;
    return outcome;
}

void SimpleFrameMetadataStore::release(FrameMetadata* frame_metadata_pointer) {
    if (frame_metadata_pointer == nullptr) {
        return;
    }

    // store가 closed 상태여도, release는 안전해야 한다.
    std::lock_guard<std::mutex> lock(mutex_);

    // release 시점에도 “다음 사용자”를 위해 초기화해 둔다.
    // - 실제 운영에서는 비용 문제로 최적화할 수 있으나 Stage 0에서는 안전이 우선.
    *frame_metadata_pointer = FrameMetadata{};
    free_list_.push_back(frame_metadata_pointer);
}

void SimpleFrameMetadataStore::close() {
    closed_.store(true);
}

}  // namespace stream_pipeline
