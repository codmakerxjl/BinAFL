// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "pch.h"
#include "hijacker.h"
#include "client_test.h"
#include <thread>
// The communication thread now just calls our test function.
DWORD WINAPI CommunicationThread(LPVOID lpParam) {
    run_client_test();
    return 0;
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    if (DetourIsHelperProcess()) {
        return true;
    }


    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    {

        MessageBox(NULL, L"BinClient Attached to Process!", L"Notification", MB_OK | MB_ICONINFORMATION);
        //run_fuzzing_speed_test();
        AttachHooks();
        std::thread(ControlThreadProc).detach(); //用来接受server的命令
        break;
    }
        
    case DLL_THREAD_ATTACH: break;
    case DLL_THREAD_DETACH: break;
    case DLL_PROCESS_DETACH: {
        DetachHooks();
        break;
    }

    }
    return TRUE;
}

