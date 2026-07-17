#include "Tests.hpp"

int main() {
    MyTest::RunTests(
        MyTest::TestCase(
            "test",
            []() {

            }),
        MyTest::TestCase("test", []() {

        }));

    return 0;
}
