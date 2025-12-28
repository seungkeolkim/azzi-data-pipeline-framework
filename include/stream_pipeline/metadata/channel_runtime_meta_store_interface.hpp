#pragma once

#include "stream_pipeline/common/identifiers.hpp"
#include "stream_pipeline/metadata/channel_runtime_meta.hpp"

#include <functional>

namespace stream_pipeline {

/*
 * ChannelRuntimeMetaStoreInterface
 * --------------------------------
 * - ChannelRuntimeMeta를 채널 단위로 저장/조회한다.
 * - Stage 0.5에서는 mutex 기반 구현(SimpleChannelRuntimeMetaStore)을 제공한다.
 * - 값 복사 대신 reference/closure 접근을 강제하여 lock 범위를 store가 통제한다.
 */
class ChannelRuntimeMetaStoreInterface {
public:
    using ReadCallback = std::function<void(const ChannelRuntimeMeta&)>;
    using WriteCallback = std::function<void(ChannelRuntimeMeta&)>;

    virtual ~ChannelRuntimeMetaStoreInterface() = default;

    virtual void read(ChannelIdentifier channel_identifier, const ReadCallback& callback) const = 0;
    virtual void write(ChannelIdentifier channel_identifier, const WriteCallback& callback) = 0;
};

}  // namespace stream_pipeline
