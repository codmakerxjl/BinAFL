#pragma once
#include <windows.h>
#include <string>
#include <fstream>
#include <mutex>
#include <vector>
#include <cstdarg>
#include <thread>   // 用于后台线程
#include <atomic>   // 用于线程安全的退出标志
#include <chrono>   // 用于时间操作

class FileLogger {
public:
    static FileLogger& GetInstance();
    FileLogger(const FileLogger&) = delete;
    FileLogger& operator=(const FileLogger&) = delete;

    void Init(const std::wstring& logFileName);
    void Shutdown();

    /**
     * @brief 将一条日志消息添加到内存缓冲区中。
     * @param uMsg 消息ID，用于后续可能的过滤。
     * @param format 格式化字符串。
     * @param ... 参数。
     */
    void Log(UINT uMsg, const wchar_t* format, ...);

    /**
     * @brief 从内存缓冲区中移除所有指定ID的消息。
     * @param uMsg 要移除的消息ID。
     */
    void RemoveMessagesFromBuffer(UINT uMsg);

private:
    FileLogger() = default;
    ~FileLogger() = default;

    /**
     * @brief 将内存缓冲区中的所有内容写入到磁盘文件中。
     */
    void flushToFile();

    /**
     * @brief 后台线程的主循环函数，负责定时刷新。
     */
    void timedFlushLoop();

    // 缓冲区的最大容量，达到此容量时会自动刷入磁盘
    const size_t BUFFER_FLUSH_THRESHOLD = 500;
    // 定时刷新的时间间隔（秒）
    const int FLUSH_INTERVAL_SECONDS = 10;

    std::wofstream logStream_;
    std::mutex mtx_;

    // 内存缓冲区，存储消息ID和格式化后的字符串
    std::vector<std::pair<UINT, std::wstring>> logBuffer_;

    // 用于定时刷新的后台线程和退出标志
    std::thread flusherThread_;
    std::atomic<bool> stopFlusher_{ false };
};
