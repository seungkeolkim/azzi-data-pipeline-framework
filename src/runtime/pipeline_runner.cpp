#include "stream_pipeline/runtime/pipeline_runner.hpp"

namespace stream_pipeline {

void PipelineRunner::set_nodes(std::vector<std::unique_ptr<NodeInterface>> nodes) {
    nodes_ = std::move(nodes);
}

void PipelineRunner::set_queues_to_close(std::vector<BoundedPointerQueue<FrameMetadata*>*> queues_to_close) {
    queues_to_close_ = std::move(queues_to_close);
}

void PipelineRunner::set_input_source_queues_to_close(
    std::vector<BoundedPointerQueue<InputSourceQueueItem*>*> input_source_queues_to_close) {
    input_source_queues_to_close_ = std::move(input_source_queues_to_close);
}

void PipelineRunner::start_all_nodes() {
    for (auto& node : nodes_) {
        node->start();
    }
}

void PipelineRunner::stop_all_nodes() {
    // 1) 큐 종료부터 수행한다.
    // - 블로킹 팝을 깨우기 위해 필요하다.
    for (auto* queue_pointer : input_source_queues_to_close_) {
        if (queue_pointer != nullptr) {
            queue_pointer->close();
        }
    }

    for (auto* queue_pointer : queues_to_close_) {
        if (queue_pointer != nullptr) {
            queue_pointer->close();
        }
    }

    // 2) 노드 정지와 조인을 수행한다
    for (auto& node : nodes_) {
        node->stop();
    }
}

}
