#include "Agent.h"
#include <iostream>
#include <stdexcept>
#include <string>

Agent::Agent() : listen_sock(INVALID_SOCKET), client_sock(INVALID_SOCKET), server_port(0) {
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        throw std::runtime_error("WSAStartup failed.");
    }
}

Agent::~Agent() {
    stopServer();
    WSACleanup();
}

void Agent::startServer(int port) {
    if (listen_sock != INVALID_SOCKET) {
        std::cout << "[Agent] Server is already running." << std::endl;
        return;
    }
    this->server_port = port; // 存储端口号

    listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_sock == INVALID_SOCKET) {
        throw std::runtime_error("Socket creation failed.");
    }

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    if (bind(listen_sock, (SOCKADDR*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        throw std::runtime_error("Bind failed. Error: " + std::to_string(WSAGetLastError()));
    }

    if (listen(listen_sock, SOMAXCONN) == SOCKET_ERROR) {
        throw std::runtime_error("Listen failed. Error: " + std::to_string(WSAGetLastError()));
    }

    std::cout << "[Agent] Server started and listening on port " << port << "." << std::endl;
}

void Agent::stopServer() {
    cleanupSockets();
    std::cout << "[Agent] Server stopped." << std::endl;
}

// 这就是您需要的核心接口函数
std::string Agent::executeCommandViaAgent(const std::string& taskForAgent) {
    if (listen_sock == INVALID_SOCKET) {
        throw std::logic_error("Server is not started. Call startServer() first.");
    }

    // 1. 构造启动 llm 客户端的命令
    // 使用 start cmd /k 可以让新窗口在命令执行后保持打开，方便调试
    // 如果希望执行后自动关闭，可以使用 start cmd /c
    std::string launch_command = "start cmd /k llm " + taskForAgent ;
    std::cout << "[Agent] Launching agent with task: \"" << launch_command << "\"" << std::endl;

    // 2. 在新终端中执行命令，启动 llm
    system(launch_command.c_str());

    // 3. 等待 llm 客户端连接回来
    std::cout << "[Agent] Waiting for agent to connect..." << std::endl;
    client_sock = accept(listen_sock, nullptr, nullptr);
    if (client_sock == INVALID_SOCKET) {
        throw std::runtime_error("Accept failed. Error: " + std::to_string(WSAGetLastError()));
    }
    std::cout << "[Agent] Agent connected!" << std::endl;

    // 4. 接收来自 llm 的命令
    char buffer[2048] = { 0 };
    int bytes_received = recv(client_sock, buffer, sizeof(buffer) - 1, 0);

    std::string received_command;
    if (bytes_received > 0) {
        received_command = std::string(buffer);
        std::cout << "[Agent] <- Agent: " << received_command << std::endl;
        sendResponse("{\"status\": \"PROCEED\"}");
    }
    else {
        std::cerr << "[Agent] Recv failed or agent disconnected prematurely." << std::endl;
    }

    // 5. 关闭【客户端】套接字，并返回结果
    closesocket(client_sock);
    client_sock = INVALID_SOCKET;

    return received_command;
}


// --- 私有辅助函数 ---

bool Agent::sendResponse(const std::string& response) {
    if (client_sock == INVALID_SOCKET) return false;
    std::cout << "[Agent] -> Agent: " << response << std::endl;
    int result = send(client_sock, response.c_str(), (int)response.length(), 0);
    if (result == SOCKET_ERROR) {
        std::cerr << "Send failed. Error: " << WSAGetLastError() << std::endl;
        return false;
    }
    return true;
}

void Agent::cleanupSockets() {
    if (client_sock != INVALID_SOCKET) {
        closesocket(client_sock);
        client_sock = INVALID_SOCKET;
    }
    if (listen_sock != INVALID_SOCKET) {
        closesocket(listen_sock);
        listen_sock = INVALID_SOCKET;
    }
}