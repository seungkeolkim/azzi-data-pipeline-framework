#pragma once

#include "stream_pipeline/common/identifiers.hpp"
#include <cstdint>

namespace stream_pipeline {

/*
 * BoundingBox
 * ----------
 * - Stage 0부터 일관된 bbox 표현을 사용한다.
 * - 좌표계는 “픽셀 좌표(left, top, width, height)”를 기본으로 한다.
 *
 * 주의:
 * - 정규화(0~1) 좌표는 혼동을 유발하므로 Stage 0에서는 사용하지 않는 것을 권장.
 */
struct BoundingBox {
    float left{0.0f};
    float top{0.0f};
    float width{0.0f};
    float height{0.0f};
};

/*
 * ObjectMetadata
 * --------------
 * - detection/tracking 결과의 최소 계약.
 *
 * 중요한 구분:
 * - object_identifier: “논리적 ID” (tracker가 붙으면 의미 확장 가능)
 * - object_handle: store 내부 객체 참조용 “불투명 핸들”
 */
struct ObjectMetadata {
    ObjectIdentifier object_identifier{0};

    std::int32_t class_identifier{0};
    float confidence_score{0.0f};
    BoundingBox bounding_box{};

    // tracker 연동 슬롯 (Stage 0에서는 사용하지 않음)
    std::int64_t tracking_identifier{-1};
};

}  // namespace stream_pipeline
