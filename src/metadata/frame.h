#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

// Stage 0에서 이미 확립된 계약: Store가 유일 소유자가 되고, 노드는 핸들만 보유한다.
// ChannelIdentifier는 stream/source를 나타내며 새 타입을 만들지 말라는 요구를 따른다.
struct ChannelIdentifier {
    std::string name;

    bool operator==(const ChannelIdentifier& other) const { return name == other.name; }
};

struct ChannelIdentifierHash {
    std::size_t operator()(const ChannelIdentifier& id) const noexcept {
        return std::hash<std::string>{}(id.name);
    }
};

// FrameMetadata는 wall/monotonic timestamp를 모두 유지하여 후속 stage의 지연 추적을 돕는다.
struct TimestampPair {
    int64_t wall_clock_ns;
    int64_t monotonic_clock_ns;
};

struct FrameMetadata {
    uint64_t frame_id;
    ChannelIdentifier channel;
    TimestampPair timestamps;
};

// 실제 payload는 dummy/synthetic이지만 buffer 소유권 역시 store에 둔다.
struct FrameBuffer {
    std::string payload;
};

// 간단한 객체 메타데이터. Stage 0.5에서는 타입만 구분한다.
struct ObjectMetadata {
    std::string object_type;
};

// FrameHandle은 store 내부 인덱스를 가리키는 비소유 핸들이다.
struct FrameHandle {
    size_t storage_index;
};

// FrameStore는 frame → buffer → object release 체인을 유지하도록 설계한다.
class FrameStore {
public:
    FrameHandle CreateFrame(const FrameMetadata& metadata,
                            const FrameBuffer& buffer,
                            const std::vector<ObjectMetadata>& objects) {
        storage_.push_back(FrameStorage{metadata, buffer, objects});
        return FrameHandle{storage_.size() - 1};
    }

    const FrameMetadata& GetMetadata(const FrameHandle& handle) const { return storage_.at(handle.storage_index).metadata; }

    const FrameBuffer& GetBuffer(const FrameHandle& handle) const { return storage_.at(handle.storage_index).buffer; }

    const std::vector<ObjectMetadata>& GetObjects(const FrameHandle& handle) const {
        return storage_.at(handle.storage_index).objects;
    }

    // release chain: 객체 → 버퍼 → 프레임 순서로 clear하여 정합성을 보장한다.
    void ReleaseObjects(const FrameHandle& handle) { storage_.at(handle.storage_index).objects.clear(); }

    void ReleaseBuffer(const FrameHandle& handle) { storage_.at(handle.storage_index).buffer.payload.clear(); }

    void ReleaseFrame(const FrameHandle& handle) {
        ReleaseObjects(handle);
        ReleaseBuffer(handle);
        storage_.at(handle.storage_index).metadata = FrameMetadata{};
    }

private:
    struct FrameStorage {
        FrameMetadata metadata;
        FrameBuffer buffer;
        std::vector<ObjectMetadata> objects;
    };

    std::vector<FrameStorage> storage_;
};
