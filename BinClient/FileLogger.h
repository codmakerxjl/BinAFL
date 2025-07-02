#pragma once
#include <windows.h>
#include <string>
#include <fstream>
#include <mutex>
#include <vector>
#include <cstdarg>
#include <thread>   // ���ں�̨�߳�
#include <atomic>   // �����̰߳�ȫ���˳���־
#include <chrono>   // ����ʱ�����

class FileLogger {
public:
    static FileLogger& GetInstance();
    FileLogger(const FileLogger&) = delete;
    FileLogger& operator=(const FileLogger&) = delete;

    void Init(const std::wstring& logFileName);
    void Shutdown();

    /**
     * @brief ��һ����־��Ϣ��ӵ��ڴ滺�����С�
     * @param uMsg ��ϢID�����ں������ܵĹ��ˡ�
     * @param format ��ʽ���ַ�����
     * @param ... ������
     */
    void Log(UINT uMsg, const wchar_t* format, ...);

    /**
     * @brief ���ڴ滺�������Ƴ�����ָ��ID����Ϣ��
     * @param uMsg Ҫ�Ƴ�����ϢID��
     */
    void RemoveMessagesFromBuffer(UINT uMsg);

private:
    FileLogger() = default;
    ~FileLogger() = default;

    /**
     * @brief ���ڴ滺�����е���������д�뵽�����ļ��С�
     */
    void flushToFile();

    /**
     * @brief ��̨�̵߳���ѭ������������ʱˢ�¡�
     */
    void timedFlushLoop();

    // ������������������ﵽ������ʱ���Զ�ˢ�����
    const size_t BUFFER_FLUSH_THRESHOLD = 500;
    // ��ʱˢ�µ�ʱ�������룩
    const int FLUSH_INTERVAL_SECONDS = 10;

    std::wofstream logStream_;
    std::mutex mtx_;

    // �ڴ滺�������洢��ϢID�͸�ʽ������ַ���
    std::vector<std::pair<UINT, std::wstring>> logBuffer_;

    // ���ڶ�ʱˢ�µĺ�̨�̺߳��˳���־
    std::thread flusherThread_;
    std::atomic<bool> stopFlusher_{ false };
};
