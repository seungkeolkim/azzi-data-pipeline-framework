#pragma once

#include <chrono>
#include <string>

#include "metadata/channel_runtime_meta.h"
#include "metadata/frame.h"
#include "store/channel_runtime_meta_store.h"
#include "store/node_runtime_state_store.h"
#include "utils/bounded_pointer_queue.h"
#include "utils/logger.h"

// Output 노드는 release chain을 마무리하고 TTL 기반 present 여부를 샘플링 로그로 남긴다.
class OutputNode {
public:
    OutputNode(const std::string& instance_id,
               FrameStore& frame_store,
               BoundedPointerQueue<FrameHandle>& input_queue,
               ChannelRuntimeMetaStore& meta_store,
               NodeRuntimeStateStore& node_store,
               int64_t presence_ttl_ns)
        : instance_id_(instance_id),
          frame_store_(frame_store),
          input_queue_(input_queue),
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
        const int64_t now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                   std::chrono::steady_clock::now().time_since_epoch())
                                   .count();

        bool vehicle_present = false;
        bool person_present = false;
        meta_store_.Read(meta.channel, [&](const ChannelRuntimeMeta& channel_meta) {
            vehicle_present = channel_meta.IsVehiclePresent(now_ns, presence_ttl_ns_);
            person_present = channel_meta.IsPersonPresent(now_ns, presence_ttl_ns_);
        });

        // 샘플링 로그: frame_id 3의 배수마다 출력.
        if (meta.frame_id % 3 == 0) {
            LOG_INFO("채널=" + meta.channel.name + " frame=" + std::to_string(meta.frame_id) +
                     " vehicle_present=" + (vehicle_present ? "true" : "false") +
                     " person_present=" + (person_present ? "true" : "false"));
        }

        // release chain 유지.
        frame_store_.ReleaseObjects(handle);
        frame_store_.ReleaseBuffer(handle);
        frame_store_.ReleaseFrame(handle);

        counters_.out_count.fetch_add(1);
    }

private:
    std::string instance_id_;
    FrameStore& frame_store_;
    BoundedPointerQueue<FrameHandle>& input_queue_;
    ChannelRuntimeMetaStore& meta_store_;
    NodeRuntimeCounters& counters_;
    int64_t presence_ttl_ns_;
};
