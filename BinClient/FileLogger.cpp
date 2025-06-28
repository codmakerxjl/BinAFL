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
        logStream_.open(logFileName, std::ios::out | std::ios::trunc); // ÿ�����������Ǿ���־

        // ������ʱˢ�µĺ�̨�߳�
        stopFlusher_ = false;
        flusherThread_ = std::thread(&FileLogger::timedFlushLoop, this);
    }
}

void FileLogger::Shutdown() {
    // 1. ֪ͨ��̨�߳��˳�
    stopFlusher_ = true;
    // 2. �ȴ���̨�߳�����乤������ȫ�˳�
    if (flusherThread_.joinable()) {
        flusherThread_.join();
    }

    // 3. ִ�����һ�����յ�ˢ��
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

// �������������������Ϊ�����Ǳ��Ѿ��������ĺ�������
void FileLogger::flushToFile() {
    if (logStream_.is_open() && !logBuffer_.empty()) {
        for (const auto& entry : logBuffer_) {
            logStream_ << entry.second << std::endl;
        }
        logBuffer_.clear(); // ��ջ�����
    }
}

void FileLogger::timedFlushLoop() {
    while (!stopFlusher_) {
        // ʹ��С�����˯�������߳̿��Կ�����Ӧ�˳��ź�
        for (int i = 0; i < FLUSH_INTERVAL_SECONDS; ++i) {
            if (stopFlusher_) {
                return; // ����յ��˳��źţ���������
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        // �ﵽʱ������������ˢ�»�����
        {
            std::lock_guard<std::mutex> lock(mtx_);
            flushToFile();
        }
    }
}
