#pragma once

//这个是BinServer控制BinClient ipc模块

// 定义了服务端可以发送给客户端的控制命令
enum class ControlCommand {
    NO_OP,          // 无操作
    START_LOGGING,  // 命令客户端开始记录消息
    STOP_LOGGING,   // 命令客户端停止记录消息
    TERMINATE       // 命令客户端的控制线程退出
};

// 在共享内存中传递的数据包结构
struct ControlPacket {
    ControlCommand command;
};
