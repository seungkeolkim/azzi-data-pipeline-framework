#pragma once

#include <atomic>

namespace stream_pipeline {

/*
 * GlobalState
 * -----------
 * - Stage 0에서는 “파이프라인 실행/종료” 정도만 최소로 둔다.
 * - 이후 노드 레지스트리, 플러그인, 카탈로그 등이 붙더라도
 *   running 플래그 같은 제어 변수는 계속 유효하게 남는다.
 */
struct GlobalState {
    std::atomic<bool> running{true};
};

}  // namespace stream_pipeline
