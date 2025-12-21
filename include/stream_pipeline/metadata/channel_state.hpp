#pragma once

#include "stream_pipeline/common/identifiers.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>

namespace stream_pipeline {

/*
 * ChannelConfiguration
 * --------------------
 * - Stage 0에서는 하드코딩해도 되지만, “설정 자리”를 구조로 확보한다.
 * - 최종 목표(동적 DAG/사용자 커스터마이즈)로 갈수록 이 설정이 중요해진다.
 */
struct ChannelConfiguration {
    bool enabled{true};

    // Stage 0 합의: throttle은 decode 단계에서 수행
    std::int32_t target_frames_per_second{15};

    // Stage 0 합의: 실시간이므로 drop-oldest를 기본 정책으로 사용
    std::size_t frame_queue_capacity{16};

    // Stage 0: output sampling(필요 시)
    std::int32_t output_every_n_frames{1};
};

/*
 * ChannelState
 * ------------
 * - 채널 단위 통계/설정의 기준점.
 * - output 누락, drop 발생, 처리량 등을 빠르게 파악하기 위해 Stage 0부터 카운터를 둔다.
 */
struct ChannelState {
    ChannelIdentifier channel_identifier{0};
    std::string channel_name;
    ChannelConfiguration configuration;

    std::atomic<std::uint64_t> decoded_frame_count{0};
    std::atomic<std::uint64_t> dropped_frame_count{0};
    std::atomic<std::uint64_t> output_frame_count{0};
};

}  // namespace stream_pipeline
