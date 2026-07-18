#include <cstdio>
#include <fstream>
#include <sstream>
#include <thread>
#include <vector>
#include "Tests.hpp"
#include "lib/Journal.hpp"

static std::string UniquePath() {
    static int counter = 0;
    return "/tmp/multilogger_test_" + std::to_string(++counter) + ".log";
}

int main() {
    MyTest::RunTests(
        MyTest::TestCase(
            "LogTypeToStringView_All",
            []() {
                MyTest::AssertEq(
                    MultiLogger::LogTypeToStringView(MultiLogger::LogType::Message),
                    std::string_view("Message"));
                MyTest::AssertEq(
                    MultiLogger::LogTypeToStringView(MultiLogger::LogType::Warning),
                    std::string_view("Warning"));
                MyTest::AssertEq(
                    MultiLogger::LogTypeToStringView(MultiLogger::LogType::Error),
                    std::string_view("Error"));
            }),

        MyTest::TestCase(
            "StringToLogType_Valid",
            []() {
                MyTest::AssertEq(*MultiLogger::StringToLogType("Message"),
                                 MultiLogger::LogType::Message);
                MyTest::AssertEq(*MultiLogger::StringToLogType("Warning"),
                                 MultiLogger::LogType::Warning);
                MyTest::AssertEq(*MultiLogger::StringToLogType("Error"),
                                 MultiLogger::LogType::Error);
            }),

        MyTest::TestCase(
            "StringToLogType_Invalid",
            []() {
                auto r = MultiLogger::StringToLogType("BogusType");
                MyTest::Assert(!r);
            }),

        MyTest::TestCase(
            "LogType_Comparison",
            []() {
                MyTest::Assert(MultiLogger::LogType::Message < MultiLogger::LogType::Warning);
                MyTest::Assert(MultiLogger::LogType::Warning < MultiLogger::LogType::Error);
                MyTest::Assert(MultiLogger::LogType::Message < MultiLogger::LogType::Error);
            }),

        MyTest::TestCase(
            "Log_Constructors",
            []() {
                MultiLogger::Log log1{"test"};
                MyTest::Assert(log1.Type() == MultiLogger::LogType::Message);

                MultiLogger::Log log2{"warn", MultiLogger::LogType::Warning};
                MyTest::Assert(log2.Type() == MultiLogger::LogType::Warning);

                auto tp = std::chrono::system_clock::now();
                MultiLogger::Log log3{"err", tp, MultiLogger::LogType::Error};
                MyTest::Assert(log3.Type() == MultiLogger::LogType::Error);
            }),

        MyTest::TestCase(
            "Log_OutputFormat",
            []() {
                namespace chr = std::chrono;
                using Clock = chr::system_clock;

                Clock::time_point epoch{chr::seconds{0}};
                MultiLogger::Log log{"test", epoch, MultiLogger::LogType::Warning};

                std::ostringstream oss;
                oss << log;
                std::string s = oss.str();

                std::time_t t = Clock::to_time_t(epoch);
                std::tm local{};
                ::localtime_r(&t, &local);
                char buf[20];
                std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &local);

                std::string expected = std::string{"[Warning]["} + buf + "] test";
                MyTest::AssertEq(s, expected);
            }),

        MyTest::TestCase(
            "SafeQueue_PushPop",
            []() {
                MultiLogger::SafeQueue<int> q;
                q.push(67);
                auto v = q.pop();
                MyTest::Assert(!!v);
                MyTest::AssertEq(*v, 67);
            }),

        MyTest::TestCase(
            "SafeQueue_Concurrent",
            []() {
                MultiLogger::SafeQueue<int> q;
                constexpr int P = 4;
                constexpr int N = 1000;
                std::vector<std::thread> producers;
                for (int i = 0; i < P; i++) {
                    producers.emplace_back([&q, i]() {
                        for (int j = 0; j < N; j++)
                            q.push(i * N + j);
                    });
                }
                for (auto& t: producers)
                    t.join();

                q.stop();

                int count = 0;
                while (auto v = q.pop()) {
                    count++;
                }
                MyTest::AssertEq(count, P * N);
            }),

        MyTest::TestCase(
            "SafeQueue_Stop",
            []() {
                MultiLogger::SafeQueue<int> q;
                q.stop();
                MyTest::Assert(!q.pop());
            }),

        MyTest::TestCase(
            "FileWriter_LogToFile",
            []() {
                auto path = UniquePath();
                {
                    MultiLogger::FileWriter fw{path, MultiLogger::LogType::Message};
                    fw.Log("hello world");
                }
                std::ifstream f(path);
                std::string line;
                MyTest::Assert(!!std::getline(f, line));
                MyTest::Assert(line.find("hello world") != std::string::npos);
                std::remove(path.c_str());
            }),

        MyTest::TestCase(
            "FileWriter_LogToCerr",
            []() {
                MyTest::CerrCapture capture;
                {
                    MultiLogger::FileWriter fw{std::nullopt, MultiLogger::LogType::Message};
                    fw.Log("cerr message");
                }
                auto out = capture.str();
                MyTest::Assert(out.find("cerr message") != std::string::npos);
            }),

        MyTest::TestCase(
            "FileWriter_InitBadFile",
            []() {
                bool threw = false;
                try {
                    MultiLogger::FileWriter fw{
                        "/fakeDir/bad.log", MultiLogger::LogType::Message};
                } catch (const std::runtime_error&) {
                    threw = true;
                }
                MyTest::Assert(threw);
            }),

        MyTest::TestCase(
            "FileWriter_FilterByLevel",
            []() {
                auto path = UniquePath();
                {
                    MultiLogger::FileWriter fw{path, MultiLogger::LogType::Error};
                    fw.Log("msg", MultiLogger::LogType::Message);
                    fw.Log("warn", MultiLogger::LogType::Warning);
                    fw.Log("err", MultiLogger::LogType::Error);
                }
                std::ifstream f(path);
                std::string line;
                int count = 0;
                bool hasError = false;
                while (std::getline(f, line)) {
                    count++;
                    if (line.find("err") != std::string::npos)
                        hasError = true;
                    MyTest::Assert(line.find("msg") == std::string::npos);
                    MyTest::Assert(line.find("warn") == std::string::npos);
                }
                MyTest::AssertEq(count, 1);
                MyTest::Assert(hasError);
                std::remove(path.c_str());
            }),

        MyTest::TestCase(
            "FileWriter_ChangeDefaultType",
            []() {
                auto path = UniquePath();
                {
                    MultiLogger::FileWriter fw{path, MultiLogger::LogType::Error};
                    fw.DefaultType(MultiLogger::LogType::Message);
                    fw.Log("now_allowed", MultiLogger::LogType::Message);
                }
                std::ifstream f(path);
                std::string line;
                bool found = false;
                while (std::getline(f, line)) {
                    if (line.find("now_allowed") != std::string::npos)
                        found = true;
                }
                MyTest::Assert(found);
                std::remove(path.c_str());
            }),

        MyTest::TestCase(
            "FileWriter_ChangeFile",
            []() {
                auto path1 = UniquePath();
                auto path2 = UniquePath();
                {
                    MultiLogger::FileWriter fw{path1, MultiLogger::LogType::Message};
                    fw.Log("to_first");
                    fw.File(path2);
                    fw.Log("to_second");
                }
                {
                    std::ifstream f1(path1);
                    std::string l1;
                    MyTest::Assert(!!std::getline(f1, l1));
                    MyTest::Assert(l1.find("to_first") != std::string::npos);
                }
                {
                    std::ifstream f2(path2);
                    std::string l2;
                    MyTest::Assert(!!std::getline(f2, l2));
                    MyTest::Assert(l2.find("to_second") != std::string::npos);
                }
                std::remove(path1.c_str());
                std::remove(path2.c_str());
            }),

        MyTest::TestCase(
            "FileWriter_LogWithExplicitTime",
            []() {
                auto path = UniquePath();
                {
                    MultiLogger::FileWriter fw{path, MultiLogger::LogType::Message};
                    auto tp = std::chrono::system_clock::now();
                    fw.Log("timed", tp, MultiLogger::LogType::Warning);
                }
                std::ifstream f(path);
                std::string line;
                MyTest::Assert(!!std::getline(f, line));
                MyTest::Assert(line.find("timed") != std::string::npos);
                std::remove(path.c_str());
            }),

        MyTest::TestCase(
            "FileWriter_Multithreaded",
            []() {
                auto path = UniquePath();
                constexpr int N = 20;
                {
                    MultiLogger::FileWriter fw{path, MultiLogger::LogType::Message};
                    std::vector<std::thread> threads;
                    for (int i = 0; i < N; i++) {
                        threads.emplace_back([&fw, i]() { fw.Log("thread_" + std::to_string(i)); });
                    }
                    for (auto& t: threads)
                        t.join();
                }
                std::ifstream f(path);
                std::string line;
                int count = 0;
                while (std::getline(f, line))
                    count++;
                MyTest::AssertEq(count, N);
                std::remove(path.c_str());
            })

    );
    return 0;
}
