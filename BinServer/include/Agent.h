#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>

#pragma comment(lib, "ws2_32.lib")

class Agent {
public:
    Agent();
    ~Agent();

    // �������������ڳ���ʼʱ����һ��
    void startServer(int port);

    // �رշ��������ڳ������ʱ����һ��
    void stopServer();

    // ���Ľӿں���������һ������ָ�����llm���ȴ������Ӳ�������������
    std::string executeCommandViaAgent(const std::string& taskForAgent);

private:
    bool sendResponse(const std::string& response);
    void cleanupSockets();

    WSADATA wsaData;
    SOCKET listen_sock;
    SOCKET client_sock;
    int server_port; // �洢�˿ں�
};