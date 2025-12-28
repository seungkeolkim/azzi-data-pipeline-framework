#pragma once

#include <cstdint>

#include "metadata/frame.h"

// Stage 0.5에서 요구하는 글로벌/프레임/윈도우 메타데이터를 단일 구조체로 제공한다.
struct ChannelRuntimeMeta {
    // ABI 안정성을 위한 메타 정보.
    uint32_t struct_version = 1;
    uint32_t struct_size_bytes = sizeof(ChannelRuntimeMeta);

    // 최근 관측 시각(ns, monotonic). TTL 판단에 사용한다.
    int64_t last_seen_vehicle_monotonic_ns = 0;
    int64_t last_seen_person_monotonic_ns = 0;

    // 다운스트림에서 업스트림으로 제어할 수 있는 정책 값.
    int32_t decode_fps_limit = -1;
    int32_t detection_skip_frames = 0;

    // TTL 기반 presence 판정 helper. now_ns는 monotonic 시간을 넣는다.
    bool IsVehiclePresent(int64_t now_ns, int64_t ttl_ns) const {
        return (now_ns - last_seen_vehicle_monotonic_ns) <= ttl_ns;
    }

    bool IsPersonPresent(int64_t now_ns, int64_t ttl_ns) const {
        return (now_ns - last_seen_person_monotonic_ns) <= ttl_ns;
    }
};
