#include "stream_pipeline/stores/simple_object_metadata_store.hpp"

namespace stream_pipeline {

SimpleObjectMetadataStore::SimpleObjectMetadataStore() = default;

ObjectMetadataStoreInterface::CreateOutcome
SimpleObjectMetadataStore::create(const ObjectMetadata& object_metadata) {
    CreateOutcome outcome{};
    if (closed_.load()) {
        outcome.result = CreateResult::StoreClosed;
        outcome.object_handle = 0;
        return outcome;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    const ObjectHandle object_handle = next_handle_++;
    objects_[object_handle] = object_metadata;

    outcome.result = CreateResult::Success;
    outcome.object_handle = object_handle;
    return outcome;
}

bool SimpleObjectMetadataStore::read(ObjectHandle object_handle, ObjectMetadata& out_object_metadata) {
    if (object_handle == 0) {
        return false;
    }
    if (closed_.load()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = objects_.find(object_handle);
    if (it == objects_.end()) {
        return false;
    }

    out_object_metadata = it->second;
    return true;
}

void SimpleObjectMetadataStore::release(ObjectHandle object_handle) {
    if (object_handle == 0) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    objects_.erase(object_handle);
}

void SimpleObjectMetadataStore::close() {
    closed_.store(true);
}

}  // namespace stream_pipeline
