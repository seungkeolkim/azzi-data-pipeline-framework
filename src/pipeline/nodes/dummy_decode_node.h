#pragma once

#include <chrono>
#include <string>

#include "metadata/frame.h"
#include "store/node_runtime_state_store.h"
#include "utils/bounded_pointer_queue.h"
#include "utils/logger.h"

// Stage 0.5에서 입력 소스 분리는 보류하고, 단순한 더미 디코드 노드만 만든다.
class DummyDecodeNode {
public:
    DummyDecodeNode(const std::string& instance_id,
                    FrameStore& frame_store,
                    BoundedPointerQueue<FrameHandle>& output_queue,
                    NodeRuntimeStateStore& node_store,
                    ChannelIdentifier channel)
        : instance_id_(instance_id),
          frame_store_(frame_store),
          output_queue_(output_queue),
          counters_(node_store.Get(instance_id)),
          channel_(std::move(channel)) {}

    void Run(uint64_t frame_count) {
        LOG_INFO("DummyDecodeNode 시작");
        for (uint64_t i = 0; i < frame_count; ++i) {
            ProduceFrame(i);
        }
        LOG_INFO("DummyDecodeNode 종료");
    }

private:
    void ProduceFrame(uint64_t index) {
        const auto now = std::chrono::steady_clock::now();
        const int64_t monotonic_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
        const int64_t wall_ns = monotonic_ns;  // 데모 목적: 동일 값 사용.

        FrameMetadata metadata{index, channel_, TimestampPair{wall_ns, monotonic_ns}};
        FrameBuffer buffer{"dummy_frame_" + std::to_string(index)};
        std::vector<ObjectMetadata> objects;  // 디코드 단계에서는 객체 없음.

        FrameHandle handle = frame_store_.CreateFrame(metadata, buffer, objects);
        counters_.in_count.fetch_add(1);

        auto dropped = output_queue_.Push(handle);
        if (dropped.has_value()) {
            counters_.drop_count.fetch_add(1);
            LOG_WARN("디코드 큐 포화로 프레임 drop: id=" + std::to_string(frame_store_.GetMetadata(*dropped).frame_id));
            // DropOldest 반환된 핸들은 호출자에서 release chain을 마무리한다.
            frame_store_.ReleaseFrame(*dropped);
        }

        counters_.out_count.fetch_add(1);
    }

    std::string instance_id_;
    FrameStore& frame_store_;
    BoundedPointerQueue<FrameHandle>& output_queue_;
    NodeRuntimeCounters& counters_;
    ChannelIdentifier channel_;
};
