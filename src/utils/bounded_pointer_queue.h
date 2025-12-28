#pragma once

#include <deque>
#include <optional>

// 고정 크기 포인터(또는 핸들) 큐. 가득 찼을 때 가장 오래된 항목을 drop하고
// 호출자에게 어떤 항목이 제거되었는지 반환한다.
template <typename T>
class BoundedPointerQueue {
public:
    explicit BoundedPointerQueue(size_t capacity) : capacity_(capacity) {}

    // 새 항목을 추가한다. 가득 찼다면 drop된 항목을 반환한다.
    std::optional<T> Push(const T& item) {
        std::optional<T> dropped;
        if (queue_.size() >= capacity_) {
            dropped = queue_.front();
            queue_.pop_front();
        }
        queue_.push_back(item);
        return dropped;
    }

    // 가장 오래된 항목을 가져온다. 없으면 std::nullopt 반환.
    std::optional<T> Pop() {
        if (queue_.empty()) {
            return std::nullopt;
        }
        T item = queue_.front();
        queue_.pop_front();
        return item;
    }

    size_t Size() const { return queue_.size(); }

private:
    std::deque<T> queue_;
    size_t capacity_;
};
