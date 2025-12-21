#pragma once

#include <cstddef>
#include <cstdint>
#include <condition_variable>
#include <deque>
#include <mutex>

namespace stream_pipeline {

/*
 * BoundedPointerQueue
 * -------------------
 * - “포인터 전용” bounded queue.
 * - 큐는 포인터가 가리키는 객체를 소유하지 않는다.
 *
 * Stage 0 합의(매우 중요):
 * - overflow 시 DropOldestItem (실시간 처리에서 의미 없는 오래된 프레임을 제거)
 * - drop이 발생하면 push()가 dropped_old_pointer를 반환하여 호출자가 즉시 store로 반환 가능해야 한다.
 */

enum class QueueOverflowPolicy : std::uint8_t {
    DropOldestItem,
    DropNewestItem,
    BlockWhenFull
};

template <typename PointerType>
class BoundedPointerQueue {
public:
    explicit BoundedPointerQueue(std::size_t capacity, QueueOverflowPolicy overflow_policy)
        : capacity_(capacity), overflow_policy_(overflow_policy) {}

    /*
     * PushResult
     * ----------
     * - enqueue 연산에 특화된 결과 코드.
     */
    enum class PushResult : std::uint8_t {
        Success = 0,
        DroppedNewItem,
        QueueClosed
    };

    /*
     * PushOutcome
     * -----------
     * - DropOldestItem 정책일 때 dropped_old_pointer를 통해 drop된 항목을 반환한다.
     * - 호출자는 dropped_old_pointer가 nullptr이 아니면 즉시 “원 소유자(store)”에 반환해야 한다.
     */
    struct PushOutcome {
        PushResult result{PushResult::QueueClosed};
        PointerType dropped_old_pointer{nullptr};
    };

    PushOutcome push(PointerType pointer_to_enqueue) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (closed_) {
            return {PushResult::QueueClosed, nullptr};
        }

        // BlockWhenFull: 용량이 찼으면 공간이 생길 때까지 대기.
        if (overflow_policy_ == QueueOverflowPolicy::BlockWhenFull) {
            not_full_condition_.wait(lock, [&] { return closed_ || queue_.size() < capacity_; });
            if (closed_) {
                return {PushResult::QueueClosed, nullptr};
            }
            queue_.push_back(pointer_to_enqueue);
            not_empty_condition_.notify_one();
            return {PushResult::Success, nullptr};
        }

        // Drop policies: 용량 초과 시 drop 처리
        if (queue_.size() >= capacity_) {
            if (overflow_policy_ == QueueOverflowPolicy::DropNewestItem) {
                // 새 항목을 drop. 호출자는 pointer_to_enqueue를 store로 반환해야 한다.
                return {PushResult::DroppedNewItem, nullptr};
            }

            // DropOldestItem:
            // - 가장 오래된 항목을 제거하고 새 항목을 삽입한다.
            // - 제거된 항목 포인터를 호출자에게 반환한다(해제 책임 이동).
            PointerType dropped_old_pointer = queue_.front();
            queue_.pop_front();
            queue_.push_back(pointer_to_enqueue);
            not_empty_condition_.notify_one();
            return {PushResult::Success, dropped_old_pointer};
        }

        queue_.push_back(pointer_to_enqueue);
        not_empty_condition_.notify_one();
        return {PushResult::Success, nullptr};
    }

    /*
     * PopResult
     * ---------
     * - dequeue 연산에 특화된 결과 코드.
     */
    enum class PopResult : std::uint8_t {
        Success = 0,
        QueueClosedAndEmpty
    };

    /*
     * PopOutcome
     * ----------
     * - dequeued_pointer: 꺼낸 포인터
     */
    struct PopOutcome {
        PopResult result{PopResult::QueueClosedAndEmpty};
        PointerType dequeued_pointer{nullptr};
    };

    PopOutcome pop_blocking() {
        std::unique_lock<std::mutex> lock(mutex_);
        not_empty_condition_.wait(lock, [&] { return closed_ || !queue_.empty(); });

        if (queue_.empty()) {
            return {PopResult::QueueClosedAndEmpty, nullptr};
        }

        PointerType dequeued_pointer = queue_.front();
        queue_.pop_front();
        not_full_condition_.notify_one();
        return {PopResult::Success, dequeued_pointer};
    }

    /*
     * close()
     * -------
     * - 대기 중인 스레드를 모두 깨우고 종료 상태로 만든다.
     */
    void close() {
        std::lock_guard<std::mutex> lock(mutex_);
        closed_ = true;
        not_empty_condition_.notify_all();
        not_full_condition_.notify_all();
    }

private:
    std::size_t capacity_{0};
    QueueOverflowPolicy overflow_policy_{QueueOverflowPolicy::DropOldestItem};

    std::mutex mutex_;
    std::condition_variable not_empty_condition_;
    std::condition_variable not_full_condition_;

    std::deque<PointerType> queue_;
    bool closed_{false};
};

}  // namespace stream_pipeline
