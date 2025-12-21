#pragma once

#include <cstdint>

namespace stream_pipeline {

/*
 * NodeInterface
 * -------------
 * - Node = thread/worker라는 합의를 코드 계약으로 고정한다.
 *
 * Stage 0:
 * - DecodeNode (Thread 1)
 * - DummyDetectionNode (Thread 2)
 * - OutputNode (Thread 3)
 *
 * 원칙:
 * - start(): 내부 스레드 생성 및 실행 시작
 * - stop(): 종료 요청 + join 완료
 */
class NodeInterface {
public:
    virtual ~NodeInterface() = default;

    /*
     * node_name()
     * -----------
     * - 로깅/디버깅/프로파일링에서 노드를 식별하는 문자열.
     * - string 생성 비용을 피하기 위해 const char*로 둔다.
     * - 구현은 string literal 반환을 권장.
     */
    virtual const char* node_name() const = 0;

    /*
     * StartResult
     * -----------
     * - start 연산에 특화된 결과 코드.
     * - 공용 Status/Result 공유 금지(사용자 원칙).
     */
    enum class StartResult : std::uint8_t {
        Success = 0,
        AlreadyRunning,
        InvalidState,
        InternalError
    };

    virtual StartResult start() = 0;

    /*
     * stop()
     * ------
     * - stop 요청 + join 완료까지 책임진다.
     * - "파이프라인 전체가 멈추지 않도록" 각 노드는 내부 실패를 격리하고,
     *   stop은 항상 안전하게 종료될 수 있어야 한다.
     */
    virtual void stop() = 0;
};

}  // namespace stream_pipeline
