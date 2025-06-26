// pch.h: 这是预编译标头文件。
// 下方列出的文件仅编译一次，提高了将来生成的生成性能。
// 这还将影响 IntelliSense 性能，包括代码完成和许多代码浏览功能。
// 但是，如果此处列出的文件中的任何一个在生成之间有更新，它们全部都将被重新编译。
// 请勿在此处添加要频繁更新的文件，这将使得性能优势无效。

#ifndef PCH_H
#define PCH_H

// 添加要在此处预编译的标头
#include "framework.h"
#include <fstream>
#include <cstdio>
#define _WIN32_WINNT 0x0600
#define WIN32_LEAN_AND_MEAN
constexpr auto csrDataRva_x64 = 0x186BC8;
constexpr auto csrDataSize_x64 = 0x78;
#include <windows.h>
#include <winnt.h>
#include <winternl.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <process.h>
#include <processthreadsapi.h>
#include <iostream>
#include <Windows.h>
#include <sstream>
#include <vector>
#include <string>
#include <iomanip>
#include <tlhelp32.h>
#include <cstdint> // 为了使用 uint64_t 跨平台类型
#include <set>
#include <thread>
#include <chrono>
#include <map>
#include <mutex>
// 假设我们有上一问实现的 recordAllThreadsInProcess() 函数
#include <windows.h>
#include <tlhelp32.h>
typedef struct _SECTION_IMAGE_INFORMATION {
	PVOID EntryPoint;
	ULONG StackZeroBits;
	ULONG StackReserved;
	ULONG StackCommit;
	ULONG ImageSubsystem;
	WORD SubSystemVersionLow;
	WORD SubSystemVersionHigh;
	ULONG Unknown1;
	ULONG ImageCharacteristics;
	ULONG ImageMachineType;
	ULONG Unknown2[3];
} SECTION_IMAGE_INFORMATION, * PSECTION_IMAGE_INFORMATION;

typedef struct _RTL_USER_PROCESS_INFORMATION {
	ULONG Size;
	HANDLE Process;
	HANDLE Thread;
	CLIENT_ID ClientId;
	SECTION_IMAGE_INFORMATION ImageInformation;
} RTL_USER_PROCESS_INFORMATION, * PRTL_USER_PROCESS_INFORMATION;

#define RTL_CLONE_PROCESS_FLAGS_CREATE_SUSPENDED	0x00000001
#define RTL_CLONE_PROCESS_FLAGS_INHERIT_HANDLES		0x00000002
#define RTL_CLONE_PROCESS_FLAGS_NO_SYNCHRONIZE		0x00000004

#define RTL_CLONE_PARENT				0
#define RTL_CLONE_CHILD					297





typedef DWORD pid_t;

typedef NTSTATUS(*RtlCloneUserProcess_f)(ULONG ProcessFlags,
	PSECURITY_DESCRIPTOR ProcessSecurityDescriptor /* optional */,
	PSECURITY_DESCRIPTOR ThreadSecurityDescriptor /* optional */,
	HANDLE DebugPort /* optional */,
	PRTL_USER_PROCESS_INFORMATION ProcessInformation);


// CsrClientConnectToServer函数对定义
typedef NTSTATUS(NTAPI* CsrClientConnectToServer_t)(
	IN PWSTR ObjectDirectory,
	IN ULONG ServerId,
	IN PVOID ConnectionInfo,
	IN ULONG ConnectionInfoSize,
	OUT PBOOLEAN ServerToServerCall
	);
static HMODULE ntdll = GetModuleHandleA("ntdll.dll");
CsrClientConnectToServer_t CsrClientConnectToServer = (CsrClientConnectToServer_t)GetProcAddress(ntdll, "CsrClientConnectToServer");
//注册线程到CSRSS
typedef NTSTATUS
(NTAPI* RtlRegisterThreadWithCsrss_t)(
	VOID
	);

RtlRegisterThreadWithCsrss_t RtlRegisterThreadWithCsrss = (RtlRegisterThreadWithCsrss_t)GetProcAddress(ntdll, "RtlRegisterThreadWithCsrss");


int fork();
bool resumeProcess(DWORD processId);
void ResumeProcess(DWORD processID);
bool MakeAllSectionsExecuteReadWrite64();
BOOL ConnectCsrChild();
void DebugPrint(const char* format, ...);

void AddLog(const std::string& msg);



// 用于描述一个内存区域的信息
struct MemoryRegion {
	std::string name;        // 节区名, e.g., ".data"
	PVOID startAddress;      // 起始地址
	SIZE_T size;             // 大小
};

// 用于存储整个程序全局变量的快照
// 内部是一个内存区域列表，每个区域对应一个数据备份
struct GlobalsSnapshot {
	std::vector<MemoryRegion> regions;
	std::vector<std::vector<char>> savedData;

	bool isValid() const {
		return !regions.empty() && !savedData.empty();
	}
};

std::vector<MemoryRegion> find_global_data_regions();


// 定义一个结构体，用于在回调函数之间传递数据
struct EnumData {
	DWORD processId;
	std::set<HWND>* pHandles;
};

BOOL CALLBACK EnumThreadWndProc(HWND hwnd, LPARAM lParam);
BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam);
BOOL CALLBACK EnumChildProc(HWND hwnd, LPARAM lParam);




//线程处理代码
// 用于存储每个被管理线程的信息
struct ManagedThreadInfo {
	HANDLE hThread;                 // 线程句柄
	HANDLE hStopEvent;              // 用于通知该线程停止的事件
	LPTHREAD_START_ROUTINE pOriginalThreadProc; // 原始的线程函数地址
	LPVOID pOriginalParameter;      // 原始的线程参数
};

// 全局的、线程安全的线程管理器
static std::mutex g_threadMapMutex;
static std::map<DWORD, ManagedThreadInfo> g_managedThreads;
DWORD WINAPI UniversalThreadWrapper(LPVOID lpParam);
std::string MessageIDToString(UINT msg);

bool IsUserActionMessage(UINT msg);
struct FullMessageRecord {
	MSG         msg;        // 完整的原始MSG结构
	SYSTEMTIME  timestamp;  // 记录时的本地时间，更易读
	DWORD       threadId;   // 派发此消息的线程ID
};

void SendCustomMessage();


extern HWND hwndProcessTop;
extern HWND hwndReplyWindow;
constexpr auto autoCAD_replay_lparam = 0xE110;
constexpr auto autoCAD_replay_wparam = 0x0;
void init();
struct FindDescendantData {
	LPCWSTR targetTitle;
	HWND foundHwnd;
};

void sendOpenMessage();
#endif //PCH_H
