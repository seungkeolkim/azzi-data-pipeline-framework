#pragma once

#include "stream_pipeline/concurrency/bounded_pointer_queue.hpp"
#include "stream_pipeline/metadata/channel_state.hpp"
#include "stream_pipeline/metadata/input_source_queue_item.hpp"
#include "stream_pipeline/metadata/node_runtime_state_store_interface.hpp"
#include "stream_pipeline/memory/input_source_data_buffer_store_interface.hpp"
#include "stream_pipeline/nodes/input_source_node.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace stream_pipeline {

/*
 * 더미 입력 소스 노드
 * -------------------
 * - 0.6 단계에서 입력 소스 구조를 검증하기 위한 더미 구현이다.
 * - 풀 모드에서는 내부 스레드가 일정 주기로 데이터 덩어리를 생성한다.
 * - 푸시 모드에서는 외부 호출을 통해 데이터가 들어온다고 가정한다.
 */
class DummyInputSourceNode final : public InputSourceNode {
public:
    struct Configuration {
        std::size_t size_bytes_per_item{1024};
        std::int32_t items_per_second{15};
    };

    DummyInputSourceNode(
        ChannelState& channel_state,
        InputSourceDataBufferStoreInterface& input_source_data_buffer_store,
        NodeRuntimeStateStoreInterface& node_runtime_state_store,
        BoundedPointerQueue<InputSourceQueueItem*>& output_queue,
        const Configuration& configuration,
        InputSourceMode input_source_mode = InputSourceMode::Pull);

    ~DummyInputSourceNode() override = default;

    const char* node_name() const override;

private:
    void thread_entry_() override;
    bool handle_external_push_(const std::uint8_t* data_pointer, std::size_t size_bytes) override;

    InputSourceQueueItem* create_queue_item_(const std::uint8_t* data_pointer, std::size_t size_bytes);
    void release_queue_item_(InputSourceQueueItem* item_pointer);
    void fill_dummy_bytes_(std::uint8_t* data_pointer, std::size_t size_bytes, std::uint64_t sequence_number);

    ChannelState& channel_state_;
    InputSourceDataBufferStoreInterface& input_source_data_buffer_store_;
    NodeRuntimeStateStoreInterface& node_runtime_state_store_;
    BoundedPointerQueue<InputSourceQueueItem*>& output_queue_;
    Configuration configuration_;

    NodeInstanceIdentifier node_instance_identifier_{0};
    std::atomic<std::uint64_t> sequence_counter_{0};
};

}
