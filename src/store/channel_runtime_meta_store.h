#pragma once

#include <functional>
#include <mutex>
#include <unordered_map>

#include "metadata/channel_runtime_meta.h"

// 채널 단위 글로벌 메타데이터 store. mutex 기반으로 단순 구현한다.
class ChannelRuntimeMetaStore {
public:
    using ReadFn = std::function<void(const ChannelRuntimeMeta&)>;
    using WriteFn = std::function<void(ChannelRuntimeMeta&)>;

    void Read(const ChannelIdentifier& id, const ReadFn& fn) {
        std::lock_guard<std::mutex> lock(mutex_);
        fn(meta_map_[id]);
    }

    void Write(const ChannelIdentifier& id, const WriteFn& fn) {
        std::lock_guard<std::mutex> lock(mutex_);
        fn(meta_map_[id]);
    }

private:
    std::unordered_map<ChannelIdentifier, ChannelRuntimeMeta, ChannelIdentifierHash> meta_map_;
    std::mutex mutex_;
};
