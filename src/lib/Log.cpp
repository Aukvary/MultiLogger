#include "Journal.hpp"
#include <ostream>
#include <iostream>

namespace MultiLogger {
    LogType Log::Type() {
        return _type;
    }

    std::ostream& operator<<(std::ostream& os, const Log& log) {
        return os << "[" << LogTypeToStringView(log._type) << "] " << log._message;
    }
}