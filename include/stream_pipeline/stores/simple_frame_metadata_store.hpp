#pragma once

#include "stream_pipeline/metadata/frame_metadata_store_interface.hpp"

#include <atomic>
#include <cstddef>
#include <mutex>
#include <vector>

namespace stream_pipeline {

/*
 * SimpleFrameMetadataStore
 * ------------------------
 * - Stage 0용 매우 단순한 FrameMetadata store 구현.
 *
 * 설계 목표:
 * - “store가 메모리를 소유한다”는 계약을 구현으로 보여준다.
 * - pool 재사용을 하되, 구현 복잡도는 최소화한다.
 *
 * 주의:
 * - Stage 0에서는 성능보다 correctness/명확성이 우선이다.
 * - Stage 1/2에서 lock-free, slab, diagnostics로 교체할 수 있다.
 * - 외부 인터페이스(FrameMetadataStoreInterface)는 유지하는 것이 목표.
 */
class SimpleFrameMetadataStore final : public FrameMetadataStoreInterface {
public:
    explicit SimpleFrameMetadataStore(std::size_t capacity);
    ~SimpleFrameMetadataStore() override = default;

    AcquireOutcome acquire(ChannelIdentifier channel_identifier) override;
    void release(FrameMetadata* frame_metadata_pointer) override;
    void close() override;

private:
    std::mutex mutex_;
    std::vector<FrameMetadata> storage_;
    std::vector<FrameMetadata*> free_list_;

    std::atomic<bool> closed_{false};
};

}  // namespace stream_pipeline
