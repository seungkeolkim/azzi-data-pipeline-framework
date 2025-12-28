#pragma once

#include <chrono>
#include <string>

#include "metadata/frame.h"
#include "store/node_runtime_state_store.h"
#include "utils/bounded_pointer_queue.h"
#include "utils/logger.h"

// DummyDecodeNode generates synthetic frames and enqueues FrameHandles.
class DummyDecodeNode {
 public:
  DummyDecodeNode(uint64_t channel_id, FrameStore& frame_store, ChannelState& channel_state,
                  NodeRuntimeStateStore& node_state_store,
                  BoundedPointerQueue<FrameStore::FrameHandle>& output_queue,
                  std::string instance_id)
      : channel_id_(channel_id),
        frame_store_(frame_store),
        channel_state_(channel_state),
        node_state_(node_state_store.GetOrCreate(instance_id)),
        output_queue_(output_queue),
        instance_id_(std::move(instance_id)) {}

  void Start() { Logger::Instance().Log(Logger::Level::kInfo, instance_id_, "Start decode node"); }
  void Stop() { Logger::Instance().Log(Logger::Level::kInfo, instance_id_, "Stop decode node"); }

  void ProduceFrame(uint64_t frame_number) {
    node_state_.in_count.fetch_add(1);
    FrameMetadata metadata;
    metadata.frame_id = frame_number;
    metadata.channel_id = channel_id_;
    metadata.wall_clock_ns = NowWallClock();
    metadata.monotonic_clock_ns = NowMonotonic();

    const auto handle = frame_store_.AddFrame(metadata);
    auto buffer_handle = frame_store_.AddBuffer(handle, std::make_unique<FrameBuffer>("synthetic"));
    (void)buffer_handle;  // buffer retrieval occurs in downstream nodes.

    const auto dropped = output_queue_.Push(handle);
    channel_state_.decoded_frames.fetch_add(1);
    node_state_.out_count.fetch_add(1);

    if (dropped.has_value()) {
      channel_state_.dropped_frames.fetch_add(1);
      node_state_.drop_count.fetch_add(1);
      Logger::Instance().Log(Logger::Level::kWarn, instance_id_,
                             "DropOldest during enqueue, frame_handle=" +
                                 std::to_string(dropped.value()));
      const auto dropped_buffer = frame_store_.GetBufferForFrame(dropped.value());
      if (dropped_buffer.has_value()) {
        frame_store_.ReleaseBuffer(dropped_buffer.value());
      }
      frame_store_.ReleaseFrame(dropped.value());
    }
  }

 private:
  uint64_t NowWallClock() const {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
  }

  uint64_t NowMonotonic() const {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
  }

  uint64_t channel_id_;
  FrameStore& frame_store_;
  ChannelState& channel_state_;
  NodeRuntimeState& node_state_;
  BoundedPointerQueue<FrameStore::FrameHandle>& output_queue_;
  std::string instance_id_;
};
