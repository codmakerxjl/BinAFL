// pch.cpp: 与预编译标头对应的源文件

#include "pch.h"
#include "hijacker.h"
bool Initialize() {
		//初始化ipc  
	SharedMemoryIPC ipc(SharedMemoryIPC::Role::CLIENT);

	return 0;
}

/**
 * @brief 客户端控制线程的主循环。
 * 负责监听并响应来自服务端的命令。
 */
void ControlThreadProc() {
    try {
        // 作为客户端连接到控制通道
        SharedMemoryIPC controlChannel(SharedMemoryIPC::Role::CLIENT);

        while (true) {
            ControlPacket packet;
            // 阻塞等待，直到从服务端接收到一条命令
            if (controlChannel.read(packet,10000)) {
                switch (packet.command) {
                case ControlCommand::START_LOGGING:
                    StartMessageLogging();
                    break;
                case ControlCommand::STOP_LOGGING:
                    StopMessageLogging();
                    break;
                case ControlCommand::TERMINATE:
                    return; // 收到终止命令，退出线程
                }
            }
            else {
                // 如果读取失败或超时，也退出线程
                return;
            }
        }
    }
    catch (...) {
        // 捕获所有异常，确保线程不会使进程崩溃
    }
}
