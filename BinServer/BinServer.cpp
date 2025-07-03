// BinServer.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include "Agent.h"
#include <iostream>
#include "start_process.h"
#include <windows.h>
#include "pch.h"
#include "SimpleIniParser.h"
#include "CommandController.h"
#include "message_replayer.h"
std::atomic<bool> g_bExitLogThread = false;

int main(int argc, char* argv[])
{   
    // Check for correct command-line arguments.
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <executable_path> <dll_path>\n", argv[0]);
        return 1;
    }

    SimpleIniParser parser;
    if (!parser.load("config.ini")) {
        std::cerr << "无法加载配置文件" << std::endl;
        return 1;
    }
    int waitingTime=parser.getInt("AgentClient", "waitingTime");
    int agentPort=parser.getInt("AgentClient", "agentPort");
    std::string logFilePath=parser.get("server", "logFilePath");
    std::string messageFolderPath=parser.get("server", "messageFolderPath");
    std::string messagePrefix=parser.get("server", "messagePrefix");
    std::string effectiveMsgFolder=parser.get("server", "effectiveMsgFolder");

    // Convert char* to std::wstring using the user's original method.
    // For production code, consider using MultiByteToWideChar for more robust conversion.
    std::string exec_str(argv[1]);
    std::string dll_str(argv[2]);
    std::wstring executablePath(exec_str.begin(), exec_str.end());
    std::wstring dllPath(dll_str.begin(), dll_str.end());

    SharedMemoryIPC ipc(SharedMemoryIPC::Role::SERVER); //ipc初始化

    CommandController controller(ipc); //初始化

    wprintf(L"Target Executable: %s\n", executablePath.c_str());
    wprintf(L"Target DLL: %s\n", dllPath.c_str());
    PROCESS_INFORMATION pi = { 0 }; // Initialize to zero.

    startAndInjectProcess(pi, executablePath, dllPath); //启动目标程序并且注入dll
    controller.StartLogging();

    Sleep(waitingTime);
    Agent my_agent_handler;
    my_agent_handler.startServer(agentPort);

    std::string command = buildAgentPrompt(logFilePath,messageFolderPath,agentPort);
    std::string response = my_agent_handler.executeCommandViaAgent(command);

    if (response.find("done") != std::string::npos) {
        wprintf(L"Agent has successfully analyzed the log based on heuristics.");
        wprintf(L"It's my time to replay the extracted sequences.");
        // 在这里可以开始您的重放逻辑，去读取 message_sequence_*.log 文件
    }
    else {
        wprintf(L"Agent reported an error.\nResponse: %hs", response.c_str());
    }
    controller.StopLogging();

    // 1. 创建一个消息重放器实例
    MessageReplayer replayer;
    replayer.runInteractiveSession(messageFolderPath, messagePrefix);

    // 3. 会话结束后，将所有被标记为“有效”的文件保存到 "effective_sequences" 文件夹
    replayer.saveEffectiveFiles(effectiveMsgFolder);

    // 4. 打印最终结果
    std::cout << "\nInteractive session finished." << std::endl;
    std::cout << "A total of " << replayer.getEffectiveFiles().size() << " sequences were marked as effective." << std::endl;
    WaitForSingleObject(pi.hProcess, INFINITE);

    //@这里是测试通讯管道的代码
    //// Call the single function that runs the entire test.
    
    //run_server_test(pi, executablePath, dllPath);




    return 0;
}
