// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "pch.h"
#include "detours.h"
#include <windows.h>
#include <stdio.h> // for swprintf_s






//hook messageBox函数  当文件无效的时候会弹出一些messageBox窗口干扰，我们需要关闭它
static BOOL(WINAPI* OriginalMessageBox)(HWND, LPCTSTR, LPCTSTR, UINT) = MessageBox;

BOOL WINAPI HookedMessageBox(HWND hWnd, LPCTSTR lpText, LPCTSTR lpCaption, UINT uType) {
    

    //无论用户是否点击，我都返回1  1就是已经点击了的意思
    return IDOK;
}

//
//
//需要把readfile里面关于fuzzer的代码移出去，让readfile只进行读文件的操作
// 
// 
// hook readfile 系统调用
static BOOL(WINAPI* OriginalReadFile)(HANDLE, LPVOID, DWORD, LPDWORD, LPOVERLAPPED) = ReadFile;
// 暂且先认为程序读取target file只会open一个hfile，而不会open多个hfile，如果open多个就需要一个链表记录了


BOOL WINAPI HookedReadFile(
    HANDLE       hFile,
    LPVOID       lpBuffer,
    DWORD        nNumberOfBytesToRead,
    LPDWORD      lpNumberOfBytesRead,
    LPOVERLAPPED lpOverlapped
) {

    // 看一下这些文件的句柄的路径都是啥
 // 3. 缓冲区也必须是宽字符类型
    std::vector<wchar_t> filePathBuffer(MAX_PATH);

    // 4. 【关键】调用API的宽字符版本: GetFinalPathNameByHandleW
    DWORD pathLength = GetFinalPathNameByHandleW(
        hFile,
        filePathBuffer.data(),      // [out] 宽字符路径会写入这里
        filePathBuffer.size(),      // 缓冲区大小 (单位是 wchar_t 的数量)
        FILE_NAME_NORMALIZED        // 标志位保持不变
    );

    // 检查结果
    if (pathLength > 0 && pathLength < filePathBuffer.size()) {
        // 成功！现在 filePathBuffer.data() 是一个 wchar_t* 指针，指向文件路径
        // 5. 使用您的日志函数，并传入宽字符格式化字符串 L"%s"
        WriteLog(L"成功获取文件路径: %s", filePathBuffer.data());
    }
    else {
        // 失败，记录错误
        WriteLog(L"无法获取文件路径。GetLastError() = %lu", GetLastError());
    }



    // 检查当前传入的 hFile 是否就是本轮已经确认的目标句柄。
    if (g_ReadTargetFileHandle != NULL && hFile == g_ReadTargetFileHandle) {

        // --- “快速通道” ---

        if (g_analysis_flag.load() == false) {
            // 分析模式: 记录后续的读操作。

            LARGE_INTEGER readOffset = { 0 };
            if (lpOverlapped == NULL) { SetFilePointerEx(hFile, { 0 }, &readOffset, FILE_CURRENT); }
            else { readOffset.LowPart = lpOverlapped->Offset; readOffset.HighPart = lpOverlapped->OffsetHigh; }

            BOOL ret = OriginalReadFile(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped);

            if (ret && lpNumberOfBytesRead && *lpNumberOfBytesRead > 0 && g_pSharedData->op_count < MAX_OFFSETS) {
                g_pSharedData->offsets[g_pSharedData->op_count] = readOffset.QuadPart;
                g_pSharedData->sizes[g_pSharedData->op_count] = *lpNumberOfBytesRead;
                g_pSharedData->op_count++;
            }
            return ret;
        }
        else {


            BOOL sim_ret = SimulateReadFileFromMemory(
                g_pSharedData->buffer, g_pSharedData->data_size, hFile, lpBuffer,
                nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped
            );
            return sim_ret;
        }
    }


    LARGE_INTEGER readOffsetBeforeRead = { 0 };
    if (lpOverlapped == NULL) { SetFilePointerEx(hFile, { 0 }, &readOffsetBeforeRead, FILE_CURRENT); }
    else { readOffsetBeforeRead.LowPart = lpOverlapped->Offset; readOffsetBeforeRead.HighPart = lpOverlapped->OffsetHigh; }

    BOOL ret = OriginalReadFile(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped);

    // 检查读取到的内容
    if (ret && lpNumberOfBytesRead && *lpNumberOfBytesRead >= 11) {
        const BYTE* data = static_cast<BYTE*>(lpBuffer);
        if (memcmp(data, "\x41\x43\x31\x30\x32\x31\x00\x00\x00\x00\x00", 11) == 0) {

            g_ReadTargetFileHandle = hFile;

            if (g_analysis_flag.load() == false) {

                g_pSharedData->op_count = 0;
                g_pSharedData->offsets[g_pSharedData->op_count] = readOffsetBeforeRead.QuadPart;
                g_pSharedData->sizes[g_pSharedData->op_count] = *lpNumberOfBytesRead;
                g_pSharedData->op_count++;

                g_pSharedData->command = FuzzProtocol::CMD_INIT;
            }

        }
    }

    return ret;
}

// 在 pch.cpp 或 dllmain.cpp 中添加这个新函数
void RealMain() {
    // 这个函数在一个全新的线程中运行，已经脱离了 DllMain 和加载器锁的限制
    // 在这里执行所有复杂的初始化是安全的
    init();
}

BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
)
{
    if (DetourIsHelperProcess()) {
        return TRUE;
    }

    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    {
        // 初始化日志系统
        InitializeCriticalSection(&g_csLog);
        DWORD pid = GetCurrentProcessId();
        swprintf_s(g_logFilePath, MAX_PATH, L"E:\\microKernel\\GFLClient0.1\\hook_log_%lu.txt", pid);

        // 开始记录日志

        DetourRestoreAfterWith();

        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());

        HANDLE hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)RealMain, NULL, 0, NULL);
        if (hThread) {
            CloseHandle(hThread); // 我们不需要管理这个线程，让它自生自灭即可
        }

        DetourAttach(&(PVOID&)OriginalReadFile, HookedReadFile);
        DetourAttach(&(PVOID&)OriginalMessageBox, HookedMessageBox);
        LONG error = DetourTransactionCommit();
     

        break;
    }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break; // 通常无需处理
    case DLL_PROCESS_DETACH:
    {
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());

        DetourDetach(&(PVOID&)OriginalReadFile, HookedReadFile);
        DetourDetach(&(PVOID&)OriginalMessageBox, HookedMessageBox);

        LONG error = DetourTransactionCommit();


        DeleteCriticalSection(&g_csLog);
        break;
    }
    }
    return TRUE;
}


