#include "pch.h"
#include "client_test.h"
#include <windows.h>
#include <string>
#include "SharedMemoryIPC.h" // Assumes SharedMemoryIPC.h/.cpp are in the project

void run_client_test() {
    try {
        // As IPC Client, connect to existing shared memory.
        SharedMemoryIPC ipc(SharedMemoryIPC::Role::CLIENT);

        MessageBoxW(NULL, L"IPC Client thread started and connected to shared memory!", L"IPC Client", MB_OK | MB_ICONINFORMATION);

        // Loop to read data.
        while (true) {
            SharedData receivedData;
            // Read data, with a 10-second timeout.
            if (ipc.read(receivedData, 10000)) {
                // To show the received data, we'll use a MessageBox.
                wchar_t buffer[512];
                wsprintfW(buffer, L"Client received data:\n buff[0] = %d\n buff[1] = %d", receivedData.buff[0], receivedData.buff[1]);
                MessageBoxW(NULL, buffer, L"IPC Client Data", MB_OK);
            }
            else {
                // If timeout, the server probably stopped sending data.
                MessageBoxW(NULL, L"Read data timeout. Client thread will exit.", L"IPC Client", MB_OK | MB_ICONWARNING);
                break; // Exit loop.
            }
        }
    }
    catch (const std::runtime_error& e) {
        // Convert the C++ string to a wide string for MessageBoxW.
        std::string errorMsg = e.what();
        std::wstring wErrorMsg(errorMsg.begin(), errorMsg.end());
        MessageBoxW(NULL, wErrorMsg.c_str(), L"IPC Client Critical Error", MB_OK | MB_ICONERROR);
    }
}
