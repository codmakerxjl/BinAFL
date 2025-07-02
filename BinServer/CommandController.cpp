#include "pch.h"
#include "CommandController.h"

CommandController::CommandController(SharedMemoryIPC& ipc_channel)
    : channel_(ipc_channel) {}

void CommandController::StartLogging() {
    ControlPacket packet;
    packet.command = ControlCommand::START_LOGGING;
    // ʹ��ͨ�����������
    channel_.write(packet);
}

void CommandController::StopLogging() {
    ControlPacket packet;
    packet.command = ControlCommand::STOP_LOGGING;
    // ʹ��ͨ�����������
    channel_.write(packet);
}

void CommandController::SendTerminate() {
    ControlPacket packet;
    packet.command = ControlCommand::TERMINATE;
    // ʹ��ͨ�����������
    channel_.write(packet);
}
