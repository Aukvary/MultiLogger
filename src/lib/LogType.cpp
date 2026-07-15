#include <string_view>
#include "Journal.hpp"

using namespace MultiLogger;
using namespace std::string_view_literals;

std::string_view LogTypeToStringView(LogType type) {
    switch (type) {
        case LogType::Message: return "Message"sv;
        case LogType::Warning: return "Warning"sv;
        case LogType::Error: return "Error"sv;
    }
}