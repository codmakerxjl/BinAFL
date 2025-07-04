#pragma once
#ifndef HIJACKER_H
#define HIJACKER_H
#include <WinUser.h>
#include "detours.h"
#include <unordered_map>
#include <optional> // C++17, for safe return value
#include <mutex>



// 安装API钩子并初始化日志通道。
bool AttachHooks();

// 卸载API钩子并清理日志通道。
bool DetachHooks();

void StartMessageLogging();
void StopMessageLogging();
BOOL WINAPI HookedReadFile(
    HANDLE       hFile,
    LPVOID       lpBuffer,
    DWORD        nNumberOfBytesToRead,
    LPDWORD      lpNumberOfBytesRead,
    LPOVERLAPPED lpOverlapped
);
#endif 