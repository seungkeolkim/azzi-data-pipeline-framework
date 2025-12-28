#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <unordered_map>

// NodeRuntimeState tracks per-node instance counters for observability.
struct NodeRuntimeState {
  std::atomic<uint64_t> in_count{0};
  std::atomic<uint64_t> out_count{0};
  std::atomic<uint64_t> drop_count{0};
  std::atomic<uint64_t> error_count{0};
};

class NodeRuntimeStateStore {
 public:
  NodeRuntimeState& GetOrCreate(const std::string& node_instance_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_map_[node_instance_id];
  }

 private:
  std::mutex mutex_;
  std::unordered_map<std::string, NodeRuntimeState> state_map_;
};
