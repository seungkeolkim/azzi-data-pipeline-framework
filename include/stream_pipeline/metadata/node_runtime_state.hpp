#pragma once

#include "stream_pipeline/common/identifiers.hpp"

#include <atomic>
#include <cstdint>
#include <string>

namespace stream_pipeline {

/*
 * NodeRuntimeState
 * ----------------
 * - 노드 인스턴스별 런타임 상태/카운터를 중앙 store가 소유한다.
 * - 노드 내부에 카운터를 두지 않고, store를 통해 관측/갱신한다.
 */
struct NodeRuntimeState {
    NodeRuntimeState();
    NodeRuntimeState(const NodeRuntimeState& other);
    NodeRuntimeState& operator=(const NodeRuntimeState& other);

    std::uint32_t struct_version{0};
    std::uint32_t struct_size_bytes{0};

    NodeInstanceIdentifier node_instance_identifier{0};
    std::string node_name;

    std::atomic<std::uint64_t> input_count{0};
    std::atomic<std::uint64_t> output_count{0};
    std::atomic<std::uint64_t> dropped_count{0};
    std::atomic<std::uint64_t> error_count{0};
};

inline NodeRuntimeState::NodeRuntimeState()
    : struct_version(1),
      struct_size_bytes(static_cast<std::uint32_t>(sizeof(NodeRuntimeState))) {}

inline NodeRuntimeState::NodeRuntimeState(const NodeRuntimeState& other)
    : struct_version(other.struct_version),
      struct_size_bytes(other.struct_size_bytes),
      node_instance_identifier(other.node_instance_identifier),
      node_name(other.node_name),
      input_count(other.input_count.load()),
      output_count(other.output_count.load()),
      dropped_count(other.dropped_count.load()),
      error_count(other.error_count.load()) {}

inline NodeRuntimeState& NodeRuntimeState::operator=(const NodeRuntimeState& other) {
    if (this != &other) {
        struct_version = other.struct_version;
        struct_size_bytes = other.struct_size_bytes;
        node_instance_identifier = other.node_instance_identifier;
        node_name = other.node_name;
        input_count.store(other.input_count.load());
        output_count.store(other.output_count.load());
        dropped_count.store(other.dropped_count.load());
        error_count.store(other.error_count.load());
    }
    return *this;
}

}  // namespace stream_pipeline
