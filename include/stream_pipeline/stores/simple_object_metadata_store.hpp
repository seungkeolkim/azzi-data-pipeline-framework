#pragma once

#include "stream_pipeline/metadata/object_metadata_store_interface.hpp"

#include <atomic>
#include <mutex>
#include <unordered_map>

namespace stream_pipeline {

/*
 * SimpleObjectMetadataStore
 * -------------------------
 * - Stage 0용 단순 ObjectMetadata store.
 *
 * Stage 0 정책:
 * - 매 프레임 1개 object 생성 → OutputNode에서 즉시 release.
 *
 * 주의:
 * - Stage 1/2에서 tracker가 들어오면 object lifecycle 정책이 복잡해질 수 있다.
 * - 그러나 외부 인터페이스(create/read/release)는 유지하는 것이 목표.
 */
class SimpleObjectMetadataStore final : public ObjectMetadataStoreInterface {
public:
    SimpleObjectMetadataStore();
    ~SimpleObjectMetadataStore() override = default;

    CreateOutcome create(const ObjectMetadata& object_metadata) override;
    bool read(ObjectHandle object_handle, ObjectMetadata& out_object_metadata) override;
    void release(ObjectHandle object_handle) override;
    void close() override;

private:
    std::mutex mutex_;
    std::unordered_map<ObjectHandle, ObjectMetadata> objects_;

    std::atomic<bool> closed_{false};
    ObjectHandle next_handle_{1};
};

}  // namespace stream_pipeline
