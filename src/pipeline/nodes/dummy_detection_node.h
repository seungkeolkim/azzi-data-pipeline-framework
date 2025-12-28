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

// DummyDetectionNode consumes frames, attaches synthetic objects, and writes channel runtime meta.
class DummyDetectionNode {
 public:
  DummyDetectionNode(uint64_t channel_id, FrameStore& frame_store,
                     ChannelRuntimeMetaStore& channel_meta_store,
                     NodeRuntimeStateStore& node_state_store,
                     BoundedPointerQueue<FrameStore::FrameHandle>& input_queue,
                     BoundedPointerQueue<FrameStore::BufferHandle>& output_queue,
                     std::string instance_id)
      : channel_id_(channel_id),
        frame_store_(frame_store),
        channel_meta_store_(channel_meta_store),
        node_state_(node_state_store.GetOrCreate(instance_id)),
        input_queue_(input_queue),
        output_queue_(output_queue),
        instance_id_(std::move(instance_id)) {}

  void Start() { Logger::Instance().Log(Logger::Level::kInfo, instance_id_, "Start detection node"); }
  void Stop() { Logger::Instance().Log(Logger::Level::kInfo, instance_id_, "Stop detection node"); }

  bool ProcessNext() {
    if (input_queue_.Empty()) {
      return false;
    }
    node_state_.in_count.fetch_add(1);
    auto frame_handle = input_queue_.Pop();
    FrameMetadata* metadata = frame_store_.GetFrame(frame_handle);
    if (metadata == nullptr) {
      node_state_.error_count.fetch_add(1);
      Logger::Instance().Log(Logger::Level::kError, instance_id_, "Missing frame metadata");
      return false;
    }

    auto buffer_handle = frame_store_.GetBufferForFrame(frame_handle);
    if (!buffer_handle.has_value()) {
      node_state_.error_count.fetch_add(1);
      Logger::Instance().Log(Logger::Level::kError, instance_id_, "Missing buffer for frame");
      return false;
    }

    // Synthetic detection: even frame ids carry vehicles, odd ones do not.
    std::string label = (metadata->frame_id % 2 == 0) ? "vehicle" : "background";
    frame_store_.AddObject(buffer_handle.value(),
                           std::make_unique<ObjectMetadata>(ObjectMetadata{
                               metadata->frame_id, metadata->frame_id, label}));

    // Update runtime meta with last seen vehicle timestamp.
    if (label == "vehicle") {
      channel_meta_store_.Write(channel_id_, [this, metadata](ChannelRuntimeMeta& meta) {
        meta.last_seen_vehicle_monotonic_ns = metadata->monotonic_clock_ns;
      });
    }

    const auto dropped = output_queue_.Push(buffer_handle.value());
    node_state_.out_count.fetch_add(1);
    if (dropped.has_value()) {
      node_state_.drop_count.fetch_add(1);
      Logger::Instance().Log(Logger::Level::kWarn, instance_id_,
                             "DropOldest at detection output, buffer_handle=" +
                                 std::to_string(dropped.value()));
      frame_store_.ReleaseBuffer(dropped.value());
    }
    return true;
  }

 private:
  uint64_t channel_id_;
  FrameStore& frame_store_;
  ChannelRuntimeMetaStore& channel_meta_store_;
  NodeRuntimeState& node_state_;
  BoundedPointerQueue<FrameStore::FrameHandle>& input_queue_;
  BoundedPointerQueue<FrameStore::BufferHandle>& output_queue_;
  std::string instance_id_;
};
