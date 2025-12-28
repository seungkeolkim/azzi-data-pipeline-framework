#pragma once

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>

namespace stream_pipeline {

/*
 * Logger (Stage 0.5)
 * ------------------
 * - 최소한의 thread-safe logger.
 * - 과도한 프레임별 로그를 피하기 위해 노드 내부에서 샘플링/요약 시점에 호출한다.
 */
class Logger {
public:
    enum class Level { Info, Warning, Error, Debug };

    static Logger& instance();

    void set_min_level(Level level);
    void log(Level level, const std::string& message);
    void log_with_node(Level level, const std::string& node_name, const std::string& message);

private:
    Logger() = default;

    std::string now_string_() const;
    std::string level_to_string_(Level level) const;

    std::mutex mutex_;
    Level min_level_{Level::Info};
};

inline Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

inline void Logger::set_min_level(Level level) {
    std::lock_guard<std::mutex> lock(mutex_);
    min_level_ = level;
}

inline void Logger::log(Level level, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (level < min_level_) {
        return;
    }
    std::cout << now_string_() << " [" << level_to_string_(level) << "] " << message << std::endl;
}

inline void Logger::log_with_node(Level level, const std::string& node_name, const std::string& message) {
    std::ostringstream oss;
    oss << "[" << node_name << "] " << message;
    log(level, oss.str());
}

inline std::string Logger::now_string_() const {
    using namespace std::chrono;
    const auto now = system_clock::now();
    const auto time_t_now = system_clock::to_time_t(now);

    std::tm tm_now{};
    // localtime_r is POSIX; Stage 0 개발 컨테이너 가정.
    localtime_r(&time_t_now, &tm_now);

    std::ostringstream oss;
    oss << std::put_time(&tm_now, "%F %T");

    const auto ns_part = duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count() % 1'000'000'000LL;
    oss << "." << std::setw(9) << std::setfill('0') << ns_part;
    return oss.str();
}

inline std::string Logger::level_to_string_(Level level) const {
    switch (level) {
        case Level::Info:
            return "INFO";
        case Level::Warning:
            return "WARN";
        case Level::Error:
            return "ERROR";
        case Level::Debug:
            return "DEBUG";
        default:
            return "UNKNOWN";
    }
}

}  // namespace stream_pipeline
