#pragma once

#include <cstdint>
#include <chrono>

// ChannelRuntimeMeta stores cross-frame, per-channel runtime state.
struct ChannelRuntimeMeta {
  static constexpr uint32_t kStructVersion = 1;

  uint32_t struct_version{kStructVersion};
  uint32_t struct_size_bytes{sizeof(ChannelRuntimeMeta)};

  // Last time specific classes were observed. Stored as monotonic nanoseconds.
  uint64_t last_seen_vehicle_monotonic_ns{0};
  uint64_t last_seen_person_monotonic_ns{0};

  // Downstream to upstream control knobs. Defaults keep behavior unchanged.
  int32_t decode_fps_limit{-1};
  int32_t detection_skip_frames{0};

  bool IsVehiclePresent(uint64_t now_monotonic_ns, uint64_t ttl_ns) const {
    return now_monotonic_ns <= last_seen_vehicle_monotonic_ns + ttl_ns;
  }

  bool IsPersonPresent(uint64_t now_monotonic_ns, uint64_t ttl_ns) const {
    return now_monotonic_ns <= last_seen_person_monotonic_ns + ttl_ns;
  }
};
