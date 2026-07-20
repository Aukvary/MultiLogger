#include <cstdio>
#include <fcntl.h>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>
#include "Tests.hpp"
#include "lib/Journal.hpp"

static std::string UniquePath() {
    static int counter = 0;
    return "/tmp/multilogger_test_" + std::to_string(++counter) + ".log";
}

static const char* APP_BINARY = "./build/logger_app";

struct AppResult {
    int exitCode;
    std::string stdOut;
    std::string stdErr;
};

static std::string readFileStr(const std::string& path) {
    std::ifstream f(path);
    if (!f)
        return {};
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static AppResult runApp(const std::vector<std::string>& args, const std::string& stdinContent) {
    char stdinPattern[] = "/tmp/mlt_stdin_XXXXXX";
    char stdoutPattern[] = "/tmp/mlt_stdout_XXXXXX";
    char stderrPattern[] = "/tmp/mlt_stderr_XXXXXX";

    int stdinFd = mkstemp(stdinPattern);
    int stdoutFd = mkstemp(stdoutPattern);
    int stderrFd = mkstemp(stderrPattern);

    auto cleanup = [&]() {
        if (stdinFd != -1)
            close(stdinFd);
        if (stdoutFd != -1)
            close(stdoutFd);
        if (stderrFd != -1)
            close(stderrFd);
        unlink(stdinPattern);
        unlink(stdoutPattern);
        unlink(stderrPattern);
    };

    if (stdinFd == -1 || stdoutFd == -1 || stderrFd == -1) {
        cleanup();
        return {-1, "", "failed to create temp files"};
    }

    write(stdinFd, stdinContent.data(), stdinContent.size());
    lseek(stdinFd, 0, SEEK_SET);

    pid_t pid = fork();
    if (pid == -1) {
        cleanup();
        return {-1, "", "fork failed"};
    }

    if (pid == 0) {
        dup2(stdinFd, STDIN_FILENO);
        dup2(stdoutFd, STDOUT_FILENO);
        dup2(stderrFd, STDERR_FILENO);
        close(stdinFd);
        close(stdoutFd);
        close(stderrFd);

        std::vector<const char*> argv;
        argv.push_back(APP_BINARY);
        for (const auto& a: args)
            argv.push_back(a.c_str());
        argv.push_back(nullptr);

        execvp(argv[0], (char* const*)argv.data());
        _exit(127);
    }

    close(stdinFd);
    close(stdoutFd);
    close(stderrFd);

    int status;
    waitpid(pid, &status, 0);

    std::string stdOut = readFileStr(stdoutPattern);
    std::string stdErr = readFileStr(stderrPattern);

    unlink(stdinPattern);
    unlink(stdoutPattern);
    unlink(stderrPattern);

    int exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    return {exitCode, stdOut, stdErr};
}

int main() {
    MyTest::RunTests(
        MyTest::TestCase(
            "LogTypeToStringView_All",
            []() {
                MyTest::AssertEq(
                    MultiLogger::LogTypeToString(MultiLogger::LogType::Message),
                    std::string_view("Message"));
                MyTest::AssertEq(
                    MultiLogger::LogTypeToString(MultiLogger::LogType::Warning),
                    std::string_view("Warning"));
                MyTest::AssertEq(
                    MultiLogger::LogTypeToString(MultiLogger::LogType::Error),
                    std::string_view("Error"));
            }),

        MyTest::TestCase(
            "StringToLogType_Valid",
            []() {
                MyTest::AssertEq(
                    *MultiLogger::StringToLogType("Message"), MultiLogger::LogType::Message);
                MyTest::AssertEq(
                    *MultiLogger::StringToLogType("Warning"), MultiLogger::LogType::Warning);
                MyTest::AssertEq(
                    *MultiLogger::StringToLogType("Error"), MultiLogger::LogType::Error);
            }),

        MyTest::TestCase(
            "StringToLogType_Invalid",
            []() {
                auto r = MultiLogger::StringToLogType("Undefined");
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
                MyTest::CerrCapture capture;
                {
                    MultiLogger::FileWriter fw{"/fakeDir/bad.log", MultiLogger::LogType::Message};
                    fw.Log("cerr message");
                }
                MyTest::Assert(capture.str().find("cerr message") != std::string::npos);
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
            }),
        MyTest::TestCase(
            "Startup_Defaults",
            []() {
                auto res = runApp({}, "exit\n");
                MyTest::AssertEq(res.exitCode, 0);
                MyTest::Assert(res.stdOut.find("~undefined~:(Message)> ") != std::string::npos);
            }),

        MyTest::TestCase(
            "Startup_File",
            []() {
                auto logPath = UniquePath();
                auto res = runApp({"-f", logPath}, "exit\n");
                MyTest::AssertEq(res.exitCode, 0);
                MyTest::Assert(res.stdOut.find(logPath) != std::string::npos);
                std::remove(logPath.c_str());
            }),

        MyTest::TestCase(
            "Startup_Dt",
            []() {
                auto res = runApp({"-dt", "Error"}, "exit\n");
                MyTest::AssertEq(res.exitCode, 0);
                MyTest::Assert(res.stdOut.find("~undefined~:(Error)> ") != std::string::npos);
            }),

        MyTest::TestCase(
            "Startup_FileAndDt",
            []() {
                auto logPath = UniquePath();
                auto res = runApp({"-f", logPath, "-dt", "Warning"}, "exit\n");
                MyTest::AssertEq(res.exitCode, 0);
                MyTest::Assert(res.stdOut.find(logPath) != std::string::npos);
                MyTest::Assert(res.stdOut.find("(Warning)> ") != std::string::npos);
                std::remove(logPath.c_str());
            }),

        MyTest::TestCase(
            "Startup_MissingFileArg",
            []() {
                auto res = runApp({"-f"}, "exit\n");
                MyTest::AssertEq(res.exitCode, 0);
                MyTest::Assert(res.stdErr.find("lost arg -f") != std::string::npos);
            }),

        MyTest::TestCase(
            "Startup_MissingDtArg",
            []() {
                auto res = runApp({"-dt"}, "exit\n");
                MyTest::AssertEq(res.exitCode, 0);
                MyTest::Assert(res.stdErr.find("lost arg -dt") != std::string::npos);
            }),

        MyTest::TestCase(
            "Startup_InvalidType",
            []() {
                auto res = runApp({"-dt", "Bogus"}, "exit\n");
                MyTest::AssertEq(res.exitCode, 0);
                MyTest::Assert(res.stdErr.find("Undefined typeBogus") != std::string::npos);
            }),

        MyTest::TestCase(
            "Startup_BadFilePath",
            []() {
                auto res = runApp({"-f", "/fakeDir/bad.log"}, "exit\n");
                MyTest::AssertEq(res.exitCode, 0);
                MyTest::Assert(res.stdErr.find("Failed to open log file") != std::string::npos);
                MyTest::Assert(res.stdOut.find("~undefined~") != std::string::npos);
            }),

        MyTest::TestCase(
            "Interactive_Help",
            []() {
                auto res = runApp({}, "-h\nexit\n");
                MyTest::AssertEq(res.exitCode, 0);
                MyTest::Assert(res.stdOut.find("Options & Commands") != std::string::npos);
            }),

        MyTest::TestCase(
            "Interactive_ListTypes",
            []() {
                auto res = runApp({}, "-tl\nexit\n");
                MyTest::AssertEq(res.exitCode, 0);
                MyTest::Assert(res.stdOut.find("Message, Warning, Error") != std::string::npos);
            }),

        MyTest::TestCase(
            "Interactive_ChangeFile",
            []() {
                auto logPath1 = UniquePath();
                auto logPath2 = UniquePath();
                auto input = "-f " + logPath2 + "\nexit\n";
                auto res = runApp({"-f", logPath1}, input);
                MyTest::AssertEq(res.exitCode, 0);
                MyTest::Assert(res.stdOut.find(logPath2) != std::string::npos);
                std::remove(logPath1.c_str());
                std::remove(logPath2.c_str());
            }),

        MyTest::TestCase(
            "Interactive_ChangeType",
            []() {
                auto res = runApp({}, "-dt Warning\nexit\n");
                MyTest::AssertEq(res.exitCode, 0);
                MyTest::Assert(res.stdOut.find("(Warning)> ") != std::string::npos);
            }),

        MyTest::TestCase(
            "Interactive_InvalidType",
            []() {
                auto res = runApp({}, "-dt Bogus\nexit\n");
                MyTest::AssertEq(res.exitCode, 0);
                MyTest::Assert(res.stdErr.find("Undefined typeBogus") != std::string::npos);
            }),

        MyTest::TestCase(
            "Interactive_MissingFileArg",
            []() {
                auto res = runApp({}, "-f\nexit\n");
                MyTest::AssertEq(res.exitCode, 0);
                MyTest::Assert(res.stdErr.find("-f requires a file path") != std::string::npos);
            }),

        MyTest::TestCase(
            "Interactive_MissingTypeArg",
            []() {
                auto res = runApp({}, "-dt\nexit\n");
                MyTest::AssertEq(res.exitCode, 0);
                MyTest::Assert(res.stdErr.find("-dt requires a log type") != std::string::npos);
            }),

        MyTest::TestCase(
            "Interactive_LogWithType",
            []() {
                auto logPath = UniquePath();
                auto input = "-l Error log_msg_123\nexit\n";
                auto res = runApp({"-f", logPath}, input);
                MyTest::AssertEq(res.exitCode, 0);

                std::ifstream f(logPath);
                std::string line;
                bool found = false;
                while (std::getline(f, line)) {
                    if (line.find("log_msg_123") != std::string::npos) {
                        found = true;
                        MyTest::Assert(line.find("[Error]") != std::string::npos);
                    }
                }
                MyTest::Assert(found);
                std::remove(logPath.c_str());
            }),

        MyTest::TestCase(
            "Interactive_LogWithoutType",
            []() {
                auto logPath = UniquePath();
                auto input = "-l plain_msg_456\nexit\n";
                auto res = runApp({"-f", logPath, "-dt", "Warning"}, input);
                MyTest::AssertEq(res.exitCode, 0);

                std::ifstream f(logPath);
                std::string line;
                bool found = false;
                while (std::getline(f, line)) {
                    if (line.find("plain_msg_456") != std::string::npos) {
                        found = true;
                        MyTest::Assert(line.find("[Warning]") != std::string::npos);
                    }
                }
                MyTest::Assert(found);
                std::remove(logPath.c_str());
            }),

        MyTest::TestCase(
            "Interactive_LogFiltered",
            []() {
                auto logPath = UniquePath();
                auto input = "-l Warning filtered_low\nexit\n";
                auto res = runApp({"-f", logPath, "-dt", "Error"}, input);
                MyTest::AssertEq(res.exitCode, 0);

                std::ifstream f(logPath);
                std::string line;
                bool found = false;
                while (std::getline(f, line)) {
                    if (line.find("filtered_low") != std::string::npos)
                        found = true;
                }
                MyTest::Assert(!found);
                std::remove(logPath.c_str());
            }),

        MyTest::TestCase(
            "Interactive_LogAfterChangeFile",
            []() {
                auto logPath1 = UniquePath();
                auto logPath2 = UniquePath();
                auto input = "-f " + logPath2 + "\n-l Error after_change\nexit\n";
                auto res = runApp({"-f", logPath1}, input);
                MyTest::AssertEq(res.exitCode, 0);
                MyTest::Assert(res.stdOut.find(logPath2) != std::string::npos);

                std::ifstream f2(logPath2);
                std::string line;
                bool foundAfter = false;
                while (std::getline(f2, line)) {
                    if (line.find("after_change") != std::string::npos)
                        foundAfter = true;
                }
                MyTest::Assert(foundAfter);

                std::ifstream f1(logPath1);
                bool foundBefore = false;
                while (std::getline(f1, line)) {
                    if (line.find("after_change") != std::string::npos)
                        foundBefore = true;
                }
                MyTest::Assert(!foundBefore);

                std::remove(logPath1.c_str());
                std::remove(logPath2.c_str());
            }),

        MyTest::TestCase(
            "Interactive_LogMissingArg",
            []() {
                auto res = runApp({}, "-l\nexit\n");
                MyTest::AssertEq(res.exitCode, 0);
                MyTest::Assert(res.stdErr.find("-l requires a message") != std::string::npos);
            }),

        MyTest::TestCase(
            "Interactive_Exit",
            []() {
                auto res = runApp({}, "exit\n");
                MyTest::AssertEq(res.exitCode, 0);
            }),

        MyTest::TestCase(
            "Interactive_EmptyLine",
            []() {
                auto res = runApp({}, "\nexit\n");
                MyTest::AssertEq(res.exitCode, 0);
            }),

        MyTest::TestCase(
            "Interactive_UnknownCommand",
            []() {
                auto res = runApp({}, "foobar\nexit\n");
                MyTest::AssertEq(res.exitCode, 0);
                MyTest::Assert(res.stdErr.find("undefined command") != std::string::npos);
            })

    );
    return 0;
}
