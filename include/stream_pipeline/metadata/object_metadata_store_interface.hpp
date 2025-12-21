#pragma once

#include "stream_pipeline/metadata/object_metadata.hpp"
#include <cstdint>

namespace stream_pipeline {

/*
 * ObjectHandle
 * ------------
 * - ObjectMetadataStoreInterface 내부 객체를 참조하는 opaque handle.
 * - FrameMetadata에는 handle만 저장되고, ObjectMetadata 자체는 store가 소유한다.
 */
using ObjectHandle = std::uint64_t;

/*
 * ObjectMetadataStoreInterface
 * ----------------------------
 * - ObjectMetadata의 생성/조회/해제를 담당하는 store 계약.
 *
 * Stage 0:
 * - 프레임마다 object 1개 생성 → output 후 즉시 release.
 *
 * Stage 1+:
 * - tracker/occlusion이 붙으면, release가 “즉시 파기”가 아니라
 *   store 내부에서 shadow/probation 정책을 통해 유예될 수 있다.
 * - 외부 API는 가능하면 유지(create/read/release).
 */
class ObjectMetadataStoreInterface {
public:
    virtual ~ObjectMetadataStoreInterface() = default;

    /*
     * CreateResult
     * ------------
     * - object 생성 연산에 특화된 결과 코드.
     * - 공용 Status/Result 공유 금지(사용자 원칙).
     */
    enum class CreateResult : std::uint8_t {
        Success = 0,
        StoreClosed,
        OutOfMemory,
        InvalidArgument,
        InternalError
    };

    /*
     * CreateOutcome
     * -------------
     * - field 이름은 object_handle로 명확히 적는다.
     */
    struct CreateOutcome {
        CreateResult result{CreateResult::InternalError};
        ObjectHandle object_handle{0};
    };

    /*
     * create()
     * --------
     * - object_metadata를 store에 등록하고 object_handle 반환.
     */
    virtual CreateOutcome create(const ObjectMetadata& object_metadata) = 0;

    /*
     * read()
     * ------
     * - object_handle에 해당하는 ObjectMetadata를 out_object_metadata로 복사.
     *
     * 반환:
     * - true  : 성공
     * - false : invalid handle / store closed / 내부 오류
     */
    virtual bool read(ObjectHandle object_handle, ObjectMetadata& out_object_metadata) = 0;

    /*
     * release()
     * ----------
     * - object_handle을 store로 반환.
     * - Stage 0에서는 output 이후 즉시 반환.
     */
    virtual void release(ObjectHandle object_handle) = 0;

    /*
     * close()
     * -------
     * - store 종료.
     */
    virtual void close() = 0;
};

}  // namespace stream_pipeline
