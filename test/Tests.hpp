#pragma once
#include <exception>
#include <iostream>
#include <string_view>

namespace MyTest {
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
