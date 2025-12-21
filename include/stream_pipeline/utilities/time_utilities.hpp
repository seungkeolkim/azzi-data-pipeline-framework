#pragma once

#include "stream_pipeline/common/time_types.hpp"

namespace stream_pipeline {

/*
 * now_timestamp_pair()
 * --------------------
 * - wall-clock(Unix epoch) + monotonic time을 동시에 찍는다.
 *
 * 설계 메모:
 * - Stage 0에서는 "decode 완료 직후"에 decode_completed_timestamp를 찍는다.
 * - 그러나 이상적인 구조는 receive_timestamp / decode_completed_timestamp를 분리하는 것이다.
 *   이 문서는 TODO로 남겼고, 구현은 Stage 0 단순화를 위해 여기 함수 하나로 통일한다.
 */
TimestampPair now_timestamp_pair();

}  // namespace stream_pipeline
