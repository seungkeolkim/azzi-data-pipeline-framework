#pragma once

#include "stream_pipeline/metadata/node_runtime_state.hpp"

#include <functional>
#include <string>

namespace stream_pipeline {

/*
 * NodeRuntimeStateStoreInterface
 * ------------------------------
 * - 노드 인스턴스별 런타임 상태를 중앙에서 관리한다.
 * - NodeInstanceIdentifier를 key로 사용하며, closure 기반 접근으로 lock 범위를 통제한다.
 */
class NodeRuntimeStateStoreInterface {
public:
    using ReadCallback = std::function<void(const NodeRuntimeState&)>;
    using WriteCallback = std::function<void(NodeRuntimeState&)>;

    virtual ~NodeRuntimeStateStoreInterface() = default;

    virtual void register_node(NodeInstanceIdentifier node_instance_identifier, const std::string& node_name) = 0;
    virtual void read(NodeInstanceIdentifier node_instance_identifier, const ReadCallback& callback) const = 0;
    virtual void write(NodeInstanceIdentifier node_instance_identifier, const WriteCallback& callback) = 0;
};

}  // namespace stream_pipeline
