#include "pch.h"
#include "FileLogger.h"
#include <algorithm> // for std::remove_if

FileLogger& FileLogger::GetInstance() {
    static FileLogger instance;
    return instance;
}

void FileLogger::Init(const std::wstring& logFileName) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!logStream_.is_open()) {
        logStream_.open(logFileName, std::ios::out | std::ios::trunc); // 每次启动都覆盖旧日志

        // 启动定时刷新的后台线程
        stopFlusher_ = false;
        flusherThread_ = std::thread(&FileLogger::timedFlushLoop, this);
    }
}

void FileLogger::Shutdown() {
    // 1. 通知后台线程退出
    stopFlusher_ = true;
    // 2. 等待后台线程完成其工作并安全退出
    if (flusherThread_.joinable()) {
        flusherThread_.join();
    }

    // 3. 执行最后一次最终的刷新
    std::lock_guard<std::mutex> lock(mtx_);
    flushToFile();
    if (logStream_.is_open()) {
        logStream_.close();
    }
}

void FileLogger::Log(UINT uMsg, const wchar_t* format, ...) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!logStream_.is_open()) return;

    wchar_t buffer[1024];
    va_list args;
    va_start(args, format);
    vswprintf_s(buffer, _countof(buffer), format, args);
    va_end(args);

    logBuffer_.emplace_back(uMsg, buffer);

    if (logBuffer_.size() >= BUFFER_FLUSH_THRESHOLD) {
        flushToFile();
    }
}

void FileLogger::RemoveMessagesFromBuffer(UINT uMsgToRemove) {
    std::lock_guard<std::mutex> lock(mtx_);

    logBuffer_.erase(
        std::remove_if(logBuffer_.begin(), logBuffer_.end(),
            [uMsgToRemove](const auto& entry) {
                return entry.first == uMsgToRemove;
            }),
        logBuffer_.end()
    );
}

// 这个函数本身不加锁，因为它总是被已经持有锁的函数调用
void FileLogger::flushToFile() {
    if (logStream_.is_open() && !logBuffer_.empty()) {
        for (const auto& entry : logBuffer_) {
            logStream_ << entry.second << std::endl;
        }
        logBuffer_.clear(); // 清空缓冲区
    }
}

void FileLogger::timedFlushLoop() {
    while (!stopFlusher_) {
        // 使用小间隔的睡眠来让线程可以快速响应退出信号
        for (int i = 0; i < FLUSH_INTERVAL_SECONDS; ++i) {
            if (stopFlusher_) {
                return; // 如果收到退出信号，立即返回
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        // 达到时间间隔，锁定并刷新缓冲区
        {
            std::lock_guard<std::mutex> lock(mtx_);
            flushToFile();
        }
    }
}
