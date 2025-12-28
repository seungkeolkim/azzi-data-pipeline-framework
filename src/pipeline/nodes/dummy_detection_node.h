#pragma once

#include <chrono>
#include <string>

#include "metadata/channel_runtime_meta.h"
#include "metadata/frame.h"
#include "store/channel_runtime_meta_store.h"
#include "store/node_runtime_state_store.h"
#include "utils/bounded_pointer_queue.h"
#include "utils/logger.h"

// 더미 감지 노드: 프레임 id 패턴으로 차량/사람 존재를 판단해 메타 store에 기록한다.
class DummyDetectionNode {
public:
    DummyDetectionNode(const std::string& instance_id,
                       FrameStore& frame_store,
                       BoundedPointerQueue<FrameHandle>& input_queue,
                       BoundedPointerQueue<FrameHandle>& output_queue,
                       ChannelRuntimeMetaStore& meta_store,
                       NodeRuntimeStateStore& node_store,
                       int64_t presence_ttl_ns)
        : instance_id_(instance_id),
          frame_store_(frame_store),
          input_queue_(input_queue),
          output_queue_(output_queue),
          meta_store_(meta_store),
          counters_(node_store.Get(instance_id)),
          presence_ttl_ns_(presence_ttl_ns) {}

    void RunOnce() {
        auto handle_opt = input_queue_.Pop();
        if (!handle_opt.has_value()) {
            return;
        }
        FrameHandle handle = *handle_opt;
        counters_.in_count.fetch_add(1);

        const FrameMetadata& meta = frame_store_.GetMetadata(handle);
        const bool vehicle_detected = (meta.frame_id % 2 == 0);
        const bool person_detected = (meta.frame_id % 5 == 0);

        meta_store_.Write(meta.channel, [&](ChannelRuntimeMeta& channel_meta) {
            if (vehicle_detected) {
                channel_meta.last_seen_vehicle_monotonic_ns = meta.timestamps.monotonic_clock_ns;
            }
            if (person_detected) {
                channel_meta.last_seen_person_monotonic_ns = meta.timestamps.monotonic_clock_ns;
            }
        });

        auto dropped = output_queue_.Push(handle);
        if (dropped.has_value()) {
            counters_.drop_count.fetch_add(1);
            LOG_WARN("감지 큐 포화로 프레임 drop: id=" + std::to_string(frame_store_.GetMetadata(*dropped).frame_id));
            frame_store_.ReleaseFrame(*dropped);
        }

        counters_.out_count.fetch_add(1);
    }

private:
    std::string instance_id_;
    FrameStore& frame_store_;
    BoundedPointerQueue<FrameHandle>& input_queue_;
    BoundedPointerQueue<FrameHandle>& output_queue_;
    ChannelRuntimeMetaStore& meta_store_;
    NodeRuntimeCounters& counters_;
    int64_t presence_ttl_ns_;
};
