#pragma once

#include <chrono>
#include <optional>
#include <string>

#include "metadata/frame.h"
#include "metadata/channel_runtime_meta.h"
#include "store/channel_runtime_meta_store.h"
#include "store/node_runtime_state_store.h"
#include "utils/bounded_pointer_queue.h"
#include "utils/logger.h"

// OutputNode reads buffered detections, performs TTL-based checks, and releases resources.
class OutputNode {
 public:
  OutputNode(uint64_t channel_id, FrameStore& frame_store, ChannelState& channel_state,
             ChannelRuntimeMetaStore& channel_meta_store,
             NodeRuntimeStateStore& node_state_store,
             BoundedPointerQueue<FrameStore::BufferHandle>& input_queue, std::string instance_id)
      : channel_id_(channel_id),
        frame_store_(frame_store),
        channel_state_(channel_state),
        channel_meta_store_(channel_meta_store),
        node_state_(node_state_store.GetOrCreate(instance_id)),
        input_queue_(input_queue),
        instance_id_(std::move(instance_id)) {}

  void Start() { Logger::Instance().Log(Logger::Level::kInfo, instance_id_, "Start output node"); }
  void Stop() { Logger::Instance().Log(Logger::Level::kInfo, instance_id_, "Stop output node"); }

  bool ProcessNext(uint64_t vehicle_ttl_ns) {
    if (input_queue_.Empty()) {
      return false;
    }
    node_state_.in_count.fetch_add(1);
    auto buffer_handle = input_queue_.Pop();

    auto object_handle = frame_store_.GetObjectForBuffer(buffer_handle);
    auto frame_handle = frame_store_.GetFrameForBuffer(buffer_handle);
    if (!frame_handle.has_value()) {
      node_state_.error_count.fetch_add(1);
      Logger::Instance().Log(Logger::Level::kError, instance_id_, "Missing frame for buffer");
      return false;
    }

    const auto now_monotonic = NowMonotonic();
    bool vehicle_present = false;
    channel_meta_store_.Read(channel_id_, [vehicle_ttl_ns, now_monotonic, &vehicle_present](
                                               const ChannelRuntimeMeta& meta) {
      vehicle_present = meta.IsVehiclePresent(now_monotonic, vehicle_ttl_ns);
    });

    Logger::Instance().Log(Logger::Level::kInfo, instance_id_,
                           "Output frame=" + std::to_string(frame_handle.value()) +
                               " vehicle_present=" + (vehicle_present ? "true" : "false"));

    channel_state_.output_frames.fetch_add(1);
    node_state_.out_count.fetch_add(1);

    // Release chain: object -> buffer -> frame.
    if (object_handle.has_value()) {
      frame_store_.ReleaseObject(object_handle.value());
    }
    frame_store_.ReleaseBuffer(buffer_handle);
    frame_store_.ReleaseFrame(frame_handle.value());

    return true;
  }

 private:
  uint64_t NowMonotonic() const {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
  }

  uint64_t channel_id_;
  FrameStore& frame_store_;
  ChannelState& channel_state_;
  ChannelRuntimeMetaStore& channel_meta_store_;
  NodeRuntimeState& node_state_;
  BoundedPointerQueue<FrameStore::BufferHandle>& input_queue_;
  std::string instance_id_;
};
