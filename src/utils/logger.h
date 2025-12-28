#pragma once

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>

// 간단한 콘솔 로거. Stage 0.5에서는 프레임마다 과한 로그를 피하기 위해
// 노드 시작/종료, 드롭, 오류 등 핵심 이벤트만 기록한다.
class Logger {
public:
    enum class Level {
        kInfo,
        kWarn,
        kError,
    };

    // 스레드 안전한 단일톤 로거.
    static Logger& Instance() {
        static Logger instance;
        return instance;
    }

    void Log(Level level, const std::string& message) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::ostringstream oss;
        oss << TimeStampString() << " [" << LevelToString(level) << "] " << message;
        std::cout << oss.str() << std::endl;
    }

private:
    Logger() = default;

    std::string TimeStampString() {
        using Clock = std::chrono::system_clock;
        const auto now = Clock::now();
        const std::time_t now_time = Clock::to_time_t(now);
        std::tm tm_buf{};
#if defined(_WIN32)
        localtime_s(&tm_buf, &now_time);
#else
        localtime_r(&now_time, &tm_buf);
#endif
        std::ostringstream oss;
        oss << std::put_time(&tm_buf, "%F %T");
        return oss.str();
    }

    const char* LevelToString(Level level) {
        switch (level) {
            case Level::kInfo:
                return "INFO";
            case Level::kWarn:
                return "WARN";
            case Level::kError:
                return "ERROR";
            default:
                return "INFO";
        }
    }

    std::mutex mutex_;
};

// 편의 매크로. 주석을 한글로 두어 Stage 0.5 가이드라인을 따른다.
#define LOG_INFO(msg) Logger::Instance().Log(Logger::Level::kInfo, (msg))
#define LOG_WARN(msg) Logger::Instance().Log(Logger::Level::kWarn, (msg))
#define LOG_ERROR(msg) Logger::Instance().Log(Logger::Level::kError, (msg))
