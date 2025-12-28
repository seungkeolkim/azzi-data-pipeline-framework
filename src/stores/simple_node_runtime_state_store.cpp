#include "stream_pipeline/stores/simple_node_runtime_state_store.hpp"

#include <utility>

namespace stream_pipeline {

void SimpleNodeRuntimeStateStore::register_node(
    NodeInstanceIdentifier node_instance_identifier,
    const std::string& node_name) {
    std::lock_guard<std::mutex> lock(mutex_);
    NodeRuntimeState& state = get_or_create_unlocked_(node_instance_identifier);
    state.node_instance_identifier = node_instance_identifier;
    if (state.node_name.empty()) {
        state.node_name = node_name;
    }
}

void SimpleNodeRuntimeStateStore::read(
    NodeInstanceIdentifier node_instance_identifier,
    const ReadCallback& callback) const {
    if (!callback) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    const NodeRuntimeState& state = get_or_create_unlocked_(node_instance_identifier);
    callback(state);
}

void SimpleNodeRuntimeStateStore::write(
    NodeInstanceIdentifier node_instance_identifier,
    const WriteCallback& callback) {
    if (!callback) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    NodeRuntimeState& state = get_or_create_unlocked_(node_instance_identifier);
    state.node_instance_identifier = node_instance_identifier;
    callback(state);
}

NodeRuntimeState& SimpleNodeRuntimeStateStore::get_or_create_unlocked_(
    NodeInstanceIdentifier node_instance_identifier) const {
    auto iterator = state_by_node_.find(node_instance_identifier);
    if (iterator == state_by_node_.end()) {
        auto [inserted_iterator, _] = state_by_node_.emplace(node_instance_identifier, NodeRuntimeState{});
        return inserted_iterator->second;
    }
    return iterator->second;
}

}  // namespace stream_pipeline
