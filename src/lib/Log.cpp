#include <chrono>
#include <ctime>
#include <iostream>
#include <ostream>
#include "Journal.hpp"

using system_clock = std::chrono::system_clock;

namespace MultiLogger {
    LogType Log::Type() const {
        return _type;
    }

    std::ostream& operator<<(std::ostream& os, const Log& log) {
        auto time = system_clock::to_time_t(log._time);
        std::tm local_time{};
        localtime_r(&time, &local_time);

        char time_buf[20];
        std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", &local_time);

        return os << "[" << LogTypeToStringView(log._type) << "][" << time_buf << "] " << log._message;
    }
} // namespace MultiLogger
