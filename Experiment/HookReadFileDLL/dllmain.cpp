// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "pch.h"
#include "detours.h"
#include <Windows.h>
#include <mutex>
#include <queue>
#include <string>
#include <WinUser.h>
#include <psapi.h>
#include <sstream>
#define IDC_EDIT_LOG      1001
#define IDC_BTN_CLEAR     1002
#define IDC_BTN_REPLAY    1003  // 新增：重放message按鈕ID
#define IDC_BTN_RESTORE   1004  // 新增：恢復按鈕ID
#define IDC_BTB_RECORDMSG 1005 //记录message信息的按钮


// IDs for the new Send Message feature ---
#define IDC_EDIT_HWND     1010
#define IDC_EDIT_MSGID    1011
#define IDC_EDIT_WPARAM   1012
#define IDC_EDIT_LPARAM   1013
#define IDC_BTN_SENDMSG   1014
#define IDC_STATIC_INFO   1015 // A static label for instructions


// *** 消息序列发送功能控件 ID ***
#define IDC_STATIC_SEQ_INFO     1020
#define IDC_EDIT_MSG_SEQUENCE   1021
#define IDC_BTN_SEND_SEQUENCE   1022


// 全局变量
static HWND g_hMainWnd = nullptr;
static HWND g_hEdit = nullptr;
static HINSTANCE g_hInstance = nullptr;
static std::mutex g_msgMutex;
static std::queue<std::string> g_msgQueue;
static const UINT WM_APP_UPDATE = WM_APP + 1;

//HOOK GetMessage 函数，这是GUI程序的核心功能
static LRESULT(WINAPI* TrueSendMessageW)(HWND, UINT, WPARAM, LPARAM) = SendMessageW;


static bool g_isRecording = false;                      // 新增：记录状态开关
static std::set<UINT> g_recordedMessages;               // 新增：存储唯一消息的集合
static std::mutex g_recordMutex;                        // 新增：保护 g_recordedMessages 的互斥锁
static LRESULT(WINAPI* TrueDispatchMessageW)(const MSG* lpMsg) = DispatchMessageW;
static std::vector<FullMessageRecord> g_messageLog;
static std::mutex g_logMutex;

//下面是针对跳板的安装
// 1. 定义我们目标函数的原型 (函数签名)
//typedef void(__fastcall* PFN_SECURITY_CHECK_COOKIE)(uintptr_t);
typedef int64_t(__fastcall* PFN_DWG_TARGET_FUNC)(int64_t a1, int a2);


// 2. 创建一个全局指针，用于保存原始函数的地址 (Detours会填充它)
//static PFN_SECURITY_CHECK_COOKIE True_SecurityCheckCookie = NULL;
static PFN_DWG_TARGET_FUNC TrueDwgTargetFunc = NULL;


// 声明我们自己的恢复函数
void retore2checkpoint();


int64_t __fastcall DetourDwgTargetFunc(int64_t a1, int a2)
{
    // a. 在调用原始函数【之前】的操作
    //    记录传入的参数值
    std::stringstream ss;
    ss << "[+] Hooked 0x1E4490 Called!\n"
        << "    -> Arg1 (a1 - __int64): 0x" << std::hex << a1 << " (" << std::dec << a1 << ")\n"
        << "    -> Arg2 (a2 - int): " << std::dec << a2;
    AddLog(ss.str());

    // b. 调用原始函数，并保存返回值
    //    这是非常关键的一步，它保证了程序原有功能的正常运行
    AddLog("    -> Calling original function...");
    int64_t original_result = TrueDwgTargetFunc(a1, a2);
    AddLog("    <- Original function returned.");

    // c. 在调用原始函数【之后】的操作
    //    记录原始函数的返回值
    ss.str(""); // 清空 stringstream
    ss << "    -> Original function returned: 0x" << std::hex << original_result
        << " (" << std::dec << original_result << ")";
    AddLog(ss.str());

    // d. 返回结果 (您可以选择返回原始结果，或者修改它)
    AddLog("[+] Hooked function finished.");
    retore2checkpoint();
    return original_result;
}

void InstallDwgHook()
{
    const DWORD64 HOOK_RVA = 0x1E4490;

    HMODULE hModule = GetModuleHandle(NULL);
    if (!hModule) {
        AddLog("[Error] GetModuleHandle(NULL) failed! Cannot find dwgviewr.exe module.");
        return;
    }

    PVOID pTarget = (PVOID)((uintptr_t)hModule + HOOK_RVA);

    TrueDwgTargetFunc = (PFN_DWG_TARGET_FUNC)pTarget;

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(&(PVOID&)TrueDwgTargetFunc, DetourDwgTargetFunc);

    LONG error = DetourTransactionCommit();
    if (error == NO_ERROR) {
        std::stringstream ss;
        ss << "[+] Hook attached successfully to dwgviewr.exe + 0x1E4490 (Address: 0x" << std::hex << (uintptr_t)pTarget << ")";
        AddLog(ss.str());
    }
    else {
        AddLog("[Error] Hook attach failed with code: " + std::to_string(error));
    }
}

// 5. 编写卸载 Hook 的函数
void UninstallDwgHook()
{
    if (TrueDwgTargetFunc == NULL) return;
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourDetach(&(PVOID&)TrueDwgTargetFunc, DetourDwgTargetFunc);
    DetourTransactionCommit();
    AddLog("[-] Hook at 0x1E4490 detached.");
}

// 6. 用于在新线程中安全执行安装的函数
DWORD WINAPI HookInstallationThread(LPVOID lpParam)
{
    Sleep(2000);
    InstallDwgHook();
    return 0;
}
//void __fastcall Detour_SecurityCheckCookie(uintptr_t StackCookie)
//{
//    
//    
//}
//
//// 4. 编写安装和卸载Hook的函数
//void InstallDetoursHook()
//{
//    // 目标函数的RVA
//    const DWORD64 HOOK_RVA = 0x393FA0;
//    // 计算目标函数的绝对地址
//    PVOID pTarget = (PVOID)((uintptr_t)GetModuleHandleA(NULL) + HOOK_RVA);
//
//    // ★ Detours的关键步骤：在Attach之前，必须将我们的跳板指针指向目标地址
//    True_SecurityCheckCookie = (PFN_SECURITY_CHECK_COOKIE)pTarget;
//
//    std::stringstream ss;
//    ss << std::hex << std::uppercase; // 设置为大写十六进制输出
//    ss << "[Detours] Attaching hook to target at 0x" << (uintptr_t)pTarget;
//    AddLog(ss.str());
//
//    // 开始一个“事务”来应用Hook
//    DetourTransactionBegin();
//    DetourUpdateThread(GetCurrentThread());
//
//    // 附加我们的Detour。
//    DetourAttach(&(PVOID&)True_SecurityCheckCookie, Detour_SecurityCheckCookie);
//
//    // 提交事务，应用内存补丁
//    LONG error = DetourTransactionCommit();
//    if (error == NO_ERROR) {
//        AddLog("[Detours] Hook attached successfully!");
//    }
//    else {
//        AddLog("[Detours Error] Attach failed with code: " + std::to_string(error));
//    }
//}
//
//void UninstallDetoursHook()
//{
//    AddLog("[Detours] Detaching hook...");
//    DetourTransactionBegin();
//    DetourUpdateThread(GetCurrentThread());
//    // 卸载我们的Detour
//    DetourDetach(&(PVOID&)True_SecurityCheckCookie, Detour_SecurityCheckCookie);
//    DetourTransactionCommit();
//}
//
//// 5. 用于在新线程中安全执行安装的函数
//DWORD WINAPI HookInstallationThread(LPVOID lpParam)
//{
//    Sleep(2000); // 等待2秒，让主程序和您的UI窗口充分初始化
//    InstallDetoursHook();
//    return 0;
//}


//hook 一下同步消息函数

void SendCustomMessage()
{
    char buffer[256];
    HWND targetHwnd = nullptr;
    UINT msgId = 0;
    WPARAM wParam = 0;
    LPARAM lParam = 0;

    try
    {
        // 1. Get HWND from the edit control
        GetDlgItemTextA(g_hMainWnd, IDC_EDIT_HWND, buffer, sizeof(buffer));
        if (strlen(buffer) == 0) {
            AddLog("[Error] HWND cannot be empty.");
            return;
        }
        targetHwnd = (HWND)std::stoull(buffer, nullptr, 16);

        // 2. Get Message ID
        GetDlgItemTextA(g_hMainWnd, IDC_EDIT_MSGID, buffer, sizeof(buffer));
        if (strlen(buffer) == 0) {
            AddLog("[Error] Message ID cannot be empty.");
            return;
        }
        msgId = std::stoul(buffer, nullptr, 16);

        // 3. Get wParam
        GetDlgItemTextA(g_hMainWnd, IDC_EDIT_WPARAM, buffer, sizeof(buffer));
        if (strlen(buffer) > 0) { // Allow empty for 0
            wParam = std::stoull(buffer, nullptr, 16);
        }

        // 4. Get lParam
        GetDlgItemTextA(g_hMainWnd, IDC_EDIT_LPARAM, buffer, sizeof(buffer));
        if (strlen(buffer) > 0) { // Allow empty for 0
            lParam = std::stoull(buffer, nullptr, 16);
        }
    }
    catch (const std::exception& e)
    {
        std::stringstream ss;
        ss << "[Error] Invalid input. Please enter valid hexadecimal numbers. Details: " << e.what();
        AddLog(ss.str());
        return;
    }

    // 5. Validate the handle and send the message
    if (!IsWindow(targetHwnd))
    {
        std::stringstream ss;
        ss << "[Error] The provided HWND (0x" << std::hex << std::uppercase << (UINT_PTR)targetHwnd << ") is not a valid window.";
        AddLog(ss.str());
        return;
    }

    // Use PostMessage - it's generally safer as it doesn't block.
    PostMessage(targetHwnd, msgId, wParam, lParam);

    std::stringstream ss;
    ss << "[Sent] Posted message to HWND 0x" << std::hex << std::uppercase
        << (UINT_PTR)targetHwnd << ": ID=0x" << msgId
        << ", wParam=0x" << wParam << ", lParam=0x" << lParam;
    AddLog(ss.str());
}

// *** 发送消息序列的核心功能函数 ***
void SendSequenceOfMessages() {
    char buffer[256];
    HWND targetHwnd = nullptr;

    // 1. 获取目标 HWND
    GetDlgItemTextA(g_hMainWnd, IDC_EDIT_HWND, buffer, sizeof(buffer));
    if (strlen(buffer) == 0) {
        AddLog("[Seq Error] Target HWND cannot be empty.");
        return;
    }
    try {
        targetHwnd = (HWND)std::stoull(buffer, nullptr, 16);
    }
    catch (const std::exception& e) {
        AddLog("[Seq Error] Invalid HWND format.");
        return;
    }

    if (!IsWindow(targetHwnd)) {
        std::stringstream ss;
        ss << "[Seq Error] The provided HWND (0x" << std::hex << std::uppercase << (UINT_PTR)targetHwnd << ") is not a valid window.";
        AddLog(ss.str());
        return;
    }

    // 2. 获取多行编辑框的全部内容
    int textLen = GetWindowTextLength(GetDlgItem(g_hMainWnd, IDC_EDIT_MSG_SEQUENCE));
    if (textLen == 0) {
        AddLog("[Seq Info] Message sequence is empty. Nothing to send.");
        return;
    }
    std::string allMessages;
    allMessages.resize(textLen + 1);
    GetDlgItemTextA(g_hMainWnd, IDC_EDIT_MSG_SEQUENCE, &allMessages[0], textLen + 1);
    allMessages.resize(textLen); // 调整大小以匹配实际长度

    std::stringstream ss(allMessages);
    std::string line;
    int line_num = 0;
    int sent_count = 0;

    AddLog("--- Starting Message Sequence ---");

    // 3. 逐行解析并发送消息
    while (std::getline(ss, line)) {
        line_num++;
        // 移除回车符，以防万一
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue; // 跳过空行
        }

        std::stringstream lineStream(line);
        std::string segment;
        std::vector<std::string> parts;

        // 按逗号分割
        while (std::getline(lineStream, segment, ',')) {
            // 去除前导/后导空格
            segment.erase(0, segment.find_first_not_of(" \t"));
            segment.erase(segment.find_last_not_of(" \t") + 1);
            parts.push_back(segment);
        }

        if (parts.size() != 3) {
            AddLog("[Seq Error] Line " + std::to_string(line_num) + ": Invalid format. Expected 'MsgID, wParam, lParam'. Skipping.");
            continue;
        }

        try {
            UINT msgId = std::stoul(parts[0], nullptr, 16);
            WPARAM wParam = std::stoull(parts[1], nullptr, 16);
            LPARAM lParam = std::stoull(parts[2], nullptr, 16);

            // 发送消息
            PostMessage(targetHwnd, msgId, wParam, lParam);

            std::stringstream log_ss;
            log_ss << "[Seq Sent] Line " << line_num << ": Posted to HWND 0x" << std::hex << std::uppercase
                << (UINT_PTR)targetHwnd << ": ID=0x" << msgId
                << ", wParam=0x" << wParam << ", lParam=0x" << lParam;
            AddLog(log_ss.str());
            sent_count++;

        }
        catch (const std::exception& e) {
            AddLog("[Seq Error] Line " + std::to_string(line_num) + ": Invalid hexadecimal value. Skipping.");
        }
    }
    AddLog("--- Message Sequence Finished. Sent " + std::to_string(sent_count) + " messages. ---");
}


void SaveRecordedMessagesToFile() {
    std::lock_guard<std::mutex> lock(g_logMutex); // 锁定资源
    if (g_messageLog.empty()) {
        AddLog("No messages were recorded.\n");
        return;
    }

    std::ofstream logFile("E:\\microKernel\\HookReadFileDLL\\fullmessagelog.txt");
    if (!logFile.is_open()) {
        AddLog("Error: Failed to open full_message_log.txt for writing.\n");
        return;
    }

    logFile << "--- Full Message Log ---\n";
    logFile << "Total messages recorded: " << g_messageLog.size() << "\n\n";

    // 设置流格式以输出十六进制
    logFile << std::hex << std::uppercase;

    // 遍历所有记录并格式化输出
    for (const auto& record : g_messageLog) {
        // 格式化时间戳 HH:MM:SS.ms
        logFile << "[" << std::setw(2) << record.timestamp.wHour << ":"
            << std::setw(2) << record.timestamp.wMinute << ":"
            << std::setw(2) << record.timestamp.wSecond << "."
            << std::setw(3) << record.timestamp.wMilliseconds << "]";

        // 线程ID
        logFile << "[TID: " << std::setw(4) << record.threadId << "] ";

        // 窗口句柄
        logFile << "HWND: 0x" << std::setw(8) << (UINT_PTR)record.msg.hwnd << " | ";

        // 消息名和ID
        logFile << "Msg: " << std::left << std::setw(20) << MessageIDToString(record.msg.message)
            << std::right << "(0x" << std::setw(4) << record.msg.message << ") | ";

        // 参数
        logFile << "wParam: 0x" << std::setw(8) << record.msg.wParam << " | ";
        logFile << "lParam: 0x" << std::setw(8) << record.msg.lParam << "\n";
    }

    logFile.close();

    std::string final_msg = "Successfully saved " + std::to_string(g_messageLog.size()) + " messages to full_message_log.txt\n";
    AddLog(final_msg);
}

LRESULT WINAPI MySendMessageW(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    // 只有在“记录中”状态，并且消息是“用户操作消息”时，才执行记录
    if (g_isRecording && Msg && IsUserActionMessage(Msg)) { // <-- 在这里添加过滤器调用

        // (下面的记录逻辑保持不变)
        FullMessageRecord record;
        record.msg.message = Msg;
        record.msg.hwnd = hWnd;
        record.msg.lParam = lParam;
        record.msg.wParam = wParam;
        record.threadId = GetCurrentThreadId();
        GetLocalTime(&record.timestamp);

        {
            std::lock_guard<std::mutex> lock(g_logMutex);
            g_messageLog.push_back(record);
        }
    }

    // ★★★ 必须调用原始的SendMessageW函数
    return TrueSendMessageW(hWnd, Msg, wParam, lParam);
}

LRESULT WINAPI MyDispatchMessageW(const MSG* lpMsg) {
    // 只有在“记录中”状态，并且消息是“用户操作消息”时，才执行记录
    if (g_isRecording && lpMsg && IsUserActionMessage(lpMsg->message)) { // <-- 在这里添加过滤器调用

        // (下面的记录逻辑保持不变)
        FullMessageRecord record;
        record.msg = *lpMsg;
        record.threadId = GetCurrentThreadId();
        GetLocalTime(&record.timestamp);

        {
            std::lock_guard<std::mutex> lock(g_logMutex);
            g_messageLog.push_back(record);
        }
    }

    // 必须调用原始函数
    return TrueDispatchMessageW(lpMsg);
}

void RecordMsg() {
    g_isRecording = !g_isRecording;

    if (g_isRecording) {
        // 开始记录
        {
            std::lock_guard<std::mutex> lock(g_logMutex);
            g_messageLog.clear(); // 修改：清空新的vector
        }
        AddLog("Full message recording started...\n");
        if (g_hMainWnd) {
            SetDlgItemTextA(g_hMainWnd, IDC_BTB_RECORDMSG, "Stop Recording");
        }
    }
    else {
        // 停止记录
        AddLog("Full message recording stopped.\n");
        if (g_hMainWnd) {
            SetDlgItemTextA(g_hMainWnd, IDC_BTB_RECORDMSG, "Record Messages");
        }
        SaveRecordedMessagesToFile();
    }
}


//记录全局变量，data段和bss段
void recordGlobalVar(GlobalsSnapshot& snapshot) {
    std::stringstream log_stream;

    snapshot.regions.clear();
    snapshot.savedData.clear();

    snapshot.regions = find_global_data_regions();
    if (snapshot.regions.empty()) {
        log_stream << "错误: 未能找到任何全局数据节区 (.data, .bss, .rdata)。" << std::endl;
        AddLog(log_stream.str());
        return;
    }

    snapshot.savedData.resize(snapshot.regions.size());
    size_t totalBytesCopied = 0;

    // --- 定义一个合理的最大节区大小，例如 256MB ---
    const SIZE_T MAX_REASONABLE_SECTION_SIZE = 256 * 1024 * 1024;

    for (size_t i = 0; i < snapshot.regions.size(); ++i) {
        const auto& region = snapshot.regions[i];

        // **增加大小检查**
        if (region.size == 0) {
            continue; // 跳过空节区
        }
        if (region.size > MAX_REASONABLE_SECTION_SIZE) {
            log_stream << "警告: 节区 '" << region.name << "' 的大小 (" << region.size
                << " 字节) 超出限制，已跳过。" << std::endl;
            continue; // 跳过超大节区
        }

        try {
            // **将内存分配放在 try 块中**
            snapshot.savedData[i].resize(region.size);

            // ... (后面是和上次一样的安全内存复制逻辑) ...
            char* pCurrentSource = (char*)region.startAddress;
            char* pCurrentDest = snapshot.savedData[i].data();
            SIZE_T remainingSize = region.size;

            while (remainingSize > 0) {
                MEMORY_BASIC_INFORMATION mbi;
                if (VirtualQuery(pCurrentSource, &mbi, sizeof(mbi)) == 0) {
                    break;
                }

                bool isReadable = (mbi.State == MEM_COMMIT) &&
                    (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE));

                SIZE_T bytesInThisBlock =min(remainingSize, (SIZE_T)((char*)mbi.BaseAddress + mbi.RegionSize - pCurrentSource));

                if (isReadable) {
                    memcpy(pCurrentDest, pCurrentSource, bytesInThisBlock);
                    totalBytesCopied += bytesInThisBlock;
                }
                else {
                    memset(pCurrentDest, 0, bytesInThisBlock);
                }

                pCurrentSource += bytesInThisBlock;
                pCurrentDest += bytesInThisBlock;
                remainingSize -= bytesInThisBlock;
            }

        }
        catch (const std::bad_alloc& e) {
            // **捕获内存分配异常**
            log_stream << "严重错误: 为节区 '" << region.name << "' 分配 " << region.size
                << " 字节内存失败! 原因: " << e.what() << std::endl;
            // 清理这个失败的条目，以保持数据一致性
            snapshot.savedData[i].clear();
            snapshot.savedData[i].shrink_to_fit(); // 释放内存
        }
        catch (const std::exception& e) {
            // 捕获其他可能的标准异常
            log_stream << "严重错误: 处理节区 '" << region.name << "' 时发生未知异常: " << e.what() << std::endl;
        }
    }

    log_stream << "快照捕获完成，共处理 " << snapshot.regions.size() << " 个区域, 成功复制了 " << totalBytesCopied << " 字节。" << std::endl;
    AddLog(log_stream.str());
}


//把新增的malloc添加到chunck里面，如果free掉了，也同样需要从chunk list删掉
void MonitorHeap2Chunck() {

}


//记录当前线程的id
std::vector<uint64_t> recordThreads() {


    std::vector<uint64_t> threadIds;

    // 1. 获取当前进程ID
    DWORD currentProcessId = GetCurrentProcessId();

    // 2. 创建系统线程快照
    HANDLE hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (hThreadSnap == INVALID_HANDLE_VALUE) {
        // 创建快照失败
        return threadIds;
    }

    // 3. 准备遍历
    THREADENTRY32 te32;
    te32.dwSize = sizeof(THREADENTRY32); // 在调用Thread32First之前必须设置此成员

    // 4. 获取第一个线程的信息
    if (!Thread32First(hThreadSnap, &te32)) {
        CloseHandle(hThreadSnap); // 出错，关闭句柄
        return threadIds;
    }

    // 5. 遍历所有线程
    do {
        // 检查此线程是否属于当前进程
        if (te32.th32OwnerProcessID == currentProcessId) {
            threadIds.push_back(static_cast<uint64_t>(te32.th32ThreadID));
        }
    } while (Thread32Next(hThreadSnap, &te32));

    // 6. 清理并关闭快照句柄
    CloseHandle(hThreadSnap);

    return threadIds;
}

//记录属于当前进程的当前窗口的窗口句柄,通过遍历该进程的线程获得它的所有窗口包括子窗口
/**
 * @brief 记录当前进程所拥有的所有窗口句柄，包括所有顶层窗口和它们的子窗口。
 * @return std::vector<HWND> 包含所有窗口句柄的列表。
 */
std::vector<HWND> recordWindows() {

    std::set<HWND> windowHandlesSet;

    // 准备传递给回调函数的数据
    EnumData data;
    data.processId = GetCurrentProcessId(); // 获取当前进程ID
    data.pHandles = &windowHandlesSet;

    // 开始枚举屏幕上所有的顶层窗口
    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&data));

    // 将set中的结果复制到vector中返回
    return std::vector<HWND>(windowHandlesSet.begin(), windowHandlesSet.end());
}


//直接根据之前的全局变量的内存地址和数据直接恢复全局变量
void restoreGlobalVar(const GlobalsSnapshot& snapshot) {
    std::stringstream log_stream;

    if (!snapshot.isValid()) {
        log_stream << "错误: 快照数据无效，无法恢复。" << std::endl;
        AddLog(log_stream.str());
        return;
    }

    // 遍历快照中的每个区域并恢复数据
    for (size_t i = 0; i < snapshot.regions.size(); ++i) {
        const auto& region = snapshot.regions[i];
        const auto& data = snapshot.savedData[i];

        // 检查数据有效性，防止意外
        if (data.empty() || region.size == 0 || region.size != data.size()) {
            continue;
        }

        // --- 核心修改：使用 VirtualProtect ---

        // 1. 保存旧的内存保护属性
        DWORD oldProtect;

        // 2. 临时将内存区域设置为可读写。
        //    使用 PAGE_EXECUTE_READWRITE 更具通用性，因为它对代码和数据节区都有效。
        BOOL success = VirtualProtect(
            region.startAddress,      // 目标地址
            region.size,              // 区域大小
            PAGE_EXECUTE_READWRITE,   // 新的保护属性：可执行、可读、可写
            &oldProtect               // 用于接收旧保护属性的变量地址
        );

        if (success) {
            // 3. 只有在成功修改属性后，才执行写入操作
            memcpy(region.startAddress, data.data(), region.size);

            // 4. (关键步骤) 将内存保护属性恢复原状
            DWORD tempProtect; // VirtualProtect 需要一个地址，即使我们不关心第二次调用的“旧”属性
            VirtualProtect(
                region.startAddress,
                region.size,
                oldProtect, // 恢复原始的保护属性
                &tempProtect
            );
        }
        else {
            // 如果修改保护属性失败，记录错误
            log_stream << "错误: 无法修改内存保护属性以恢复节区 '" << region.name
                << "'。错误码: " << GetLastError() << std::endl;
        }
    }

    log_stream << "从快照恢复数据完成。" << std::endl;
    AddLog(log_stream.str());
}

//把仍然存在chunk list的数据给free掉  这一步需要在kill线程和close窗口之后执行，因为如果先执行的话，可能导致double free的问题，而且在执行的前一刻才会停止对heap的监控
void freeHeapChunk() {


}

//杀掉线程  这一步需要在 closeWindows之前执行，因为一个线程可能控制多个窗口，当你直接杀掉线程之后，有些窗口其实没有必要再关闭了
void killNewThreads(const std::vector<uint64_t>& initial_thread_ids) {
    if (initial_thread_ids.empty()) {
        AddLog("Error: Initial thread vector is empty; cannot determine new threads.\n");
        return;
    }

    AddLog("--- Starting termination of new threads ---\n");

    // Get the ID of the current thread to ensure we don't terminate it.
    uint64_t self_thread_id = GetCurrentThreadId();

    // Get the vector of all current threads in this process.
    std::vector<uint64_t> current_threads = recordThreads();

    // Find and terminate new threads.
    for (uint64_t thread_id : current_threads) {
        // Use std::find to check if the thread_id exists in the initial vector.
        bool is_initial_thread = (std::find(initial_thread_ids.begin(), initial_thread_ids.end(), thread_id) != initial_thread_ids.end());

        // A thread is "new" if it's not in the initial vector.
        // Also, we must not kill the thread that is executing this function.
        if (!is_initial_thread && thread_id != self_thread_id) {
            std::stringstream log_stream;
            log_stream << "Detected new thread ID: " << thread_id << ". Attempting to terminate...\n";

            // 1. Open a handle to the thread with termination rights.
            HANDLE hThread = OpenThread(THREAD_TERMINATE, FALSE, static_cast<DWORD>(thread_id));

            if (hThread != NULL) {
                // 2. Attempt to forcefully terminate the thread.
                if (TerminateThread(hThread, 1)) { // Use a non-zero exit code
                    log_stream << "  > Success: Thread " << thread_id << " has been terminated.\n";
                }
                else {
                    log_stream << "  > Error: Failed to terminate thread " << thread_id
                        << ". Win32 Error Code: " << GetLastError() << "\n";
                }
                // 3. Close the handle to prevent resource leaks.
                CloseHandle(hThread);
            }
            else {
                log_stream << "  > Error: Failed to open a handle for thread " << thread_id
                    << ". Win32 Error Code: " << GetLastError() << "\n";
            }
            AddLog(log_stream.str());
        }
    }

    AddLog("--- Termination of new threads complete ---\n");
}


//关闭属于这个进程或者这个进程创建的窗口，可以通过获取现在的窗口和之前的窗口取一个差集，然后把这些新增的给关闭了
/// <summary>
/// 此函数会发送关闭请求，然后进入一个带延时和重试的循环来验证窗口是否真正关闭，
/// 以处理目标程序响应慢和消息异步的问题。
/// </summary>
/// <param name="initial_handles">程序某个稳定状态时记录的初始窗口句柄列表。</param>
/// <returns>1 表示成功（所有新窗口都被关闭或没有新窗口），0 表示失败（尝试关闭后仍有窗口存在）。</returns>
int closeNewWindows(const std::vector<HWND>& initial_handles) {
    // --- 在函数开头定义配置常量，方便调整 ---
    // 重试次数，可以根据需要增加
    const int VERIFICATION_RETRY_COUNT = 20;
    // 每次重试的间隔时间(毫秒)，如果程序关闭慢，可以适当增加这个值
    const int VERIFICATION_SLEEP_INTERVAL_MS = 250;
    // 总等待时间 = 20 * 250ms = 5000ms = 5秒

    std::stringstream log_stream;
    log_stream << "--- Starting smart closing of new windows ---\n";
    log_stream << "    (Verification Timeout: " << VERIFICATION_RETRY_COUNT * VERIFICATION_SLEEP_INTERVAL_MS << "ms)\n";

    // 1. 获取初始窗口集合和当前所有窗口列表
    std::set<HWND> initial_set(initial_handles.begin(), initial_handles.end());
    std::vector<HWND> current_windows = recordWindows();

    // 2. 筛选出所有新窗口
    std::vector<HWND> new_windows_list;
    std::set<HWND> new_windows_set;
    for (HWND hwnd : current_windows) {
        // 使用 std::find 在 vector 中查找，对于无序集合，这样也可以，但 set 更高效
        // 这里我们用 set 来进行查找
        if (initial_set.find(hwnd) == initial_set.end()) {
            // 确保窗口句柄在添加到列表前仍然有效
            if (IsWindow(hwnd)) {
                new_windows_list.push_back(hwnd);
                new_windows_set.insert(hwnd);
            }
        }
    }

    // 如果没有发现新窗口，任务已经“成功”完成
    if (new_windows_list.empty()) {
        log_stream << "No new windows found. Operation successful.\n";
        AddLog(log_stream.str());
        return 1; // 成功
    }

    // 3. 找出所有“新窗口树的根”
    std::vector<HWND> windows_to_close;
    for (HWND hwnd : new_windows_list) {
        HWND parent = GetParent(hwnd);
        // 如果一个新窗口的父窗口不是新窗口，或者它没有父窗口，那么它就是一个根
        if (parent == NULL || new_windows_set.find(parent) == new_windows_set.end()) {
            windows_to_close.push_back(hwnd);
        }
    }

    // 如果过滤后没有需要关闭的根窗口（例如所有新窗口都是彼此的子窗口，但根已关闭），也视为成功
    if (windows_to_close.empty()) {
        log_stream << "No new window tree roots were found to close. Operation successful.\n";
        AddLog(log_stream.str());
        return 1; // 成功
    }

    // 只对这些“根”窗口发送异步关闭指令
    log_stream << "Identified " << windows_to_close.size()
        << " new window tree root(s) to close...\n";
    for (HWND hwnd : windows_to_close) {
        log_stream << "  > Posting WM_CLOSE to root window: " << hwnd << "\n";
        PostMessage(hwnd, WM_CLOSE, 0, 0); // 使用PostMessage避免阻塞
    }

    // 等待并反复检查，直到所有目标窗口都消失
    log_stream << "--- Verifying window closure ---\n";
    bool all_closed_successfully = false;
    for (int i = 0; i < VERIFICATION_RETRY_COUNT; ++i) {
        // 等待一小段时间，给目标程序处理消息的时间
        Sleep(VERIFICATION_SLEEP_INTERVAL_MS);

        bool all_gone_this_check = true;
        for (HWND hwnd : windows_to_close) {
            // IsWindow() 是我们确认状态的唯一标准
            if (IsWindow(hwnd)) {
                // 只要还有一个窗口存在，本次检查就没通过
                all_gone_this_check = false;
                break; // 无需再检查此轮的其他窗口
            }
        }

        if (all_gone_this_check) {
            // 如果这一轮检查下来，所有目标窗口都没了，说明关闭成功
            all_closed_successfully = true;
            log_stream << "Verification check PASSED after " << (i + 1) * VERIFICATION_SLEEP_INTERVAL_MS << "ms.\n";
            break; // 成功，提前退出等待循环
        }
    }

    // 根据最终的验证结果返回状态码
    if (all_closed_successfully) {
        log_stream << "Final Result: SUCCESS. All targeted new windows have been closed.\n";
        AddLog(log_stream.str());
        return 1; // 
    }
    else {
        log_stream << "Final Result: FAILED. Some windows did not close within the timeout.\n";
        // 记录下哪些窗口未能关闭
        for (HWND hwnd : windows_to_close) {
            if (IsWindow(hwnd)) {
                log_stream << "  > Lingering window handle that failed to close: " << hwnd << "\n";
            }
        }
        AddLog(log_stream.str());
        return 0; // 返回失败
    }
}

//全局变量
GlobalsSnapshot gs;
std::vector<uint64_t> g_threads;
std::vector<HWND> g_windows;
// checkpoint 函数  快照记录的开始点  这里会记录全局变量，堆，线程和窗口的变化。
void checkpoint() {

    g_windows=recordWindows();

}

//恢复到checkpoint之前的状态
static std::atomic<bool> g_isRestoring = false;

// 工作线程函数
DWORD WINAPI RestoreWorkerThread(LPVOID lpParam) {
    // 接收并转换时间指针
    using HighResClock = std::chrono::high_resolution_clock;
    auto pStartTime = static_cast<HighResClock::time_point*>(lpParam);

    // 健壮性检查：如果收到的指针是空的，则直接退出
    if (pStartTime == nullptr) {
        AddLog("[Worker Error] Received a null start time parameter. Aborting.");
        g_isRestoring = false;
        return 1; // 异常退出
    }

    AddLog("==> Worker thread started. Beginning restore sequence...");

    // 
    int close_result = closeNewWindows(g_windows);
    if (close_result == 1) {
        AddLog("[Worker] Window closure successful. Proceeding to send open message...");
        sendOpenMessage();
    }
    else {
        AddLog("[Worker] Window closure FAILED. Loop interrupted.");
    }
    // 

    // 记录结束时间并计算差值
    auto endTime = HighResClock::now();
    // 计算从开始到结束的时间差
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - *pStartTime);

    // 打印耗时日志
    std::stringstream ss;
    ss << "[TIMING] Full close/open cycle took: " << duration.count() << " ms.";
    AddLog(ss.str());

    AddLog("<== Worker thread finished its task.");

    // 释放传递过来的时间对象内存，防止内存泄漏
    delete pStartTime;

    g_isRestoring = false; // 任务完成，解锁
    return 0;
}
// 恢复到checkpoint之前的状态
void retore2checkpoint() {
    if (g_isRestoring.load()) {
        AddLog("[retore2checkpoint] Restore already in progress. Ignoring.");
        return;
    }

    g_isRestoring = true;

    // 记录开始时间
    // 定义高精度时钟类型别名，方便使用
    using HighResClock = std::chrono::high_resolution_clock;
    // 在堆上动态分配一个时间点对象，以便将其指针传递给新线程
    auto pStartTime = new HighResClock::time_point(HighResClock::now());

    AddLog("[retore2checkpoint] Kicking off worker thread, timer started...");

    // 将开始时间指针作为参数(lpParam)传递 
    HANDLE hThread = CreateThread(NULL, 0, RestoreWorkerThread, pStartTime, 0, NULL);

    if (hThread) {
        CloseHandle(hThread);
    }
    else {
        AddLog("[ERROR] Failed to create RestoreWorkerThread!");
        // 如果线程创建失败，必须释放我们刚刚分配的内存，防止泄漏
        delete pStartTime;
        g_isRestoring = false;
    }
}


// 重放message的空函数
void ReplayMessageFunc()
{

    AddLog("[Debug]: Check Point . \n");



    checkpoint();


}

// 快照恢复的空函数
void RestoreSnapshotFunc()
{


    AddLog("[Debug]: Restore  .\n");


    //帮当前状态恢复的到执行前
    retore2checkpoint();

}



LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        g_hEdit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
            0, 0, 0, 0, hWnd, (HMENU)IDC_EDIT_LOG, g_hInstance, nullptr);

        // --- 新增：消息序列控件 ---
        CreateWindowA("STATIC", "Message Sequence (MsgID, wParam, lParam per line, all in Hex):", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hWnd, (HMENU)IDC_STATIC_SEQ_INFO, g_hInstance, NULL);
        CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "0100,0041,001E0001\r\n0102,0041,001E0001\r\n0101,0041,C01E0001", // 默认示例
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL,
            0, 0, 0, 0, hWnd, (HMENU)IDC_EDIT_MSG_SEQUENCE, g_hInstance, nullptr);
        CreateWindowA("BUTTON", "Send Sequence", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, hWnd, (HMENU)IDC_BTN_SEND_SEQUENCE, g_hInstance, nullptr);


        // --- 单条消息发送控件 ---
        CreateWindowA("STATIC", "Target HWND:", WS_CHILD | WS_VISIBLE | SS_RIGHT, 0, 0, 0, 0, hWnd, NULL, g_hInstance, NULL);
        CreateWindowA("STATIC", "Msg ID:", WS_CHILD | WS_VISIBLE | SS_RIGHT, 0, 0, 0, 0, hWnd, NULL, g_hInstance, NULL);
        CreateWindowA("STATIC", "wParam:", WS_CHILD | WS_VISIBLE | SS_RIGHT, 0, 0, 0, 0, hWnd, NULL, g_hInstance, NULL);
        CreateWindowA("STATIC", "lParam:", WS_CHILD | WS_VISIBLE | SS_RIGHT, 0, 0, 0, 0, hWnd, NULL, g_hInstance, NULL);
        CreateWindowA("STATIC", "(All values in Hex)", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hWnd, (HMENU)IDC_STATIC_INFO, g_hInstance, NULL);

        CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0, 0, 0, hWnd, (HMENU)IDC_EDIT_HWND, g_hInstance, NULL);
        CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0, 0, 0, hWnd, (HMENU)IDC_EDIT_MSGID, g_hInstance, NULL);
        CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0, 0, 0, hWnd, (HMENU)IDC_EDIT_WPARAM, g_hInstance, NULL);
        CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0, 0, 0, hWnd, (HMENU)IDC_EDIT_LPARAM, g_hInstance, NULL);
        CreateWindowA("BUTTON", "Send Single Msg", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, hWnd, (HMENU)IDC_BTN_SENDMSG, g_hInstance, nullptr);

        // --- 底部主功能按钮 ---
        CreateWindowA("BUTTON", "Clear Log", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, hWnd, (HMENU)IDC_BTN_CLEAR, g_hInstance, nullptr);
        CreateWindowA("BUTTON", "Checkpoint", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, hWnd, (HMENU)IDC_BTN_REPLAY, g_hInstance, nullptr);
        CreateWindowA("BUTTON", "Restore", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, hWnd, (HMENU)IDC_BTN_RESTORE, g_hInstance, nullptr);
        CreateWindowA("BUTTON", "Record Messages", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, hWnd, (HMENU)IDC_BTB_RECORDMSG, g_hInstance, nullptr);

        // --- 统一设置字体 ---
        HFONT hFont = CreateFont(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Arial");

        // 遍历所有控件设置字体
        for (int i = IDC_EDIT_LOG; i <= IDC_BTN_SEND_SEQUENCE; ++i) { // 扩展范围以包含新控件
            HWND hItem = GetDlgItem(hWnd, i);
            if (hItem) SendMessage(hItem, WM_SETFONT, (WPARAM)hFont, TRUE);
        }
        HWND hStatic = NULL;
        while (hStatic = FindWindowExA(hWnd, hStatic, "STATIC", NULL)) {
            SendMessage(hStatic, WM_SETFONT, (WPARAM)hFont, TRUE);
        }

        RECT rcClient;
        GetClientRect(hWnd, &rcClient);
        SendMessage(hWnd, WM_SIZE, 0, MAKELPARAM(rcClient.right, rcClient.bottom));

        return 0;
    }
    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDC_BTN_CLEAR:
            SetWindowTextA(g_hEdit, "");
            return 0;
        case IDC_BTN_REPLAY:
            ReplayMessageFunc();
            return 0;
        case IDC_BTN_RESTORE:
            RestoreSnapshotFunc();
            return 0;
        case IDC_BTB_RECORDMSG:
            RecordMsg();
            return 0;
        case IDC_BTN_SENDMSG:
            SendCustomMessage();
            return 0;
            // *** 新增：处理“发送序列”按钮点击事件 ***
        case IDC_BTN_SEND_SEQUENCE:
            SendSequenceOfMessages();
            return 0;
        }
        break;
    }
    case WM_SIZE:
    {
        // *** 修改：重构UI布局以容纳新控件 ***
        int windowWidth = LOWORD(lParam);
        int windowHeight = HIWORD(lParam);

        const int PADDING = 10;
        const int ROW_SPACING = 8;
        const int BUTTON_HEIGHT = 30;
        const int INPUT_HEIGHT = 22;
        const int LABEL_WIDTH = 80;

        int y = PADDING;

        // 底部主功能按钮行
        int bottom_buttons_y = windowHeight - BUTTON_HEIGHT - PADDING;

        // 单条消息发送行
        int single_msg_y = bottom_buttons_y - INPUT_HEIGHT - ROW_SPACING;

        // 消息序列控件区域
        int seq_controls_y = single_msg_y - ROW_SPACING; // 序列控件在此之上

        // --- 1. 日志窗口 (最上方) ---
        int log_height = seq_controls_y - PADDING - 110; // 110是为序列编辑框留出的高度
        if (g_hEdit) {
            SetWindowPos(g_hEdit, nullptr, PADDING, y, windowWidth - (PADDING * 2), log_height, SWP_NOZORDER);
        }
        y += log_height + ROW_SPACING;

        // --- 2. 消息序列区域 ---
        SetWindowPos(GetDlgItem(hWnd, IDC_STATIC_SEQ_INFO), nullptr, PADDING, y, 400, 15, SWP_NOZORDER);
        y += 18;
        int seq_edit_width = windowWidth - PADDING * 3 - 120; // 120 for button
        SetWindowPos(GetDlgItem(hWnd, IDC_EDIT_MSG_SEQUENCE), nullptr, PADDING, y, seq_edit_width, 90, SWP_NOZORDER);
        SetWindowPos(GetDlgItem(hWnd, IDC_BTN_SEND_SEQUENCE), nullptr, PADDING + seq_edit_width + PADDING, y, 120, BUTTON_HEIGHT, SWP_NOZORDER);
        y += 90 + ROW_SPACING;


        // --- 3. 单条消息发送区域 ---
        SetWindowPos(GetDlgItem(hWnd, IDC_STATIC_INFO), nullptr, PADDING, single_msg_y - 18, 120, 15, SWP_NOZORDER);

        int currentX = PADDING;
        // HWND
        SetWindowPos(FindWindowExA(hWnd, NULL, "STATIC", "Target HWND:"), nullptr, currentX, single_msg_y + 2, LABEL_WIDTH, INPUT_HEIGHT, SWP_NOZORDER);
        currentX += LABEL_WIDTH;
        SetWindowPos(GetDlgItem(hWnd, IDC_EDIT_HWND), nullptr, currentX, single_msg_y, 100, INPUT_HEIGHT, SWP_NOZORDER);
        currentX += 100 + PADDING;
        // Msg ID
        SetWindowPos(FindWindowExA(hWnd, NULL, "STATIC", "Msg ID:"), nullptr, currentX, single_msg_y + 2, 55, INPUT_HEIGHT, SWP_NOZORDER);
        currentX += 55;
        SetWindowPos(GetDlgItem(hWnd, IDC_EDIT_MSGID), nullptr, currentX, single_msg_y, 80, INPUT_HEIGHT, SWP_NOZORDER);
        currentX += 80 + PADDING;
        // wParam
        SetWindowPos(FindWindowExA(hWnd, NULL, "STATIC", "wParam:"), nullptr, currentX, single_msg_y + 2, 55, INPUT_HEIGHT, SWP_NOZORDER);
        currentX += 55;
        SetWindowPos(GetDlgItem(hWnd, IDC_EDIT_WPARAM), nullptr, currentX, single_msg_y, 120, INPUT_HEIGHT, SWP_NOZORDER);
        currentX += 120 + PADDING;
        // lParam
        SetWindowPos(FindWindowExA(hWnd, NULL, "STATIC", "lParam:"), nullptr, currentX, single_msg_y + 2, 55, INPUT_HEIGHT, SWP_NOZORDER);
        currentX += 55;
        SetWindowPos(GetDlgItem(hWnd, IDC_EDIT_LPARAM), nullptr, currentX, single_msg_y, 120, INPUT_HEIGHT, SWP_NOZORDER);
        currentX += 120 + PADDING;
        // Send Button
        int send_btn_width = max(120, windowWidth - currentX - PADDING);
        SetWindowPos(GetDlgItem(hWnd, IDC_BTN_SENDMSG), nullptr, currentX, single_msg_y, send_btn_width, INPUT_HEIGHT + 2, SWP_NOZORDER);

        // --- 4. 底部主功能按钮 ---
        currentX = PADDING;
        const int BTN_WIDTH = 110;
        SetWindowPos(GetDlgItem(hWnd, IDC_BTN_CLEAR), nullptr, currentX, bottom_buttons_y, BTN_WIDTH, BUTTON_HEIGHT, SWP_NOZORDER);
        currentX += BTN_WIDTH + PADDING;
        SetWindowPos(GetDlgItem(hWnd, IDC_BTN_REPLAY), nullptr, currentX, bottom_buttons_y, BTN_WIDTH, BUTTON_HEIGHT, SWP_NOZORDER);
        currentX += BTN_WIDTH + PADDING;
        SetWindowPos(GetDlgItem(hWnd, IDC_BTN_RESTORE), nullptr, currentX, bottom_buttons_y, BTN_WIDTH, BUTTON_HEIGHT, SWP_NOZORDER);
        currentX += BTN_WIDTH + PADDING;
        SetWindowPos(GetDlgItem(hWnd, IDC_BTB_RECORDMSG), nullptr, currentX, bottom_buttons_y, 140, BUTTON_HEIGHT, SWP_NOZORDER);

        return 0;
    }
    case WM_APP_UPDATE:
    {
        std::lock_guard<std::mutex> lock(g_msgMutex);
        while (!g_msgQueue.empty())
        {
            auto& msgStr = g_msgQueue.front();
            int nLen = GetWindowTextLength(g_hEdit);
            SendMessage(g_hEdit, EM_SETSEL, (WPARAM)nLen, (LPARAM)nLen);
            SendMessageA(g_hEdit, EM_REPLACESEL, FALSE, (LPARAM)(msgStr + "\r\n").c_str());
            g_msgQueue.pop();
        }
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}

// 窗口线程
DWORD WINAPI WindowThread(LPVOID) {
    // 注册窗口类
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = g_hInstance;
    wc.lpszClassName = L"DWGMonitorClass"; // Unicode类名
    RegisterClassExW(&wc);

    // 创建窗口
    g_hMainWnd = CreateWindowExW(
        WS_EX_TOPMOST,
        L"DWGMonitorClass",
        L"DWG监控器", // 正常显示中文标题
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT,
        1200, 700,
        nullptr, nullptr, g_hInstance, nullptr
    );

    // 消息循环
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // 清理
    {
        std::lock_guard<std::mutex> lock(g_msgMutex);
        g_hMainWnd = nullptr;
        g_hEdit = nullptr;
    }
    return 0;
}

// 添加日志
void AddLog(const std::string& msg)
{
    {
        std::lock_guard<std::mutex> lock(g_msgMutex);
        g_msgQueue.push(msg);
    }
    if (g_hMainWnd) {
        PostMessageA(g_hMainWnd, WM_APP_UPDATE, 0, 0);
    }
}

static pid_t pid = -1;

// ReadFile钩子
static BOOL(WINAPI* TrueReadFile)(HANDLE, LPVOID, DWORD, LPDWORD, LPOVERLAPPED) = ReadFile;

BOOL WINAPI HookedReadFile(
    HANDLE hFile, LPVOID lpBuffer, DWORD nToRead,
    LPDWORD lpRead, LPOVERLAPPED lpOverlapped)
{
    BOOL ret = TrueReadFile(hFile, lpBuffer, nToRead, lpRead, lpOverlapped);

    if (ret && lpRead && *lpRead >= 6)
    {
        const BYTE* data = static_cast<BYTE*>(lpBuffer);
        if (memcmp(data, "\x41\x43\x31\x30\x32\x31\x00\x00\x00\x00\x00", 11) == 0)
        {
            char buf[256];
            sprintf_s(buf, "[Handle: 0x%p] DWG detected at 0x%p", hFile, lpBuffer);
            AddLog(buf);
            int a = 0;

            if (!a) {
                //pid = fork();  //fork是父进程返回子进程pid，子进程返回0
                a = 1;
            }

            //为父进程时
            if (pid != 0 && pid != -1) {
                //resumeProcess(pid);
                        /// * fix stdio */
                char buf1[256];
                sprintf_s(buf1, "现在尝试更改魔术字，检查一下文件会报错么");
                const BYTE newHeader[] = { 0xbadbeef };
                memcpy((void*)data, newHeader, sizeof(newHeader));
            }
            //为子进程时
            if (pid == 0) {
                // 子进程部分代码...
            // 如果子进程也需要记录日志到自己的文件，或者通过其他IPC方式通知父进程，则需要相应实现
                OutputDebugStringA("[Debugxxxxxxxxxx]:I am a child in read not fork original");

                FILE* childFile = NULL;
                errno_t childFileErr = fopen_s(&childFile, "E:\\ICT\\masterRecording\\harness\\DLL-Injector\\Source\\x64\\Release\\child_info.txt", "w");
                if (childFileErr == 0 && childFile != NULL) {
                    fprintf(childFile, "子进程: 我是子进程，我执行到这里了。\n");
                    fprintf(childFile, "子进程: 我的进程ID是 %lu, 线程ID是 %lu\n", GetCurrentProcessId(), GetCurrentThreadId());
                    //打印lpBuffer指向的数据
                    fprintf(childFile, "%s\n", (char*)lpBuffer);


                    fclose(childFile);
                }
                else {
                    // 无法打开文件，可以考虑使用 OutputDebugStringA 或者其他方式输出信息
                    // printf 在没有控制台的子进程中可能看不到效果
                    char dbgMsg[200];
                    sprintf_s(dbgMsg, sizeof(dbgMsg), "子进程: 无法打开 child_info.txt 文件进行写入。errno: %d\n", childFileErr);
                    OutputDebugStringA(dbgMsg);
                }


            }


            else {

            }

        }
    }
    return ret;
}

DWORD WINAPI MyFork(LPVOID lpParam) {
    // fork();
    OutputDebugStringA("[Debugxxxxxxxxx]:success in myfork");
    return 0;
}



BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved)
{
    if (DetourIsHelperProcess()) return TRUE;

    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
    {


        g_hInstance = hModule;
        DisableThreadLibraryCalls(hModule);

        // 创建窗口线程
        CreateThread(nullptr, 0, WindowThread, nullptr, 0, nullptr);

        // 安装钩子
        DetourRestoreAfterWith();
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        //DetourAttach(&(PVOID&)TrueReadFile, HookedReadFile);
        //CreateThread(NULL, 0, MyFork, NULL, 0, nullptr);
        DetourAttach(&(PVOID&)TrueDispatchMessageW, MyDispatchMessageW);
        DetourAttach(&(PVOID&)TrueSendMessageW, MySendMessageW);
        

        //这是里用来测试message打开速率的代码，属于找到顶级和打开目标窗口的初始化部分
        HANDLE hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)init, NULL, 0, NULL);
        if (hThread) {
            CloseHandle(hThread); // 我们不需要管理这个线程，让它自生自灭即可
        }

        DetourTransactionCommit();

        /*CreateThread(nullptr, 0, HookInstallationThread, nullptr, 0, nullptr);*/
       // CreateThread(nullptr, 0, HookInstallationThread, nullptr, 0, nullptr);

        break;
    }
    case DLL_PROCESS_DETACH:
    {
        // 卸载钩子
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        //DetourDetach(&(PVOID&)TrueReadFile, HookedReadFile);
        DetourDetach(&(PVOID&)TrueDispatchMessageW, MyDispatchMessageW);
        DetourDetach(&(PVOID&)TrueSendMessageW, MySendMessageW);
        DetourTransactionCommit();

        //如果还在录制，则自动保存一下
        if (g_isRecording) {
            SaveRecordedMessagesToFile();
        }

        // 关闭窗口
        if (g_hMainWnd) {
            PostMessage(g_hMainWnd, WM_CLOSE, 0, 0);
        }

        /*UninstallDetoursHook();*/
        UninstallDwgHook();
        break;
    }
    }
    return TRUE;
}
