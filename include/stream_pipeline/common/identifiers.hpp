#pragma once

#include <cstdint>

namespace stream_pipeline {

/*
 * Identifiers (Strong Typing by Alias)
 * -----------------------------------
 * 목적:
 * - Channel / Frame / Object ID를 같은 정수 타입으로 섞어 쓰는 실수를 원천적으로 줄인다.
 * - "한 달 뒤의 나는 남" 원칙에 따라 타입 자체가 문서가 되도록 한다.
 *
 * 주의:
 * - typedef/using은 강한 타입은 아니지만, 최소한 의미 구분과 검색성을 높인다.
 * - 더 강한 타입이 필요해지면 struct wrapper로 교체 가능(외부 계약 유지 고려).
 */

using ChannelIdentifier = std::uint32_t;
using FrameIdentifier   = std::uint64_t;
using ObjectIdentifier  = std::uint64_t;

}  // namespace stream_pipeline
