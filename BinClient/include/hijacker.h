#pragma once
#ifndef HIJACKER_H
#define HIJACKER_H
#include <WinUser.h>
#include "detours.h"
#include <unordered_map>
#include <optional> // C++17, for safe return value
#include <mutex>



// ��װAPI���Ӳ���ʼ����־ͨ����
bool AttachHooks();

// ж��API���Ӳ�������־ͨ����
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