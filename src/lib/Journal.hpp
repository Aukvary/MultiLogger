#pragma once
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <optional>
#include <ostream>
#include <queue>
#include <shared_mutex>
#include <stdexcept>
#include <string_view>
#include <thread>

namespace MultiLogger {
    enum class LogType : char { Message, Warning, Error };
    std::string_view LogTypeToStringView(LogType type);

    struct Log {
    private:
        using system_clock = std::chrono::system_clock;

        std::string _message;
        system_clock::time_point _time;
        LogType _type;

    public:
        Log(std::string_view message, system_clock::time_point time, LogType type) :
            _message{message}, _time{time}, _type{type} {
        }

        Log(std::string_view message, LogType type) :
            _message{message}, _time{system_clock::now()}, _type{type} {
        }

        Log(std::string_view message) :
            _message{message}, _time{system_clock::now()}, _type{LogType::Message} {
        }

        friend std::ostream& operator<<(std::ostream& os, const Log& log);

        LogType Type();
    };


    template<typename T>
    class SafeQueue {
    private:
        std::queue<T> _queue;
        std::mutex _mutex;
        std::condition_variable _condVar;
        bool _is_stopped = false;

    public:
        void push(T el);
        std::optional<T> pop();
        void stop();
    };


    class FileWriter {
    private:
        using system_clock = std::chrono::system_clock;
        SafeQueue<Log> _logQueue;

        std::shared_mutex _writerMutex;
        std::string _fileName;
        std::ofstream _fileStream;
        LogType _defaultType;

        std::thread _writerThread;

    public:
        FileWriter(std::optional<std::string_view> fileName, LogType defaultType) :
            _fileName{fileName ? std::string(*fileName) : "~undefined~"},
            _defaultType{defaultType} {
            if (fileName != std::nullopt) {
                _fileStream.open(std::string(*fileName), std::ios::app);
                if (!_fileStream.is_open())
                    throw std::runtime_error{"Failed to open log file"};
            }

            _writerThread = std::thread{[this]() {
                while (auto logOpt = _logQueue.pop()) {
                    MultiLogger::Log log = std::move(*logOpt);
                    std::shared_lock<std::shared_mutex> lock(_writerMutex);
                    if (_fileStream.is_open())
                        _fileStream << log << std::endl;
                }
            }};
        }

        FileWriter(const FileWriter&) = delete;
        FileWriter& operator=(const FileWriter&) = delete;

        std::string File();
        void File(std::string_view fileName);

        LogType DefaultType();
        void DefaultType(LogType type);

        void Log(Log log);
        void Log(std::string_view message, LogType type);
        void Log(std::string_view message, system_clock::time_point time, LogType type);

        ~FileWriter() {
            _logQueue.stop();
            if (_writerThread.joinable()) {
                _writerThread.join();
            }
        }
    };
}; // namespace MultiLogger
