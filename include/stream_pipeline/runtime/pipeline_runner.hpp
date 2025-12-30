#pragma once

#include "stream_pipeline/concurrency/bounded_pointer_queue.hpp"
#include "stream_pipeline/metadata/channel_state.hpp"
#include "stream_pipeline/metadata/global_state.hpp"
#include "stream_pipeline/metadata/frame_metadata.hpp"
#include "stream_pipeline/metadata/input_source_queue_item.hpp"
#include "stream_pipeline/runtime/node_interface.hpp"

#include <memory>
#include <vector>

namespace stream_pipeline {

/*
 * 파이프라인 실행 관리자
 * ----------------------
 * - 노드 생성, 시작, 종료를 한 곳에 모아 정지 순서를 명확히 한다.
 *
 * 정지 순서가 중요한 이유:
 * - 블로킹 팝은 큐 종료가 없으면 대기가 끝나지 않을 수 있다.
 * - 따라서 실행 관리자가 큐 종료와 노드 정지의 순서를 표준화해야 한다.
 */
class PipelineRunner {
public:
    PipelineRunner() = default;

    void start_all_nodes();
    void stop_all_nodes();

    // 노드 및 큐를 외부에서 주입받기 위한 설정 함수
    void set_nodes(std::vector<std::unique_ptr<NodeInterface>> nodes);

    // 큐 종료를 위해 큐 포인터를 실행 관리자가 보관한다
    void set_queues_to_close(std::vector<BoundedPointerQueue<FrameMetadata*>*> queues_to_close);

    void set_input_source_queues_to_close(
        std::vector<BoundedPointerQueue<InputSourceQueueItem*>*> input_source_queues_to_close);

private:
    std::vector<std::unique_ptr<NodeInterface>> nodes_;
    std::vector<BoundedPointerQueue<FrameMetadata*>*> queues_to_close_;
    std::vector<BoundedPointerQueue<InputSourceQueueItem*>*> input_source_queues_to_close_;
};

}
