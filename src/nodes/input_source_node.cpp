#include "stream_pipeline/nodes/input_source_node.hpp"

#include "stream_pipeline/utilities/logger.hpp"

#include <exception>

namespace stream_pipeline {

InputSourceNode::InputSourceNode(InputSourceMode input_source_mode)
    : input_source_mode_(input_source_mode) {}

InputSourceNode::~InputSourceNode() {
    stop();
}

InputSourceNode::InputSourceMode InputSourceNode::input_source_mode() const {
    return input_source_mode_;
}

InputSourceNode::InputSourceState InputSourceNode::input_source_state() const {
    return input_source_state_.load();
}

bool InputSourceNode::push_from_external(const std::uint8_t* data_pointer, std::size_t size_bytes) {
    if (input_source_mode_ != InputSourceMode::Push) {
        return false;
    }

    std::lock_guard<std::mutex> lock(external_push_mutex_);
    try {
        return handle_external_push_(data_pointer, size_bytes);
    } catch (const std::exception&) {
        set_input_source_state_(InputSourceState::Error);
        Logger::instance().log_with_node(Logger::Level::Error, node_name(), "external push exception");
        return false;
    } catch (...) {
        set_input_source_state_(InputSourceState::Error);
        Logger::instance().log_with_node(Logger::Level::Error, node_name(), "external push unknown exception");
        return false;
    }
}

NodeInterface::StartResult InputSourceNode::start() {
    if (running_.load()) {
        return StartResult::AlreadyRunning;
    }

    stop_requested_.store(false);
    running_.store(true);
    set_input_source_state_(InputSourceState::Running);

    Logger::instance().log_with_node(Logger::Level::Info, node_name(), "start requested");

    worker_thread_ = std::thread(&InputSourceNode::thread_entry_wrapper_, this);
    return StartResult::Success;
}

void InputSourceNode::stop() {
    if (!running_.load()) {
        return;
    }

    stop_requested_.store(true);

    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }

    running_.store(false);
    if (input_source_state_.load() != InputSourceState::Error) {
        set_input_source_state_(InputSourceState::Stopped);
    }

    Logger::instance().log_with_node(Logger::Level::Info, node_name(), "stopped");
}

void InputSourceNode::set_input_source_state_(InputSourceState new_state) {
    input_source_state_.store(new_state);
}

bool InputSourceNode::stop_requested() const {
    return stop_requested_.load();
}

void InputSourceNode::thread_entry_wrapper_() {
    try {
        thread_entry_();
    } catch (const std::exception&) {
        set_input_source_state_(InputSourceState::Error);
        Logger::instance().log_with_node(Logger::Level::Error, node_name(), "thread exception");
    } catch (...) {
        set_input_source_state_(InputSourceState::Error);
        Logger::instance().log_with_node(Logger::Level::Error, node_name(), "thread unknown exception");
    }

    if (input_source_state_.load() != InputSourceState::Error) {
        set_input_source_state_(InputSourceState::Stopped);
    }
    running_.store(false);
}

}
