#pragma once

#include "stream_pipeline/metadata/channel_runtime_meta_store_interface.hpp"

#include <mutex>
#include <unordered_map>

namespace stream_pipeline {

/*
 * SimpleChannelRuntimeMetaStore
 * ------------------------------
 * - Stage 0.5용 mutex 기반 ChannelRuntimeMeta store.
 * - channel_identifier를 key로 하고, 없는 경우 default 값을 생성한다.
 */
class SimpleChannelRuntimeMetaStore final : public ChannelRuntimeMetaStoreInterface {
public:
    SimpleChannelRuntimeMetaStore() = default;
    ~SimpleChannelRuntimeMetaStore() override = default;

    void read(ChannelIdentifier channel_identifier, const ReadCallback& callback) const override;
    void write(ChannelIdentifier channel_identifier, const WriteCallback& callback) override;

private:
    ChannelRuntimeMeta& get_or_create_unlocked_(ChannelIdentifier channel_identifier) const;

    mutable std::mutex mutex_;
    mutable std::unordered_map<ChannelIdentifier, ChannelRuntimeMeta> meta_by_channel_;
};

}  // namespace stream_pipeline
