#pragma once
#include "SharedMemoryIPC.h" // 假设此文件已在项目中

/**
 * @class CommandController
 * @brief 通过一个已建立的IPC通道，向客户端发送控制命令。
 */
class CommandController {
public:
    /**
     * @brief 构造函数。
     * @param ipc_channel 一个已经初始化好的 SharedMemoryIPC 通道引用。
     */
    explicit CommandController(SharedMemoryIPC& ipc_channel);

    /**
     * @brief 发送命令，通知客户端开始消息截取和记录。
     */
    void StartLogging();

    /**
     * @brief 发送命令，通知客户端停止消息截取和记录。
     */
    void StopLogging();

    /**
     * @brief 发送命令，通知客户端的控制线程可以安全退出了。
     */
    void SendTerminate();

private:
    SharedMemoryIPC& channel_; // 对通信通道的引用
};
