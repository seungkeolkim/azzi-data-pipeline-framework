#pragma once

#include <deque>
#include <optional>
#include <utility>

// Bounded queue that stores handles/pointers. When full, the oldest item is dropped and
// returned to the caller so the caller can perform release + logging.
template <typename T>
class BoundedPointerQueue {
 public:
  explicit BoundedPointerQueue(size_t capacity) : capacity_(capacity) {}

  // Push returns an optional dropped item when the queue was already full.
  std::optional<T> Push(T item) {
    std::optional<T> dropped;
    if (queue_.size() >= capacity_) {
      dropped = std::move(queue_.front());
      queue_.pop_front();
    }
    queue_.push_back(std::move(item));
    return dropped;
  }

  bool Empty() const { return queue_.empty(); }

  T Pop() {
    T value = std::move(queue_.front());
    queue_.pop_front();
    return value;
  }

  size_t Size() const { return queue_.size(); }

 private:
  size_t capacity_;
  std::deque<T> queue_;
};
