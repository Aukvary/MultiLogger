#include <shared_mutex>
#include <string>
#include "Journal.hpp"

namespace MultiLogger {
    FileWriter::FileWriter(std::optional<std::string_view> fileName, LogType defaultType) :
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
                    std::cerr << "[Error] File was has missed, log \"" << log << "\" was has lost"
                              << std::endl;
            }
        }};
    }

    std::string FileWriter::File() const {
        std::shared_lock<std::shared_mutex> lock(_writerMutex);
        return _fileName;
    }

    void FileWriter::File(std::string_view fileName) {
        std::unique_lock<std::shared_mutex> lock(_writerMutex);

        if (_fileStream.is_open()) {
            _fileStream.close();
        }

        _fileName = fileName;

        _fileStream.open(_fileName, std::ios::app);

        if (!_fileStream.is_open()) {
            throw std::runtime_error{"Failed to open new log file"};
        }
    }

    LogType FileWriter::DefaultType() const {
        std::shared_lock<std::shared_mutex> lock(_writerMutex);
        return _defaultType;
    }

    void FileWriter::DefaultType(LogType type) {
        std::unique_lock<std::shared_mutex> lock(_writerMutex);
        _defaultType = type;
    }

    void FileWriter::PushLog(MultiLogger::Log log) {
        if (log.Type() < _defaultType)
            return;
        _logQueue.push(std::move(log));
    }

    void FileWriter::Log(MultiLogger::Log log) {
        std::shared_lock<std::shared_mutex> lock(_writerMutex);
        PushLog(std::move(log));
    }

    void FileWriter::Log(std::string_view message) {
        std::shared_lock<std::shared_mutex> lock(_writerMutex);
        PushLog(MultiLogger::Log{message, _defaultType});
    }

    void FileWriter::Log(std::string_view message, LogType type) {
        std::shared_lock<std::shared_mutex> lock(_writerMutex);
        PushLog(MultiLogger::Log{message, type});
    }

    void FileWriter::Log(std::string_view message, system_clock::time_point time, LogType type) {
        std::shared_lock<std::shared_mutex> lock(_writerMutex);
        PushLog(MultiLogger::Log{message, time, type});
    }

    FileWriter::~FileWriter() {
        _logQueue.stop();
        if (_writerThread.joinable()) {
            _writerThread.join();
        }
    }
} // namespace MultiLogger
