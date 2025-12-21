#pragma once

#include <cstdint>

namespace stream_pipeline {

/*
 * Time Types
 * ----------
 * 시간은 혼동되면 가장 큰 사고가 나는 영역이다.
 * - “무슨 기준의 시간인지”가 타입 이름에 포함되어야 한다.
 * - 운영/디버깅 시 wall-clock과 monotonic을 함께 남기는 것이 유리하다.
 *
 * Stage 0 가정:
 * - 실시간 RTSP만 가정하되, playback/external까지 확장될 수 있게 슬롯은 둔다.
 */

// Epoch 기반 wall-clock (UTC 기준 epoch time)
using UnixTimeNanoseconds = std::int64_t;

// Monotonic clock (epoch 의미 없음, latency 측정에 유리)
using MonotonicTimeNanoseconds = std::int64_t;

/*
 * StreamTimeBase
 * --------------
 * - 이 스트림의 timestamp가 어떤 시간축에 기반하는지 나타낸다.
 */
enum class StreamTimeBase : std::uint8_t {
    Realtime = 0,  // live RTSP
    Playback = 1,  // recorded playback
    External = 2   // timestamps supplied externally (e.g., custom TCP ingest)
};

/*
 * TimestampPair
 * -------------
 * - wall-clock + monotonic의 “한 쌍”.
 * - stage별로 이 pair를 찍으면:
 *   1) wall-clock으로 “언제 발생했는지” 확인 가능
 *   2) monotonic으로 “지연/latency” 안정적으로 계산 가능
 */
struct TimestampPair {
    UnixTimeNanoseconds wall_clock_time_nanoseconds{0};
    MonotonicTimeNanoseconds monotonic_time_nanoseconds{0};
};

}  // namespace stream_pipeline
