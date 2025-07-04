#pragma once

#include <windows.h>
#include <string>
#include <stdexcept> // 用于抛出异常
#include "ipc_control.h"


// 共享数据结构
struct SharedData {
    int buff[256];
};

// 为共享内存和信号量定义唯一的字符串名称
const wchar_t* const SHM_NAME = L"BinAFLSharedMemory";
const wchar_t* const SEM_SERVER_READY_NAME = L"SemServerReady"; // 表示缓冲区空闲，可以写入
const wchar_t* const SEM_DATA_READY_NAME = L"SemDataReady";     // 表示数据已写入，可以读取

class SharedMemoryIPC {
public:
    // 定义角色，决定是创建资源还是打开现有资源
    enum class Role {
        SERVER,
        CLIENT
    };

    /**
     * @brief 构造函数，根据角色初始化共享内存和同步对象。
     * @param role 指定是作为服务端（创建者）还是客户端（访问者）。
     * @throws std::runtime_error 如果任何 WinAPI 调用失败。
     */
    SharedMemoryIPC(Role role);

    /**
     * @brief 析构函数，自动清理所有句柄和内存映射。
     */
    ~SharedMemoryIPC();

    // 禁止拷贝和赋值，因为该类管理着唯一的系统资源句柄
    SharedMemoryIPC(const SharedMemoryIPC&) = delete;
    SharedMemoryIPC& operator=(const SharedMemoryIPC&) = delete;

    /**
     * @brief (生产者) 等待缓冲区空闲，然后写入数据。
     * @param data 要写入共享内存的数据。
     * @param timeout 超时时间（毫秒），INFINITE 表示无限等待。
     * @return 如果成功写入则为 true，如果超时则为 false。
     * @throws std::runtime_error 如果等待或释放信号量失败。
     */
    bool write(const SharedData& data, DWORD timeout = INFINITE);

    /**
     * @brief (消费者) 等待新数据到达，然后读取数据。
     * @param data 用于接收从共享内存读取的数据的引用。
     * @param timeout 超时时间（毫秒），INFINITE 表示无限等待。
     * @return 如果成功读取到新数据则为 true，如果超时则为 false。
     * @throws std::runtime_error 如果等待或释放信号量失败。
     */
    bool read(SharedData& data, DWORD timeout = INFINITE);

    //对控制packet的读写重载
    bool write(const ControlPacket& packet, DWORD timeout = INFINITE);
    bool read(ControlPacket& packet, DWORD timeout = INFINITE);
private:
    void create_resources();
    void open_resources();

    Role role_;
    HANDLE hMapFile_ = NULL;
    HANDLE hSemServerReady_ = NULL;
    HANDLE hSemDataReady_ = NULL;
    SharedData* pSharedData_ = nullptr;
    ControlPacket* pControlPacket_ = nullptr;
};