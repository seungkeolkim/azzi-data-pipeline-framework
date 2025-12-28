#pragma once

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>

// Simple thread-safe logger to provide observability for Stage 0.5.
class Logger {
 public:
  enum class Level { kInfo, kWarn, kError };

  static Logger& Instance() {
    static Logger instance;
    return instance;
  }

  void Log(Level level, const std::string& component, const std::string& message) {
    const auto now = std::chrono::system_clock::now();
    const auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buffer{};
#if defined(_WIN32)
    localtime_s(&tm_buffer, &time_t_now);
#else
    localtime_r(&time_t_now, &tm_buffer);
#endif
    std::stringstream prefix;
    prefix << std::put_time(&tm_buffer, "%Y-%m-%d %H:%M:%S") << " [" << ToString(level)
           << "] [" << component << "] ";

    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << prefix.str() << message << std::endl;
  }

 private:
  std::string ToString(Level level) {
    switch (level) {
      case Level::kInfo:
        return "INFO";
      case Level::kWarn:
        return "WARN";
      case Level::kError:
        return "ERROR";
    }
    return "UNKNOWN";
  }

  std::mutex mutex_;
};
