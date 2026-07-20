#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <mutex>
#include <optional>
#include <queue>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace MultiLogger {
    /** @brief Типы уровней важности сообщений журнала.
     *  @details Значения упорядочены: Message (0) < Warning (1) < Error (2).*/
    enum class LogType : char { Message, Warning, Error };

    /** @brief Преобразует LogType в строковое представление.
     *  @details При неизвестном типе возвращает пустой string_view.
     *  @param type Уровень важности сообщения.
     *  @return std::string_view — "Message", "Warning", "Error" или пустая строка. */
    std::string_view LogTypeToString(LogType type);

    /** @brief Преобразует строковое представление в LogType.
     *  @details При несовпадении ни с одним известным именем возвращает std::nullopt.
     *  @param str Строка с названием уровня важности.
     *  @return std::optional<LogType> — значение типа или nullopt при неверном имени. */
    std::optional<LogType> StringToLogType(std::string_view str);

    /** @brief Выводит строковое представление LogType в поток.
     *  @details Делегирует вызов LogTypeToStringView.
     *  @param os Выходной поток.
     *  @param type Уровень важности сообщения.
     *  @return std::ostream& — ссылка на выходной поток. */
    std::ostream& operator<<(std::ostream& os, LogType type);

    /** @brief Сообщение журнала: текст, временная метка, уровень важности.*/
    struct Log {
    private:
        using system_clock = std::chrono::system_clock;

        std::string _message;
        system_clock::time_point _time;
        LogType _type;
        bool _isFlushMarker = false;

        Log() : _type{LogType::Error}, _isFlushMarker{true} {
        }

    public:
        /** @brief Конструктор копирования (по умолчанию).
         *  @param log Исходное сообщение для копирования. */
        Log(const Log& log) = default;

        /** @brief Конструктор перемещения (по умолчанию).
         *  @param log Исходное сообщение для перемещения. */
        Log(Log&& log) = default;

        /** @brief Создаёт сообщение с заданным текстом, временем и уровнем важности.
         *  @param message Текст сообщения.
         *  @param time Временная метка сообщения.
         *  @param type Уровень важности сообщения. */
        Log(std::string_view message, system_clock::time_point time, LogType type) :
            _message{message}, _time{time}, _type{type} {
        }

        /** @brief Создаёт сообщение с заданным текстом и уровнем важности (время = now()).
         *  @param message Текст сообщения.
         *  @param type Уровень важности сообщения. */
        Log(std::string_view message, LogType type) :
            _message{message}, _time{system_clock::now()}, _type{type} {
        }

        /** @brief Создаёт сообщение только с текстом (уровень = Message, время = now()).
         *  @param message Текст сообщения. */
        Log(std::string_view message) :
            _message{message}, _time{system_clock::now()}, _type{LogType::Message} {
        }

        /** @brief Проверяет, является ли сообщение маркером принудительного сброса.
         *  @details Маркер сброса не записывается в файл, а используется для
         *           синхронизации: после его обработки поток-писатель сигнализирует
         *           вызывающему потоку, что все предыдущие сообщения записаны.
         *  @return true, если объект является маркером сброса. */
        bool IsFlushMarker() const;

        /** @brief Возвращает уровень важности сообщения.
         *  @return LogType — Message, Warning или Error. */
        LogType Type() const;

        /** @brief Создаёт маркер принудительного сброса очереди.
         *  @details Вызывает приватный конструктор Log() с _isFlushMarker = true.
         *           Используется в FileWriter::File() для синхронизации при смене файла.
         *  @return Log — объект-маркер сброса. */
        static Log CreateFlushMarker();

        /** @brief Дружественная функция: форматирует Log в выходной поток.
         *  @details Формат: "[Type][YYYY-MM-DD HH:MM:SS] message".
         *           Время преобразуется через localtime_r.
         *  @param os Выходной поток.
         *  @param log Сообщение журнала.
         *  @return std::ostream& — ссылка на выходной поток. */
        friend std::ostream& operator<<(std::ostream& os, const Log& log);
    };


    /** @brief Потокобезопасная очередь с блокирующим извлечением.
     *  @details Внутренняя синхронизация через std::mutex и std::condition_variable.
     *           push() уведомляет один ожидающий поток. pop() блокируется до появления
     *           элемента или остановки очереди. После stop() все ожидающие потоки
     *           получают std::nullopt.
     *  @tparam T Тип хранимых элементов (должен быть перемещаемым). */
    template<typename T>
    class SafeQueue {
    private:
        std::queue<T> _queue;
        std::mutex _mutex;
        std::condition_variable _condVar;
        bool _is_stopped = false;

    public:
        /** @brief Добавляет элемент в очередь.
         *  @details Захватывает мьютекс на время вставки, затем уведомляет один
         *           поток, ожидающий в pop(). Принимает любое значение через
         *           forwarding reference и перемещает его в очередь.
         *  @tparam U Тип элемента (выводится компилятором).
         *  @param el Элемент для добавления (перемещается в очередь). */
        template<typename U>
        void push(U&& el) {
            {
                std::lock_guard<std::mutex> lock(_mutex);
                _queue.push(std::forward<U>(el));
            }
            _condVar.notify_one();
        }

        /** @brief Извлекает элемент с блокировкой до появления данных.
         *  @details Ожидает на condition_variable, пока очередь не станет непустой
         *           или не будет вызван stop(). При остановленной пустой очереди
         *           возвращает std::nullopt.
         *  @return std::optional<T> — элемент очереди или nullopt после stop(). */
        std::optional<T> pop() {
            std::unique_lock<std::mutex> lock(_mutex);

            _condVar.wait(lock, [this]() { return !_queue.empty() || _is_stopped; });

            if (_queue.empty() && _is_stopped)
                return std::nullopt;

            T el = std::move(_queue.front());
            _queue.pop();

            return el;
        }

        /** @brief Останавливает очередь, разблокируя все ожидающие pop().
         *  @details Устанавливает флаг _is_stopped = true и уведомляет все потоки
         *           через notify_all. Последующие вызовы pop() сразу возвращают nullopt. */
        void stop() {
            {
                std::lock_guard<std::mutex> lock(_mutex);
                _is_stopped = true;
            }

            _condVar.notify_all();
        }
    };


    /** @brief Асинхронный писатель журнала в файл с фильтрацией по уровню важности.
     *  @details Запись выполняется в отдельном фоновом потоке. Сообщения передаются
     *           через SafeQueue — потокобезопасную очередь. Фильтрация по уровню
     *           важности: сообщения с уровнем ниже DefaultType() отбрасываются.
     *           При потере файлового потока сообщения выводятся в std::cerr.
     *           Формат: "[Error] Log file has been lost, log <message> could not be written".
     *           Конструктор запускает поток; деструктор останавливает очередь и
     *           дожидается завершения потока через join(). Копирование запрещено. */
    class FileWriter {
    private:
        using system_clock = std::chrono::system_clock;
        SafeQueue<MultiLogger::Log> _logQueue;

        mutable std::shared_mutex _writerMutex;
        mutable std::mutex _flushMutex;
        std::condition_variable _flushCv;
        bool _flushed = false;
        std::atomic<bool> _fileChanging{false};

        std::string _fileName;
        std::ofstream _fileStream;
        LogType _defaultType;

        std::thread _writerThread;

        /** @brief Фильтрует сообщение по уровню важности и помещает в очередь.
         *  @details Если уровень сообщения ниже _defaultType, сообщение отбрасывается.
         *  @param log Сообщение для записи. */
        void PushLog(MultiLogger::Log log);

    public:
        /** @brief Инициализирует писатель и запускает поток записи.
         *  @details Если fileName задан, открывает файл в режиме append.
     *           При ошибке открытия выводит ошибку в std::cerr; если файл не открыт,
     *           каждое сообщение пишется в std::cerr в формате
     *           "[Error] Log file has been lost, log <message> could not be written".
     *           Если fileName = nullopt — то же поведение (вывод в std::cerr).
     *           После открытия файла запускает _writerThread, который циклически
         *           извлекает логи из очереди и записывает их.
         *  @param fileName Путь к файлу журнала (или nullopt).
         *  @param defaultType Уровень важности по умолчанию. */
        FileWriter(
            std::optional<std::string_view> fileName, LogType defaultType = LogType::Message);

        FileWriter(const FileWriter&) = delete;
        FileWriter& operator=(const FileWriter&) = delete;

        /** @brief Возвращает текущее имя файла журнала.
         *  @details Потокобезопасно через shared_lock.
         *  @return std::string — имя файла. */
        std::string File() const;

        /** @brief Меняет файл журнала.
         *  @details Сначала вставляет в очередь маркер сброса (CreateFlushMarker())
         *           и ожидает, пока поток-писатель его обработает — это гарантирует,
         *           что все предыдущие сообщения записаны. Затем закрывает старый
         *           файл и открывает новый в режиме append. При ошибке открытия
         *           бросает std::runtime_error.
         *  @param fileName Новый путь к файлу журнала. */
        void File(std::string_view fileName);

        /** @brief Возвращает текущий уровень важности по умолчанию.
         *  @details Потокобезопасно через shared_lock.
         *  @return LogType — текущий уровень. */
        LogType DefaultType() const;

        /** @brief Устанавливает уровень важности по умолчанию.
         *  @details Потокобезопасно через unique_lock. Сообщения с уровнем ниже
         *           нового порога будут отбрасываться.
         *  @param type Новый уровень по умолчанию. */
        void DefaultType(LogType type);

        /** @brief Добавляет готовый объект Log в очередь на запись.
         *  @details Захватывает shared_lock, затем вызывает PushLog() для фильтрации.
         *  @param log Сообщение журнала. */
        void Log(Log log);

        /** @brief Добавляет текстовое сообщение в очередь на запись (уровень по умолчанию).
         *  @details Захватывает shared_lock, создаёт Log с _defaultType, вызывает PushLog().
         *  @param message Текст сообщения. */
        void Log(std::string_view message);

        /** @brief Добавляет текстовое сообщение с указанным уровнем в очередь на запись.
         *  @details Захватывает shared_lock, вызывает PushLog() для фильтрации.
         *  @param message Текст сообщения.
         *  @param type Уровень важности сообщения. */
        void Log(std::string_view message, LogType type);

        /** @brief Добавляет текстовое сообщение с временем и уровнем в очередь на запись.
         *  @details Захватывает shared_lock, вызывает PushLog() для фильтрации.
         *  @param message Текст сообщения.
         *  @param time Временная метка сообщения.
         *  @param type Уровень важности сообщения. */
        void Log(std::string_view message, system_clock::time_point time, LogType type);

        /** @brief Останавливает очередь и завершает поток записи.
         *  @details Вызывает _logQueue.stop() и join() на _writerThread.
         *           Гарантирует, что все сообщения из очереди будут обработаны. */
        ~FileWriter();
    };
}; // namespace MultiLogger
