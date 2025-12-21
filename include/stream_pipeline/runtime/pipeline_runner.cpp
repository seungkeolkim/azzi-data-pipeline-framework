#pragma once

#include "stream_pipeline/concurrency/bounded_pointer_queue.hpp"
#include "stream_pipeline/metadata/channel_state.hpp"
#include "stream_pipeline/metadata/global_state.hpp"
#include "stream_pipeline/runtime/node_interface.hpp"

#include <memory>
#include <vector>

namespace stream_pipeline {

/*
 * PipelineRunner (Stage 0)
 * ------------------------
 * - 노드 생성/시작/종료를 한 곳에 모아 “정지 순서”를 명확히 한다.
 *
 * Stage 0에서 정지 순서가 중요한 이유:
 * - pop_blocking()은 queue close가 없으면 join이 멈출 수 있다.
 * - 따라서 runner가 queue close와 노드 stop의 순서를 표준화해야 한다.
 */
class PipelineRunner {
public:
    PipelineRunner() = default;

    void start_all_nodes();
    void stop_all_nodes();

    // 노드 및 큐/상태를 외부에서 주입받기 위한 setter
    void set_nodes(std::vector<std::unique_ptr<NodeInterface>> nodes);

    // Stage 0: 큐 close를 위해 큐 포인터를 runner가 소유(참조)
    void set_queues_to_close(std::vector<BoundedPointerQueue<FrameMetadata*>*> queues_to_close);

private:
    std::vector<std::unique_ptr<NodeInterface>> nodes_;
    std::vector<BoundedPointerQueue<FrameMetadata*>*> queues_to_close_;
};

}  // namespace stream_pipeline
