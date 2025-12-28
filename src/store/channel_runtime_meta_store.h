#pragma once

#include <functional>
#include <mutex>
#include <unordered_map>

#include "metadata/channel_runtime_meta.h"

// Store for ChannelRuntimeMeta. The store owns the meta objects; callers access them via
// read/write lambdas so the store can coordinate locking.
class ChannelRuntimeMetaStore {
 public:
  using ChannelIdentifier = uint64_t;

  void Read(ChannelIdentifier channel_id,
            const std::function<void(const ChannelRuntimeMeta&)>& reader) {
    std::lock_guard<std::mutex> lock(mutex_);
    ChannelRuntimeMeta& meta = meta_map_[channel_id];
    reader(meta);
  }

  void Write(ChannelIdentifier channel_id, const std::function<void(ChannelRuntimeMeta&)>& writer) {
    std::lock_guard<std::mutex> lock(mutex_);
    ChannelRuntimeMeta& meta = meta_map_[channel_id];
    writer(meta);
  }

 private:
  std::mutex mutex_;
  std::unordered_map<ChannelIdentifier, ChannelRuntimeMeta> meta_map_;
};
