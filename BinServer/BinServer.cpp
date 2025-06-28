// BinServer.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include "start_process.h"
#include <windows.h>
#include "pch.h"
#include "server_test.h"
#include "CommandController.h"

std::atomic<bool> g_bExitLogThread = false;

int main(int argc, char* argv[])
{
    // Check for correct command-line arguments.
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <executable_path> <dll_path>\n", argv[0]);
        return 1;
    }
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

    wprintf(L"Waiting for target process to exit...\n");
    WaitForSingleObject(pi.hProcess, INFINITE);

    //@这里是测试通讯管道的代码
    //// Call the single function that runs the entire test.
    
    //run_server_test(pi, executablePath, dllPath);




    return 0;
}
