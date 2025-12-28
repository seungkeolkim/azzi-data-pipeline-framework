#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

#include "utils/logger.h"

// FrameMetadata contains information about where and when a frame was generated.
struct FrameMetadata {
  uint64_t frame_id{0};
  uint64_t channel_id{0};

  // Wall-clock and monotonic timestamps are both recorded to allow cross-node latency
  // measurements while still providing monotonic ordering.
  uint64_t wall_clock_ns{0};
  uint64_t monotonic_clock_ns{0};
};

// FrameBuffer stores the raw bytes for a frame. In Stage 0.5 the buffer is synthetic.
struct FrameBuffer {
  explicit FrameBuffer(std::string payload) : payload(std::move(payload)) {}
  std::string payload;
};

// ObjectMetadata represents detection output attached to a frame buffer. In Stage 0.5 this is
// synthetic and uses a simple label.
struct ObjectMetadata {
  uint64_t object_id{0};
  uint64_t frame_id{0};
  std::string label;
};

// ChannelState holds per-channel atomic counters that were already available in Stage 0.
struct ChannelState {
  std::atomic<uint64_t> decoded_frames{0};
  std::atomic<uint64_t> dropped_frames{0};
  std::atomic<uint64_t> output_frames{0};
};

// Store contracts: the store owns the underlying objects; nodes only keep handles.
// Handles are represented as integer IDs to keep ownership clear.

class FrameStore {
 public:
  using FrameHandle = uint64_t;
  using BufferHandle = uint64_t;
  using ObjectHandle = uint64_t;

  FrameHandle AddFrame(FrameMetadata metadata) {
    const FrameHandle handle = next_frame_handle_++;
    frames_.emplace(handle, std::move(metadata));
    return handle;
  }

  BufferHandle AddBuffer(FrameHandle frame_handle, std::unique_ptr<FrameBuffer> buffer) {
    const BufferHandle handle = next_buffer_handle_++;
    buffers_.emplace(handle, std::move(buffer));
    frame_to_buffer_[frame_handle] = handle;
    buffer_to_frame_[handle] = frame_handle;
    return handle;
  }

  ObjectHandle AddObject(BufferHandle buffer_handle, std::unique_ptr<ObjectMetadata> object) {
    const ObjectHandle handle = next_object_handle_++;
    objects_.emplace(handle, std::move(object));
    buffer_to_object_[buffer_handle] = handle;
    return handle;
  }

  FrameMetadata* GetFrame(FrameHandle handle) {
    const auto iter = frames_.find(handle);
    return iter == frames_.end() ? nullptr : &iter->second;
  }

  FrameBuffer* GetBuffer(BufferHandle handle) {
    const auto iter = buffers_.find(handle);
    return iter == buffers_.end() ? nullptr : iter->second.get();
  }

  ObjectMetadata* GetObject(ObjectHandle handle) {
    const auto iter = objects_.find(handle);
    return iter == objects_.end() ? nullptr : iter->second.get();
  }

  // Release chain respects Stage 0: object -> buffer -> frame.
  void ReleaseObject(ObjectHandle handle) {
    objects_.erase(handle);
  }

  void ReleaseBuffer(BufferHandle handle) {
    buffer_to_object_.erase(handle);
    buffer_to_frame_.erase(handle);
    buffers_.erase(handle);
  }

  void ReleaseFrame(FrameHandle handle) {
    const auto buffer_iter = frame_to_buffer_.find(handle);
    if (buffer_iter != frame_to_buffer_.end()) {
      frame_to_buffer_.erase(buffer_iter);
    }
    frames_.erase(handle);
  }

  std::optional<BufferHandle> GetBufferForFrame(FrameHandle frame_handle) {
    const auto iter = frame_to_buffer_.find(frame_handle);
    if (iter == frame_to_buffer_.end()) {
      return std::nullopt;
    }
    return iter->second;
  }

  std::optional<ObjectHandle> GetObjectForBuffer(BufferHandle buffer_handle) {
    const auto iter = buffer_to_object_.find(buffer_handle);
    if (iter == buffer_to_object_.end()) {
      return std::nullopt;
    }
    return iter->second;
  }

  const std::unordered_map<FrameHandle, BufferHandle>& GetBufferForFrameMap() const {
    return frame_to_buffer_;
  }

  std::optional<FrameHandle> GetFrameForBuffer(BufferHandle buffer_handle) const {
    const auto iter = buffer_to_frame_.find(buffer_handle);
    if (iter == buffer_to_frame_.end()) {
      return std::nullopt;
    }
    return iter->second;
  }

 private:
  FrameHandle next_frame_handle_{1};
  BufferHandle next_buffer_handle_{1};
  ObjectHandle next_object_handle_{1};

  std::unordered_map<FrameHandle, FrameMetadata> frames_;
  std::unordered_map<BufferHandle, std::unique_ptr<FrameBuffer>> buffers_;
  std::unordered_map<ObjectHandle, std::unique_ptr<ObjectMetadata>> objects_;

  std::unordered_map<FrameHandle, BufferHandle> frame_to_buffer_;
  std::unordered_map<BufferHandle, FrameHandle> buffer_to_frame_;
  std::unordered_map<BufferHandle, ObjectHandle> buffer_to_object_;
};
