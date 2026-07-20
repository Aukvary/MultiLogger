#pragma once
#include <exception>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

namespace MyTest {
    /** @brief Проверяет условие; при нарушении бросает исключение.
     *  @details Используется внутри TestCase. Исключение ловится и выводится как FAIL.
     *  @param condition Условие, которое должно быть истинным.
     *  @param msg Сообщение об ошибке (по умолчанию "Assertion failed"). */
    inline void Assert(bool condition, std::string_view msg = "Assertion failed") {
        if (!condition)
            throw std::runtime_error{std::string(msg)};
    }

    /** @brief Сравнивает фактическое значение с ожидаемым.
     *  @details При несовпадении формирует сообщение с ожидаемым и фактическим значениями.
     *  @tparam T Тип фактического значения.
     *  @tparam U Тип ожидаемого значения.
     *  @param actual Фактическое значение.
     *  @param expected Ожидаемое значение.
     *  @param msg Дополнительный контекст ошибки (опционально). */
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

    /** @brief Перехватывает вывод std::cerr в строковый буфер.
     *  @details В конструкторе подменяет rdbuf std::cerr на внутренний stringstream.
     *           В деструкторе восстанавливает исходный буфер. */
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

    /** @brief Запускает тестовую функцию с обработкой исключений.
     *  @details Выводит "PASS: имя" при успехе или "FAIL: имя: причина" при ошибке.
     *  @tparam T Тип callable-объекта.
     *  @param testName Имя теста для вывода.
     *  @param fn Тестовая функция.
     *  @return 0 при успехе, 1 при ошибке. */
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

    /** @brief Запускает набор тестов и суммирует количество ошибок.
     *  @details Использует fold-выражение (C++17) для подсчёта суммы переданных результатов.
     *  @tparam T... Типы результатов (должны приводиться к int).
     *  @param result Результаты выполнения TestCase.
     *  @return Общее количество упавших тестов. */
    template<typename... T>
    int RunTests(T&&... result) {
        int issues = (result + ...);
        std::cerr << "FAILS: " << issues << std::endl;
        return issues;
    }
} // namespace MyTest
