#include <mutex>
#include <optional>
#include "Journal.hpp"

namespace MultiLogger {
    template<typename T>
    void SafeQueue<T>::push(T el) {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _queue.push(std::move(el));
        }
        _condVar.notify_one();
    }

    template<typename T>
    std::optional<T> SafeQueue<T>::pop() {
        std::unique_lock<std::mutex> lock(_mutex);

        _condVar.wait(lock, [this]() { return !this->_queue.empty() || this->_is_stopped; });

        if (_queue.empty() && _is_stopped)
            return std::nullopt;

        T el = std::move(_queue.front());
        _queue.pop();

        return el;
    }

    template<typename T>
    void SafeQueue<T>::stop() {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _is_stopped = true;
        }

        _condVar.notify_all();
    }
} // namespace MultiLogger
