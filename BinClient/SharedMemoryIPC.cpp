#include "SharedMemoryIPC.h"
#include <system_error> // 用于更详细的错误信息
#include "pch.h"
// 辅助函数，用于抛出带 WinAPI 错误码的异常
void throw_windows_error(const std::string& message) {
    throw std::runtime_error(
        message + " (Error code: " + std::to_string(GetLastError()) + ")"
    );
}

SharedMemoryIPC::SharedMemoryIPC(Role role) : role_(role) {
    if (role_ == Role::SERVER) {
        create_resources();
    }
    else {
        open_resources();
    }

    // 将共享内存映射到进程地址空间 (服务端和客户端都需要)
    pSharedData_ = static_cast<SharedData*>(MapViewOfFile(
        hMapFile_,
        FILE_MAP_ALL_ACCESS,
        0,
        0,
        sizeof(SharedData)
    ));
    pControlPacket_ = static_cast<ControlPacket*>(MapViewOfFile(
        hMapFile_,
        FILE_MAP_ALL_ACCESS,
        0,
        0,
        sizeof(ControlPacket)
    ));
    if (pSharedData_ == nullptr) {
        throw_windows_error("pSharedData_MapViewOfFile failed");
    }
    if (pControlPacket_ == nullptr) {
        throw_windows_error("pControlPacket_MapViewOfFile failed");
    }
}

SharedMemoryIPC::~SharedMemoryIPC() {
    if (pSharedData_ != nullptr) {
        UnmapViewOfFile(pSharedData_);
    }
    if (hMapFile_ != NULL) {
        CloseHandle(hMapFile_);
    }
    if (hSemDataReady_ != NULL) {
        CloseHandle(hSemDataReady_);
    }
    if (hSemServerReady_ != NULL) {
        CloseHandle(hSemServerReady_);
    }
}

void SharedMemoryIPC::create_resources() {
    // 创建信号量:
    // SemServerReady: 初始值为1，最大值为1。表示缓冲区初始时是空闲的。
    hSemServerReady_ = CreateSemaphoreW(NULL, 1, 1, SEM_SERVER_READY_NAME);
    if (hSemServerReady_ == NULL) {
        throw_windows_error("CreateSemaphoreW for SemServerReady failed");
    }

    // SemDataReady: 初始值为0，最大值为1。表示初始时没有数据。
    hSemDataReady_ = CreateSemaphoreW(NULL, 0, 1, SEM_DATA_READY_NAME);
    if (hSemDataReady_ == NULL) {
        throw_windows_error("CreateSemaphoreW for SemDataReady failed");
    }

    // 创建文件映射对象
    hMapFile_ = CreateFileMappingW(
        INVALID_HANDLE_VALUE,
        NULL,
        PAGE_READWRITE,
        0,
        sizeof(SharedData),
        SHM_NAME
    );

    if (hMapFile_ == NULL) {
        throw_windows_error("CreateFileMappingW failed");
    }
}

void SharedMemoryIPC::open_resources() {
    // 打开已存在的信号量
    hSemServerReady_ = OpenSemaphoreW(SEMAPHORE_ALL_ACCESS, FALSE, SEM_SERVER_READY_NAME);
    if (hSemServerReady_ == NULL) {
        throw_windows_error("OpenSemaphoreW for SemServerReady failed. Is the server running?");
    }

    hSemDataReady_ = OpenSemaphoreW(SEMAPHORE_ALL_ACCESS, FALSE, SEM_DATA_READY_NAME);
    if (hSemDataReady_ == NULL) {
        throw_windows_error("OpenSemaphoreW for SemDataReady failed. Is the server running?");
    }

    // 打开已存在的文件映射对象
    hMapFile_ = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, SHM_NAME);
    if (hMapFile_ == NULL) {
        throw_windows_error("OpenFileMappingW failed. Is the server running?");
    }
}

bool SharedMemoryIPC::write(const SharedData& data, DWORD timeout) {
    // 1. 等待缓冲区空闲 (等待 SemServerReady 信号量)
    DWORD waitResult = WaitForSingleObject(hSemServerReady_, timeout);
    if (waitResult != WAIT_OBJECT_0) {
        if (waitResult == WAIT_TIMEOUT) return false; // 超时
        throw_windows_error("WaitForSingleObject on SemServerReady failed in write()");
    }

    // 2. 写入数据
    memcpy(pSharedData_, &data, sizeof(SharedData));

    // 3. 通知消费者数据已准备好 (释放 SemDataReady 信号量)
    if (!ReleaseSemaphore(hSemDataReady_, 1, NULL)) {
        throw_windows_error("ReleaseSemaphore on SemDataReady failed in write()");
    }

    return true;
}
bool SharedMemoryIPC::write(const ControlPacket& data, DWORD timeout) {
    // 1. 等待缓冲区空闲 (等待 SemServerReady 信号量)
    DWORD waitResult = WaitForSingleObject(hSemServerReady_, timeout);
    if (waitResult != WAIT_OBJECT_0) {
        if (waitResult == WAIT_TIMEOUT) return false; // 超时
        throw_windows_error("WaitForSingleObject on SemServerReady failed in write()");
    }

    // 2. 写入数据
    memcpy(pControlPacket_, &data, sizeof(ControlPacket));

    // 3. 通知消费者数据已准备好 (释放 SemDataReady 信号量)
    if (!ReleaseSemaphore(hSemDataReady_, 1, NULL)) {
        throw_windows_error("ReleaseSemaphore on SemDataReady failed in write()");
    }

    return true;
}

bool SharedMemoryIPC::read(SharedData& data, DWORD timeout) {
    // 1. 等待数据就绪 (等待 SemDataReady 信号量)
    DWORD waitResult = WaitForSingleObject(hSemDataReady_, timeout);
    if (waitResult != WAIT_OBJECT_0) {
        if (waitResult == WAIT_TIMEOUT) return false; // 超时
        throw_windows_error("WaitForSingleObject on SemDataReady failed in read()");
    }

    // 2. 读取数据
    memcpy(&data, pSharedData_, sizeof(SharedData));

    // 3. 通知生产者缓冲区已空闲 (释放 SemServerReady 信号量)
    if (!ReleaseSemaphore(hSemServerReady_, 1, NULL)) {
        throw_windows_error("ReleaseSemaphore on SemServerReady failed in read()");
    }

    return true;
}

bool SharedMemoryIPC::read(ControlPacket& data, DWORD timeout) {
    // 1. 等待数据就绪 (等待 SemDataReady 信号量)
    DWORD waitResult = WaitForSingleObject(hSemDataReady_, timeout);
    if (waitResult != WAIT_OBJECT_0) {
        if (waitResult == WAIT_TIMEOUT) return false; // 超时
        throw_windows_error("WaitForSingleObject on SemDataReady failed in read()");
    }

    // 2. 读取数据
    memcpy(&data, pControlPacket_, sizeof(ControlPacket));

    // 3. 通知生产者缓冲区已空闲 (释放 SemServerReady 信号量)
    if (!ReleaseSemaphore(hSemServerReady_, 1, NULL)) {
        throw_windows_error("ReleaseSemaphore on SemServerReady failed in read()");
    }

    return true;
}