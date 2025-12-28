#pragma once

#include <cstdint>

namespace stream_pipeline {

/*
 * ChannelRuntimeMeta (Stage 0.5)
 * ------------------------------
 * - 프레임을 넘어 유지되는 채널 단위 메타 정보의 최소 골격.
 * - Stage 0.5에서는 단일 struct로 단순화를 유지한다.
 * - TTL 기반 존재 여부 판단을 위해 monotonic timestamp를 기록한다.
 */
struct ChannelRuntimeMeta {
    ChannelRuntimeMeta();

    std::uint32_t struct_version{0};
    std::uint32_t struct_size_bytes{0};

    // monotonic clock 기준 최근 관측 시점(ns)
    std::uint64_t last_seen_vehicle_monotonic_ns{0};
    std::uint64_t last_seen_person_monotonic_ns{0};

    // TTL(ns). Stage 0.5에서는 고정 값으로 단순화한다.
    std::uint64_t vehicle_presence_ttl_nanoseconds{2'000'000'000ULL};
    std::uint64_t person_presence_ttl_nanoseconds{2'000'000'000ULL};

    // downstream -> upstream 제어 신호(자리만 확보)
    std::int64_t decode_fps_limit{-1};
    std::int64_t detection_skip_frames{0};

    bool is_vehicle_present(std::uint64_t now_monotonic_ns) const;
    bool is_person_present(std::uint64_t now_monotonic_ns) const;
};

inline ChannelRuntimeMeta::ChannelRuntimeMeta()
    : struct_version(1),
      struct_size_bytes(static_cast<std::uint32_t>(sizeof(ChannelRuntimeMeta))) {}

inline bool ChannelRuntimeMeta::is_vehicle_present(std::uint64_t now_monotonic_ns) const {
    if (last_seen_vehicle_monotonic_ns == 0) {
        return false;
    }
    if (now_monotonic_ns <= last_seen_vehicle_monotonic_ns) {
        return true;
    }
    const std::uint64_t elapsed = now_monotonic_ns - last_seen_vehicle_monotonic_ns;
    return elapsed <= vehicle_presence_ttl_nanoseconds;
}

inline bool ChannelRuntimeMeta::is_person_present(std::uint64_t now_monotonic_ns) const {
    if (last_seen_person_monotonic_ns == 0) {
        return false;
    }
    if (now_monotonic_ns <= last_seen_person_monotonic_ns) {
        return true;
    }
    const std::uint64_t elapsed = now_monotonic_ns - last_seen_person_monotonic_ns;
    return elapsed <= person_presence_ttl_nanoseconds;
}

}  // namespace stream_pipeline
