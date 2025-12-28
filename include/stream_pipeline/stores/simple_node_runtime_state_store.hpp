#pragma once

#include "stream_pipeline/metadata/node_runtime_state_store_interface.hpp"

#include <mutex>
#include <unordered_map>

namespace stream_pipeline {

/*
 * SimpleNodeRuntimeStateStore
 * ---------------------------
 * - Stage 0.5용 mutex 기반 구현.
 * - 최초 접근 시 state를 생성하고 node_name은 register_node에서 주입한다.
 */
class SimpleNodeRuntimeStateStore final : public NodeRuntimeStateStoreInterface {
public:
    SimpleNodeRuntimeStateStore() = default;
    ~SimpleNodeRuntimeStateStore() override = default;

    void register_node(NodeInstanceIdentifier node_instance_identifier, const std::string& node_name) override;
    void read(NodeInstanceIdentifier node_instance_identifier, const ReadCallback& callback) const override;
    void write(NodeInstanceIdentifier node_instance_identifier, const WriteCallback& callback) override;

private:
    NodeRuntimeState& get_or_create_unlocked_(NodeInstanceIdentifier node_instance_identifier) const;

    mutable std::mutex mutex_;
    mutable std::unordered_map<NodeInstanceIdentifier, NodeRuntimeState> state_by_node_;
};

}  // namespace stream_pipeline
