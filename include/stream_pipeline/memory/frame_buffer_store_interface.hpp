#pragma once

#include "stream_pipeline/memory/frame_buffer_types.hpp"
#include <cstdint>

namespace stream_pipeline {

/*
 * FrameBufferStoreInterface
 * -------------------------
 * - 프레임 버퍼의 수명(lifecycle)을 전담하는 store.
 * - FrameMetadata는 frame_buffer_handle만 갖고, 실제 메모리는 store가 소유한다.
 *
 * Stage 0:
 * - 단순 동적 할당(풀처럼 보이게)로 시작 가능.
 *
 * Stage 1/2:
 * - 고정 해상도 풀, 슬랩, red-zone, allocator 정책 등을 store 내부에서 교체 가능.
 *
 * 중요한 원칙:
 * - 결과 타입은 공용 Status/Result로 공유하지 않는다.
 *   “이 함수가 무엇을 하는지”와 “어느 컴포넌트의 에러인지”가 즉시 보여야 한다.
 */
class FrameBufferStoreInterface {
public:
    virtual ~FrameBufferStoreInterface() = default;

    /*
     * AcquireResult
     * ------------
     * - 버퍼 확보 연산에 특화된 결과 코드.
     */
    enum class AcquireResult : std::uint8_t {
        Success = 0,
        StoreClosed,
        OutOfMemory,
        UnsupportedFormat,
        InvalidArgument,
        InternalError
    };

    /*
     * AcquireOutcome
     * -------------
     * - acquire() 반환 타입.
     * - field 이름은 frame_buffer_handle로 명확히 적는다(F12 없이 의미 파악).
     */
    struct AcquireOutcome {
        AcquireResult result{AcquireResult::InternalError};
        FrameBufferHandle frame_buffer_handle{0};
    };

    /*
     * acquire()
     * ---------
     * - description에 맞는 버퍼를 확보하고 handle을 반환.
     * - width/height 고정 정책은 store 구현에서 강제할 수 있다.
     */
    virtual AcquireOutcome acquire(const FrameBufferDescription& frame_buffer_description) = 0;

    /*
     * view()
     * ------
     * - handle이 가리키는 메모리를 FrameBufferView로 제공.
     * - output/encode/overlay 등에서 참조가 필요할 때 사용.
     *
     * 반환:
     * - true  : out_frame_buffer_view 유효
     * - false : invalid handle / store closed / 내부 오류
     */
    virtual bool view(FrameBufferHandle frame_buffer_handle, FrameBufferView& out_frame_buffer_view) = 0;

    /*
     * release()
     * ----------
     * - handle을 store로 반환(재사용/해제는 store 정책).
     * - Stage 0에서는 OutputNode가 정리 후 release하는 것이 가장 단순하다.
     */
    virtual void release(FrameBufferHandle frame_buffer_handle) = 0;

    /*
     * close()
     * -------
     * - store 종료.
     */
    virtual void close() = 0;
};

}  // namespace stream_pipeline
