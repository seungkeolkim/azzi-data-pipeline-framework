#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <unordered_map>

// 노드 인스턴스 단위 카운터를 저장하는 store.
struct NodeRuntimeCounters {
    std::atomic<uint64_t> in_count{0};
    std::atomic<uint64_t> out_count{0};
    std::atomic<uint64_t> drop_count{0};
    std::atomic<uint64_t> error_count{0};
};

class NodeRuntimeStateStore {
public:
    NodeRuntimeCounters& Get(const std::string& node_instance_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        return counters_[node_instance_id];
    }

private:
    std::unordered_map<std::string, NodeRuntimeCounters> counters_;
    std::mutex mutex_;
};
