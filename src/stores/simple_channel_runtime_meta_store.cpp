#include "stream_pipeline/stores/simple_channel_runtime_meta_store.hpp"

namespace stream_pipeline {

void SimpleChannelRuntimeMetaStore::read(ChannelIdentifier channel_identifier, const ReadCallback& callback) const {
    if (!callback) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    auto iterator = meta_by_channel_.find(channel_identifier);
    if (iterator == meta_by_channel_.end()) {
        // read는 절대 생성하지 않는다.
        return;
    }

    const ChannelRuntimeMeta& meta = iterator->second;
    callback(meta);
}


void SimpleChannelRuntimeMetaStore::write(ChannelIdentifier channel_identifier, const WriteCallback& callback) {
    if (!callback) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    ChannelRuntimeMeta& meta = get_or_create_unlocked_(channel_identifier);
    callback(meta);
}

ChannelRuntimeMeta& SimpleChannelRuntimeMetaStore::get_or_create_unlocked_(
    ChannelIdentifier channel_identifier) const {
    auto iterator = meta_by_channel_.find(channel_identifier);
    if (iterator == meta_by_channel_.end()) {
        auto [inserted_iterator, _] = meta_by_channel_.emplace(channel_identifier, ChannelRuntimeMeta{});
        return inserted_iterator->second;
    }
    return iterator->second;
}

}  // namespace stream_pipeline
