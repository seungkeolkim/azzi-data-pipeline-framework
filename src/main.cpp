#include <chrono>
#include <thread>

#include "metadata/frame.h"
#include "pipeline/nodes/dummy_decode_node.h"
#include "pipeline/nodes/dummy_detection_node.h"
#include "pipeline/nodes/output_node.h"
#include "store/channel_runtime_meta_store.h"
#include "store/node_runtime_state_store.h"
#include "utils/logger.h"

int main() {
    LOG_INFO("Stage 0.5 데모 파이프라인 시작");

    FrameStore frame_store;
    ChannelRuntimeMetaStore channel_meta_store;
    NodeRuntimeStateStore node_state_store;

    // 각 큐는 DropOldestItem 반환을 관찰하기 위해 작은 용량으로 둔다.
    BoundedPointerQueue<FrameHandle> decode_to_detect_queue(5);
    BoundedPointerQueue<FrameHandle> detect_to_output_queue(5);

    ChannelIdentifier channel{"demo_stream"};
    const int64_t ttl_ns = std::chrono::seconds(2).count();

    DummyDecodeNode decode_node("decode_0", frame_store, decode_to_detect_queue, node_state_store, channel);
    DummyDetectionNode detection_node("detect_0", frame_store, decode_to_detect_queue, detect_to_output_queue,
                                      channel_meta_store, node_state_store, ttl_ns);
    OutputNode output_node("output_0", frame_store, detect_to_output_queue, channel_meta_store, node_state_store,
                          ttl_ns);

    decode_node.Run(12);

    // 디코드 → 감지 변환 루프.
    while (decode_to_detect_queue.Size() > 0) {
        detection_node.RunOnce();
    }

    // 감지 → 출력 루프.
    while (detect_to_output_queue.Size() > 0) {
        output_node.RunOnce();
    }

    LOG_INFO("Stage 0.5 데모 파이프라인 종료");
    return 0;
}
