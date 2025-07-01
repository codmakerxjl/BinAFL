// pch.h: 这是预编译标头文件。
// 下方列出的文件仅编译一次，提高了将来生成的生成性能。
// 这还将影响 IntelliSense 性能，包括代码完成和许多代码浏览功能。
// 但是，如果此处列出的文件中的任何一个在生成之间有更新，它们全部都将被重新编译。
// 请勿在此处添加要频繁更新的文件，这将使得性能优势无效。

#ifndef PCH_H
#define PCH_H

//重放的lparam和wparam定义
constexpr auto autoCAD_replay_lparam = 0xE110;
constexpr auto autoCAD_replay_wparam = 0x0;

// 添加要在此处预编译的标头
#include "framework.h"
#include <stdlib.h>
#include <atomic>
#include <thread>
#include <string>
#include <mutex>
#include <condition_variable>
#include <chrono> // 用于 std::this_thread::sleep_for
#include <stdio.h>
#include <vector>
#include "SharedData.h"
#include <algorithm>
struct FindDescendantData {
    LPCWSTR targetTitle;
    HWND foundHwnd;
};

//定义打开程序的顶级窗口和需要重放操作的的子窗口的全局变量
extern HWND hwndProcessTop;
extern HWND hwndReplyWindow;

//关闭s.dwg全局变量
extern std::atomic<bool> g_keepMonitoring;
extern std::atomic<bool>g_isMonitor;
extern std::atomic<double> g_latest_cpu_usage;
extern std::atomic<double> g_latest_io_read_rate;
extern std::atomic<double> g_latest_io_write_rate;
extern std::atomic<bool> g_monitoring_thread_running;
extern std::atomic<bool> g_stop_monitoring_flag;
extern HANDLE g_ReadTargetFileHandle;

extern const TCHAR shm_name[] ;
extern const TCHAR sem_fuzzer_ready_name[] ;
extern const TCHAR sem_target_done_name[] ;

extern SharedData* g_pSharedData;
extern HANDLE g_hSemFuzzerReady;
extern HANDLE g_hSemTargetDone;

// 全局变量，用于日志记录
extern CRITICAL_SECTION g_csLog;
extern wchar_t g_logFilePath[MAX_PATH];

//读文件初始化标志位，当它为false时，需要分析读文件时的结构，当它为true时，直接根据文件结构进行变异。
extern std::atomic<bool> g_analysis_flag;

void init();
HWND FindDescendantWindow(HWND hParent, LPCWSTR title);
BOOL CALLBACK EnumDescendantProc(HWND hwndChild, LPARAM lParam);
bool ReplyOpen();
//监控需要关闭的s.dwg窗口
std::thread StartDescendantWindowMonitor(HWND parentHwnd, std::wstring title);
void MonitorAndCloseDescendantWindowWorker(HWND parentHwnd, std::wstring title);
double get_current_process_cpu_usage_from_thread();
std::pair<double, double> get_current_process_io_rate_from_thread();
void start_monitoring_background_thread();
void stop_monitoring_background_thread();
void monitoring_worker_thread_function();

void reply_open_worker();

bool initSharedMem();
void WriteLog(const wchar_t* format, ...);

//直接监控整个窗口的打开和关闭
void TargetWindowsOpenAndClose();
void EndTurnAndSignalNext();
/**
 * @brief 从内存中模拟Windows API ReadFile的行为。
 *
 * @param pSharedMemBase 指向包含完整文件数据的内存块（共享内存）的基地址。
 * @param fileSize 内存中文件的总大小（以字节为单位）。
 * @param hFile 原始ReadFile调用中的文件句柄，用于获取和设置文件指针。
 * @param lpBuffer 接收数据的目标缓冲区（来自原始ReadFile调用）。
 * @param nNumberOfBytesToRead 请求读取的字节数（来自原始ReadFile调用）。
 * @param lpNumberOfBytesRead 指向一个DWORD变量的指针，函数将用实际读取的字节数填充它。
 * @param lpOverlapped 指向OVERLAPPED结构的指针（来自原始ReadFile调用）。
 *
 * @return 成功则返回TRUE，失败则返回FALSE。
 */
BOOL SimulateReadFileFromMemory(
    _In_ PVOID        pSharedMemBase,
    _In_ ULONGLONG    fileSize,
    _In_ HANDLE       hFile,
    _Out_ LPVOID      lpBuffer,
    _In_ DWORD        nNumberOfBytesToRead,
    _Out_ LPDWORD     lpNumberOfBytesRead,
    _In_opt_ LPOVERLAPPED lpOverlapped);



#endif //PCH_H
