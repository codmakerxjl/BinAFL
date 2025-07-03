#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>

#pragma comment(lib, "ws2_32.lib")

class Agent {
public:
    Agent();
    ~Agent();

    // 启动服务器，在程序开始时调用一次
    void startServer(int port);

    // 关闭服务器，在程序结束时调用一次
    void stopServer();

    // 核心接口函数：接收一个任务指令，启动llm，等待其连接并返回最终命令
    std::string executeCommandViaAgent(const std::string& taskForAgent);

private:
    bool sendResponse(const std::string& response);
    void cleanupSockets();

    WSADATA wsaData;
    SOCKET listen_sock;
    SOCKET client_sock;
    int server_port; // 存储端口号
};