#include "stream_pipeline/runtime/pipeline_runner.hpp"

namespace stream_pipeline {

void PipelineRunner::set_nodes(std::vector<std::unique_ptr<NodeInterface>> nodes) {
    nodes_ = std::move(nodes);
}

void PipelineRunner::set_queues_to_close(std::vector<BoundedPointerQueue<FrameMetadata*>*> queues_to_close) {
    queues_to_close_ = std::move(queues_to_close);
}

void PipelineRunner::start_all_nodes() {
    for (auto& node : nodes_) {
        node->start();
    }
}

void PipelineRunner::stop_all_nodes() {
    // 1) queue close부터 수행한다.
    // - pop_blocking()을 깨우기 위해 필수.
    for (auto* queue_pointer : queues_to_close_) {
        if (queue_pointer != nullptr) {
            queue_pointer->close();
        }
    }

    // 2) 노드 stop (join) 수행
    for (auto& node : nodes_) {
        node->stop();
    }
}

}  // namespace stream_pipeline
