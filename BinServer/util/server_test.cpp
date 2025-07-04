#include "pch.h"
#include "server_test.h"
#include <cstdio>       // For wprintf, fprintf
#include <thread>       // For std::this_thread
#include <chrono>       // For std::chrono
#include "SharedMemoryIPC.h" // Assumes SharedMemoryIPC.h/.cpp are in the project
#include "start_process.h" // Assumes this contains startAndInjectProcess

void run_server_test(PROCESS_INFORMATION& pi, const std::wstring& executablePath, const std::wstring& dllPath) {
    try {
        // 1. As IPC Server, create shared memory and sync objects.
        SharedMemoryIPC ipc(SharedMemoryIPC::Role::SERVER);
        wprintf(L"IPC Server started, shared memory created.\n");

        // 2. Start target process and inject our client DLL.
        if (startAndInjectProcess(pi, executablePath, dllPath)) {
            wprintf(L"DLL injected successfully. Server will start sending data...\n");

            // 3. Loop to send data.
            for (int i = 0; i < 10; ++i) {
                SharedData dataToSend;
                dataToSend.buff[0] = 1000 + i;
                dataToSend.buff[1] = 2000 + i;

                // Call write() to put data into shared memory.
                if (ipc.write(dataToSend)) {
                    wprintf(L"Server has written data, buff[0] = %d\n", dataToSend.buff[0]);
                }
                else {
                    wprintf(L"Write data timeout.\n");
                    break;
                }
                // Wait for a moment to give the client time to respond.
                std::this_thread::sleep_for(std::chrono::milliseconds(1500));
            }
            wprintf(L"Server has finished sending data.\n");

            // Wait for the target process to terminate.
            wprintf(L"Waiting for target process to exit...\n");
            WaitForSingleObject(pi.hProcess, INFINITE);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            wprintf(L"Target process has exited. Server is shutting down.\n");
        }
        else {
            fprintf(stderr, "DLL injection failed.\n");
        }
    }
    catch (const std::runtime_error& e) {
        fprintf(stderr, "A critical error occurred: %s\n", e.what());
        // Use system("pause") for easy debugging in a console window.
        system("pause");
    }
}
