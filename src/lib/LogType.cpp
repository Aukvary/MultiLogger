#include <ostream>
#include <string_view>
#include "Journal.hpp"

using namespace std::string_view_literals;

namespace MultiLogger {
    std::string_view LogTypeToString(LogType type) {
        switch (type) {
            case LogType::Message:
                return "Message"sv;
            case LogType::Warning:
                return "Warning"sv;
            case LogType::Error:
                return "Error"sv;
            default:
                return {};
        }
    }

    std::ostream& operator<<(std::ostream& os, LogType type) {
        return os << LogTypeToString(type);
    }

    std::optional<LogType> StringToLogType(std::string_view str) {
        if (str == "Message") {
            return MultiLogger::LogType::Message;
        } else if (str == "Warning") {
            return MultiLogger::LogType::Warning;
        } else if (str == "Error") {
            return MultiLogger::LogType::Error;
        }

        return std::nullopt;
    }
} // namespace MultiLogger
