#pragma once

#include "stream_pipeline/common/identifiers.hpp"
#include "stream_pipeline/common/time_types.hpp"
#include "stream_pipeline/memory/input_source_data_buffer_types.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace stream_pipeline {

/*
 * 입력 소스 큐 아이템
 * -------------------
 * - 현재 구현은 원시 바이트 덩어리다.
 * - 프레임 또는 접근 단위로 승격할 수 있다는 가능성을 후속 과제로 남긴다.
 * - 하나의 프레임이 여러 큐 슬롯에 걸칠 수 있다.
 * - 바이트 데이터는 이 구조체가 소유하지 않는다.
 */
struct InputSourceQueueItem {
    ChannelIdentifier channel_identifier{0};
    InputSourceDataBufferHandle buffer_handle{0};
    std::size_t size_bytes{0};
    std::uint64_t sequence_number{0};
    TimestampPair received_timestamp{};

    std::optional<std::int64_t> presentation_timestamp;
    std::optional<std::int64_t> decoding_timestamp;
};

}
