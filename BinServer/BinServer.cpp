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
#include "server_test.h"
std::atomic<bool> g_bExitLogThread = false;

// 将 replayer 定义为全局指针，以便 FuzzTarget 函数可以访问它
std::unique_ptr<MessageReplayer> g_replayer = nullptr;

//
// 这是我们新的目标函数，WinAFL 将会反复调用它。
// 必须使用 extern "C" 和 __declspec(dllexport) 将其导出，以便 WinAFL 按名称找到它。
//
extern "C" __declspec(dllexport) void FuzzTarget() {
    if (g_replayer) {
        g_replayer->replayAggregatedSequence(100);
    }
}

int main(int argc, char* argv[])
{   
    // Check for correct command-line arguments.
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <executable_path> <dll_path> [arguments...]\n", argv[0]);
        return -1;
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

    std::wstring arguments;
    for (int i = 3; i < argc; ++i) {
        std::string arg_str(argv[i]);
        // 为每个参数加上引号并用空格隔开
        arguments += L"\"" + std::wstring(arg_str.begin(), arg_str.end()) + L"\" ";
    }
    if (!arguments.empty()) {
        arguments.pop_back();
    }
    SharedMemoryIPC ipc(SharedMemoryIPC::Role::SERVER); //ipc初始化

    CommandController controller(ipc); //初始化
    //创建并初始化 SharedBitmap 监控器 (在启动子进程之前)

    wprintf(L"Target Executable: %s\n", executablePath.c_str());
    wprintf(L"Target DLL: %s\n", dllPath.c_str());
    wprintf(L"Target Arguments: %s\n", arguments.c_str());
    PROCESS_INFORMATION pi = { 0 }; // Initialize to zero.

    startAndInjectProcess(pi, executablePath, dllPath, arguments);
    controller.StartLogging();

    Sleep(waitingTime);
    Agent my_agent_handler;
    my_agent_handler.startServer(agentPort);

    std::string command = buildAgentPrompt(logFilePath,messageFolderPath,agentPort);
    std::string response = my_agent_handler.executeCommandViaAgent(command);

    if (response.find("done") != std::string::npos) {
        wprintf(L"Agent has successfully analyzed the log based on heuristics.");
        wprintf(L"It's my time to replay the extracted sequences.");
    }
    else {
        wprintf(L"Agent reported an error.\nResponse: %hs", response.c_str());
    }
    controller.StopLogging();

    // 1. 创建一个消息重放器实例
    g_replayer = std::make_unique<MessageReplayer>();
    g_replayer->runInteractiveSession(messageFolderPath, messagePrefix);

    // 3. 会话结束后，将所有被标记为“有效”的文件保存到 "effective_sequences" 文件夹
    g_replayer->saveEffectiveFiles(effectiveMsgFolder);

    // 4. 打印最终结果
    std::cout << "\nInteractive session finished." << std::endl;
    std::cout << "A total of " << g_replayer->getEffectiveFiles().size() << " sequences were marked as effective." << std::endl;


    // 1. 一次性加载所有有效文件到内存中的主队列
    // 这个操作可能需要几秒钟，取决于文件数量和大小
    std::cout << "Loading all effective sequences into memory..." << std::endl;
    if (g_replayer->loadAndAggregateSequences("effective_sequences", "message_")) {

        // 2. 现在可以快速地、多次地重放整个聚合序列，无需再读文件
        std::cout << "\nPress ENTER to replay the entire aggregated sequence for the first time.";
        g_replayer->replayAggregatedSequence(10); // 以10毫秒的延迟重放

        std::cout << "\nPress ENTER to replay it again instantly.";
        g_replayer->replayAggregatedSequence(10); // 再次重放

        while (true) {
            FuzzTarget();
        }


        // 3. 如果需要，可以清空队列
        // replayer.clearAggregatedSequence();
    }
    else {
        std::cout << "Failed to load any sequences. Please check the directory and prefix." << std::endl;
    }
    //WaitForSingleObject(pi.hProcess, INFINITE);

    //@这里是测试通讯管道的代码
    //// Call the single function that runs the entire test.
    
    //run_server_test(pi, executablePath, dllPath);
    //run_afl_mutator_tests();



    return 0;
}
