#include "pch.h"
#include "CommandController.h"

CommandController::CommandController(SharedMemoryIPC& ipc_channel)
    : channel_(ipc_channel) {}

void CommandController::StartLogging() {
    ControlPacket packet;
    packet.command = ControlCommand::START_LOGGING;
    // 使用通道发送命令包
    channel_.write(packet);
}

void CommandController::StopLogging() {
    ControlPacket packet;
    packet.command = ControlCommand::STOP_LOGGING;
    // 使用通道发送命令包
    channel_.write(packet);
}

void CommandController::SendTerminate() {
    ControlPacket packet;
    packet.command = ControlCommand::TERMINATE;
    // 使用通道发送命令包
    channel_.write(packet);
}
