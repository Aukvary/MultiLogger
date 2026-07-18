#pragma once
#include <exception>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

namespace MyTest {
    inline void Assert(bool condition, std::string_view msg = "Assertion failed") {
        if (!condition)
            throw std::runtime_error{std::string(msg)};
    }

    template<typename T, typename U>
    void AssertEq(const T& actual, const U& expected, std::string_view msg = "") {
        if (actual != expected) {
            std::ostringstream oss;
            if (!msg.empty())
                oss << msg << ": ";
            oss << "expected '" << expected << "', got '" << actual << "'";
            throw std::runtime_error{oss.str()};
        }
    }

    struct CerrCapture {
        std::stringstream buf;
        std::streambuf* old;
        CerrCapture() {
            old = std::cerr.rdbuf(buf.rdbuf());
        }
        ~CerrCapture() {
            std::cerr.rdbuf(old);
        }
        std::string str() const {
            return buf.str();
        }
    };

    template<typename T>
    int TestCase(std::string_view testName, T&& fn) {
        try {
            fn();
            std::cout << "PASS: " << testName << std::endl;
            return 0;
        } catch (const std::exception& err) {
            std::cerr << "FAIL: " << testName << ": " << err.what() << std::endl;
            return 1;
        }
    }

    template<typename... T>
    int RunTests(T&&... result) {
        int issues = (result + ...);
        std::cerr << "FAILS: " << issues << std::endl;
        return issues;
    }
} // namespace MyTest
