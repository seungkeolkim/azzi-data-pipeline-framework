#include <chrono>
#include <thread>

#include "metadata/frame.h"
#include "store/channel_runtime_meta_store.h"
#include "store/node_runtime_state_store.h"
#include "utils/bounded_pointer_queue.h"
#include "utils/logger.h"
#include "pipeline/nodes/dummy_decode_node.h"
#include "pipeline/nodes/dummy_detection_node.h"
#include "pipeline/nodes/output_node.h"

int main() {
  FrameStore frame_store;
  ChannelState channel_state;
  ChannelRuntimeMetaStore channel_meta_store;
  NodeRuntimeStateStore node_state_store;

  BoundedPointerQueue<FrameStore::FrameHandle> decode_to_detect_queue(/*capacity=*/3);
  BoundedPointerQueue<FrameStore::BufferHandle> detect_to_output_queue(/*capacity=*/3);

  const uint64_t channel_id = 1;
  DummyDecodeNode decode_node(channel_id, frame_store, channel_state, node_state_store,
                              decode_to_detect_queue, "decode_node_1");
  DummyDetectionNode detection_node(channel_id, frame_store, channel_meta_store, node_state_store,
                                    decode_to_detect_queue, detect_to_output_queue,
                                    "detection_node_1");
  OutputNode output_node(channel_id, frame_store, channel_state, channel_meta_store,
                         node_state_store, detect_to_output_queue, "output_node_1");

  decode_node.Start();
  detection_node.Start();
  output_node.Start();

  const uint64_t kVehicleTtlNs =
      std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(2)).count();

  for (uint64_t frame = 1; frame <= 6; ++frame) {
    decode_node.ProduceFrame(frame);
    // Simulate pipeline progression.
    detection_node.ProcessNext();
    output_node.ProcessNext(kVehicleTtlNs);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }

  // Drain remaining items.
  while (detection_node.ProcessNext()) {
    output_node.ProcessNext(kVehicleTtlNs);
  }
  while (output_node.ProcessNext(kVehicleTtlNs)) {
  }

  decode_node.Stop();
  detection_node.Stop();
  output_node.Stop();

  Logger::Instance().Log(Logger::Level::kInfo, "main", "Channel decoded=" +
                                                         std::to_string(channel_state.decoded_frames.load()) +
                                                         " dropped=" +
                                                         std::to_string(channel_state.dropped_frames.load()) +
                                                         " output=" +
                                                         std::to_string(channel_state.output_frames.load()));

  return 0;
}
