#pragma once

#include "stream_pipeline/runtime/node_interface.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <thread>

namespace stream_pipeline {

/*
 * 입력 소스 기반 노드
 * --------------------
 * - 다양한 입력 방식을 수용하는 입력 소스 계열의 기본 노드다.
 *
 * 중요한 설계 전제:
 * - 외부 네트워크 경계에서 들어오는 푸시를 전제로 설계한다.
 * - 푸시 모드는 외부 경계에서 호출될 수 있다.
 * - 0.6 단계에서는 구현하지 않지만 반드시 이 구조를 유지해야 한다.
 * - 한 달 뒤의 나는 남이다. 절대 구조를 합치지 말 것.
 */
class InputSourceNode : public NodeInterface {
public:
    enum class InputSourceMode : std::uint8_t {
        Pull = 0,
        Push = 1
    };

    enum class InputSourceState : std::uint8_t {
        Idle = 0,
        Running,
        Error,
        Disconnected,
        Stopped
    };

    explicit InputSourceNode(InputSourceMode input_source_mode);
    ~InputSourceNode() override;

    InputSourceMode input_source_mode() const;
    InputSourceState input_source_state() const;

    /*
     * 외부 푸시 진입점
     * ---------------
     * - 푸시 모드에서 외부 경계가 호출할 수 있는 진입점이다.
     * - 스레드 안전성을 보장해야 한다.
     */
    bool push_from_external(const std::uint8_t* data_pointer, std::size_t size_bytes);

    StartResult start() override;
    void stop() override;

protected:
    virtual void thread_entry_() = 0;
    virtual bool handle_external_push_(const std::uint8_t* data_pointer, std::size_t size_bytes) = 0;

    void set_input_source_state_(InputSourceState new_state);
    bool stop_requested() const;

private:
    void thread_entry_wrapper_();

    InputSourceMode input_source_mode_{InputSourceMode::Pull};
    std::atomic<InputSourceState> input_source_state_{InputSourceState::Idle};

    std::atomic<bool> running_{false};
    std::atomic<bool> stop_requested_{false};
    std::thread worker_thread_;

    std::mutex external_push_mutex_;
};

}
