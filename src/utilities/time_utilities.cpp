#include "stream_pipeline/utilities/time_utilities.hpp"

#include <chrono>

namespace stream_pipeline {

TimestampPair now_timestamp_pair() {
    TimestampPair result{};

    // Wall-clock: Unix epoch 기반 (system_clock)
    {
        const auto now = std::chrono::system_clock::now();
        const auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
        result.wall_clock_time_nanoseconds = static_cast<UnixTimeNanoseconds>(nanoseconds);
    }

    // Monotonic: latency 측정용 (steady_clock)
    {
        const auto now = std::chrono::steady_clock::now();
        const auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
        result.monotonic_time_nanoseconds = static_cast<MonotonicTimeNanoseconds>(nanoseconds);
    }

    return result;
}

}  // namespace stream_pipeline
