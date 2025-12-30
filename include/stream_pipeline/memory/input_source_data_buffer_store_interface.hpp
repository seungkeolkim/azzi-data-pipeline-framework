#pragma once

#include "stream_pipeline/memory/input_source_data_buffer_types.hpp"

#include <cstddef>
#include <cstdint>

namespace stream_pipeline {

/*
 * 입력 소스 데이터 버퍼 스토어 인터페이스
 * --------------------------------------
 * - 입력 소스 데이터 버퍼의 수명과 소유권을 전담한다.
 * - 입력 소스와 디코드 사이에서는 이 스토어의 핸들만 전달한다.
 */
class InputSourceDataBufferStoreInterface {
public:
    virtual ~InputSourceDataBufferStoreInterface() = default;

    /*
     * 입력 버퍼 확보 결과 코드
     * -----------------------
     * - 입력 버퍼 확보에 특화된 결과 코드.
     */
    enum class AcquireResult : std::uint8_t {
        Success = 0,
        StoreClosed,
        OutOfMemory,
        InvalidArgument,
        InternalError
    };

    /*
     * 입력 버퍼 확보 결과 구조체
     * -------------------------
     * - 입력 버퍼 확보 반환 타입.
     * - 핸들 필드 이름을 명확히 적는다.
     */
    struct AcquireOutcome {
        AcquireResult result{AcquireResult::InternalError};
        InputSourceDataBufferHandle buffer_handle{0};
    };

    /*
     * 입력 버퍼 확보
     * -------------
     * - 지정한 크기의 입력 버퍼를 확보하고 핸들을 반환한다.
     */
    virtual AcquireOutcome acquire(std::size_t size_bytes) = 0;

    /*
     * 입력 버퍼 조회
     * -------------
     * - 핸들이 가리키는 메모리를 뷰로 제공한다.
     */
    virtual bool view(InputSourceDataBufferHandle buffer_handle, InputSourceDataBufferView& out_view) = 0;

    /*
     * 입력 버퍼 반환
     * -------------
     * - 핸들을 스토어로 반환한다.
     */
    virtual void release(InputSourceDataBufferHandle buffer_handle) = 0;

    /*
     * 스토어 종료
     * ----------
     * - 스토어를 종료한다.
     */
    virtual void close() = 0;
};

}
