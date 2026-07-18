#pragma once
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <optional>
#include <queue>
#include <shared_mutex>
#include <stdexcept>
#include <string_view>
#include <thread>

namespace MultiLogger {
    enum class LogType : char { Message, Warning, Error };
    std::string_view LogTypeToStringView(LogType type);
    std::optional<LogType> StringToLogType(std::string_view str);
    std::ostream& operator<<(std::ostream& os, LogType type);

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

        LogType Type() const;
    };


    template<typename T>
    class SafeQueue {
    private:
        std::queue<T> _queue;
        std::mutex _mutex;
        std::condition_variable _condVar;
        bool _is_stopped = false;

    public:
        void push(const T& el) {
            {
                std::lock_guard<std::mutex> lock(_mutex);
                _queue.push(std::move(el));
            }
            _condVar.notify_one();
        }

        std::optional<T> pop() {
            std::unique_lock<std::mutex> lock(_mutex);

            _condVar.wait(lock, [this]() { return !this->_queue.empty() || this->_is_stopped; });

            if (_queue.empty() && _is_stopped)
                return std::nullopt;

            T el = std::move(_queue.front());
            _queue.pop();

            return el;
        }

        void stop() {
            {
                std::lock_guard<std::mutex> lock(_mutex);
                _is_stopped = true;
            }

            _condVar.notify_all();
        }
    };


    class FileWriter {
    private:
        using system_clock = std::chrono::system_clock;
        SafeQueue<MultiLogger::Log> _logQueue;

        mutable std::shared_mutex _writerMutex;
        std::string _fileName;
        std::ofstream _fileStream;
        LogType _defaultType;

        std::thread _writerThread;

        void PushLog(MultiLogger::Log log);

    public:
        FileWriter(std::optional<std::string_view> fileName, LogType defaultType) :
            _fileName{fileName.value_or("~undefined~")}, _defaultType{defaultType} {
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
                        _fileStream << log << '\n';
                    else
                        std::cerr << "[Error] File was has missed, log \"" << log
                                  << "\" was has lost" << std::endl;
                }
            }};
        }

        FileWriter(const FileWriter&) = delete;
        FileWriter& operator=(const FileWriter&) = delete;

        std::string File() const;
        void File(std::string_view fileName);

        LogType DefaultType() const;
        void DefaultType(LogType type);

        void Log(Log log);
        void Log(std::string_view message);
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
