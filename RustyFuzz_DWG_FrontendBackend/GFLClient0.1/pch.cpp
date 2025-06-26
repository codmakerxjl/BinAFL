// pch.cpp: 与预编译标头对应的源文件

#include "pch.h"


//父子窗口全局变量初始化为空
HWND hwndProcessTop = NULL;
HWND hwndReplyWindow = NULL;
HANDLE g_ReadTargetFileHandle = NULL;

std::atomic<bool> g_keepMonitoring(true);
std::atomic<bool>g_isMonitor(false);
std::atomic<double> g_latest_cpu_usage{ 0.0 };
std::atomic<double> g_latest_io_read_rate{ 0.0 };
std::atomic<double> g_latest_io_write_rate{ 0.0 };
std::atomic<bool> g_monitoring_thread_running{ false };
std::atomic<bool> g_stop_monitoring_flag{ false };




//共享内存的全局变量👇
const TCHAR shm_name[] = TEXT("MyAdvancedFuzzSharedMemory");
const TCHAR sem_fuzzer_ready_name[] = TEXT("MyAdvancedFuzzSemaphoreFuzzerReady");
const TCHAR sem_target_done_name[] = TEXT("MyAdvancedFuzzSemaphoreTargetDone");
HANDLE g_hSemFuzzerReady = NULL;
HANDLE g_hSemTargetDone = NULL;
SharedData* g_pSharedData = NULL;
//监控的窗口是否已经关闭
static std::thread g_monitoring_thread;
// 为重放线程增加一个全局句柄 
static std::thread g_reply_thread;

// 互斥锁，用于保护下面的条件变量和状态标志
std::mutex mtx;
// 条件变量，用于线程间的通知
std::condition_variable cv;
// 状态标志，决定当前应该是哪个线程工作。true = 该轮到ReplyOpen了，false = 该轮到监控线程了
std::atomic<bool> g_isReplyTurn{ true };
// 全局变量，用于日志记录
CRITICAL_SECTION g_csLog;
wchar_t g_logFilePath[MAX_PATH];


//读文件初始化标志位，当它为false时，需要分析读文件时的结构，当它为true时，直接根据文件结构进行变异。
std::atomic<bool> g_analysis_flag = false;
void init() {
	
	//初始化寻找子父窗口的全角变量
    while (hwndProcessTop == NULL || hwndReplyWindow == NULL) {
        hwndProcessTop = FindWindow(NULL, L"Autodesk DWG TrueView 2026 - [Start]");
        HWND t = FindDescendantWindow(hwndProcessTop, L"Start");
        hwndReplyWindow = GetWindow(t, GW_HWNDLAST);

    }



    ////监控s.dwg变量是否打开
    //        ////关闭s.dwg
    //if (!g_isMonitor) {
    //    std::thread monitorThread = StartDescendantWindowMonitor(hwndProcessTop, L"s.dwg");
    //    if (monitorThread.joinable()) { // 确保线程成功创建
    //        monitorThread.detach(); // 让监控线程独立运行
    //    }
    //    g_isMonitor = true;
    //}

    //文件打开线程
    g_reply_thread = std::thread(TargetWindowsOpenAndClose);
    if (g_reply_thread.joinable()) {
        g_reply_thread.detach();
    }

    //监控CPU使用率和IO使用率
    start_monitoring_background_thread();

    //初始化共享内存
    initSharedMem();

}


void TargetWindowsOpenAndClose() {
    // WriteLog a message indicating the thread has started.
 // WriteLog(L"TargetWindowsOpenAndClose thread started.");

    std::wstring closeWndName = L"s.dwg";

    while (1) {
    //  WriteLog(L"Starting new fuzzing iteration.");
        //如果没有打开成功就休眠一段时间再打开
        while (!ReplyOpen())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
   //   WriteLog(L"ReplyOpen() successful, posted 'open file' command.");


        //如果打开成功了，还需要通知fuzzer analysis 变量的更新  ？？？需要吗
        if (g_pSharedData != NULL) {
            // 将 Fuzzer 的全局状态同步到 DLL 的本地标志
            g_analysis_flag.store(g_pSharedData->analysis_completed);
      //    WriteLog(L"Synced analysis flag from shared memory. g_analysis_flag is now: %s.", g_analysis_flag.load() ? L"true" : L"false");
        }

        //如果打开成功了，需要判定关闭窗口的条件，这里使用IO和CPU rate 来判断，当IO或者CPU使用率较低时，程序没有在继续运算了
        const double CPU_STABILITY_THRESHOLD = 8.0;  //cpu使用率 8%
        const double IO_STABILITY_THRESHOLD = 10000.0;  //IO字节速度 1kb/s
        const wchar_t* SELECT_FILE_DIALOG_TITLE = L"Select File";
        // 尝试查找主要目标窗口且报错窗口没有产生,就会一直找
        HWND hwndToClose;
        HWND hwndErrorDialog;
    //  WriteLog(L"Beginning search for target window ('%s') or error dialog ('%s').", closeWndName.c_str(), SELECT_FILE_DIALOG_TITLE);

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        hwndToClose = FindDescendantWindow(hwndProcessTop, closeWndName.c_str());
        hwndErrorDialog = FindWindow(NULL, SELECT_FILE_DIALOG_TITLE);

        
    //  WriteLog(L"Window search complete. Found hwndToClose: 0x%p, Found hwndErrorDialog: 0x%p.", hwndToClose, hwndErrorDialog);


        //如果产生了错误窗口就直接关闭。就不用后面的窗口逻辑判断了。
        if (hwndErrorDialog) {
        //  WriteLog(L"Error dialog ('%s') detected. Closing it.", SELECT_FILE_DIALOG_TITLE);
            PostMessage(hwndErrorDialog, WM_CLOSE, 0, 0);
        //  WriteLog(L"Error dialog closed. Signaling fuzzer to continue.");
            EndTurnAndSignalNext(); // 完成任务，执行同步并交出控制权
            continue;

        }
        
        if (hwndToClose) {
         // WriteLog(L"Target window ('%s') found. Checking system stability to decide on closing.", closeWndName.c_str());
            //需要关闭的窗口出现了判断系统是否稳定了
            auto cpuUsage = get_current_process_cpu_usage_from_thread();
            auto ioRate = get_current_process_io_rate_from_thread();
       //   WriteLog(L"Current Process Metrics - CPU: %.2f%%, IO Read: %.2f bytes/sec, IO Write: %.2f bytes/sec.", cpuUsage, ioRate.first, ioRate.second);

            bool isSystemStable = (cpuUsage >= 0 && cpuUsage <= CPU_STABILITY_THRESHOLD);

            if (isSystemStable) {
            //  WriteLog(L"System is stable. Closing target window '%s'.", closeWndName.c_str());
                PostMessage(hwndToClose, WM_CLOSE, 0, 0);
             // WriteLog(L"Target window closed. Signaling fuzzer to continue.");
                EndTurnAndSignalNext(); // 完成任务，执行同步并交出控制权
            }
            else {
              //WriteLog(L"System is not stable. The current open/close cycle will now restart.");
            }

        }
        
    }
}

HWND FindDescendantWindow(HWND hParent, LPCWSTR title) {
	FindDescendantData data = { title, NULL };
	EnumChildWindows(hParent, EnumDescendantProc, (LPARAM)&data);
	return data.foundHwnd;
}


BOOL CALLBACK EnumDescendantProc(HWND hwndChild, LPARAM lParam) {
    FindDescendantData* pData = (FindDescendantData*)lParam;
    wchar_t title[256];

    if (GetWindowTextW(hwndChild, title, _countof(title))) {
        if (wcscmp(title, pData->targetTitle) == 0) {
            pData->foundHwnd = hwndChild;
            return FALSE; // 找到了，停止枚举当前父窗口的子窗口
        }
    }
    // 递归查找这个子窗口的子窗口
    EnumChildWindows(hwndChild, EnumDescendantProc, lParam);
    if (pData->foundHwnd != NULL) {
        return FALSE; // 在深层递归中找到了，停止
    }
    return TRUE; // 继续枚举同级其他子窗口
}

//失败返回0 ，成功返回非0
bool ReplyOpen() {
    if (!hwndProcessTop || !hwndReplyWindow )  //当程序的顶级父窗口或者重放窗口或者打开的窗口还未关闭时，就不会再打开窗口了
        return 0;

    return PostMessage(hwndReplyWindow, WM_COMMAND, autoCAD_replay_lparam, autoCAD_replay_wparam);

}

std::thread StartDescendantWindowMonitor(HWND parentHwnd, std::wstring title) {
    if (parentHwnd == NULL || !IsWindow(parentHwnd)) {
        return {};
    }

    return std::thread(MonitorAndCloseDescendantWindowWorker, parentHwnd, title);
}

 void EndTurnAndSignalNext() {
    // 1. 更新共享内存中的状态
    if (g_pSharedData->command != FuzzProtocol::CMD_INIT) {
        g_pSharedData->command = FuzzProtocol::CMD_CONTINUE_FUZZING;
    }
    g_pSharedData->result = FuzzProtocol::RESULT_OK;

    // 2. 通知 Fuzzer：“我这一轮搞定了，你可以处理了”
    ReleaseSemaphore(g_hSemTargetDone, 1, NULL);

    // 3. 等待 Fuzzer 处理完毕并准备好下一轮的数据
    WaitForSingleObject(g_hSemFuzzerReady, INFINITE);


    
}

// --- 对外的接口函数 (函数名和参数保持不变) ---
void MonitorAndCloseDescendantWindowWorker(HWND parentHwnd, std::wstring title) {
    // 为魔法字符串和数字定义常量，提高代码可读性和可维护性
    const double CPU_STABILITY_THRESHOLD = 8.0;
    const double IO_STABILITY_THRESHOLD = 10000.0;
    const wchar_t* SELECT_FILE_DIALOG_TITLE = L"Select File";

    while (g_keepMonitoring) {
        // --- 1. 等待轮到自己工作 ---
        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [] { return !g_isReplyTurn.load(); });
        }

        // 在被唤醒后，再次检查是否需要继续监控
        if (!g_keepMonitoring) {
            break;
        }

        // --- 2. 在当前轮次中持续监控和关闭窗口 ---
        while (g_keepMonitoring) {
            // 如果父窗口失效，本轮任务无法继续，直接跳出内层循环
            if (!IsWindow(parentHwnd)) {
                // 注意：这里原版代码没有处理父窗口失效后如何通知Fuzzer的情况，
                // 如果需要在这里也通知，可以加上 EndTurnAndSignalNext();
                break;
            }

            // 尝试查找主要目标窗口
            HWND hwndToClose = FindDescendantWindow(parentHwnd, title.c_str());

            if (hwndToClose != nullptr) {
                // 找到了目标窗口，检查系统是否稳定
                auto cpuUsage = get_current_process_cpu_usage_from_thread();
                auto ioRate = get_current_process_io_rate_from_thread();

                bool isSystemStable = (cpuUsage >= 0 && cpuUsage <= CPU_STABILITY_THRESHOLD) &&
                    (ioRate.first >= 0.0 && ioRate.first <= IO_STABILITY_THRESHOLD) &&
                    (ioRate.second >= 0.0 && ioRate.second <= IO_STABILITY_THRESHOLD);

                if (isSystemStable) {
                    PostMessage(hwndToClose, WM_CLOSE, 0, 0);
                    EndTurnAndSignalNext(); // 完成任务，执行同步并交出控制权
                    break; // 已完成本轮任务，退出内层循环
                }
                // 如果系统不稳定，则不关闭窗口，在短暂休眠后继续尝试
            }
            else {
                // 未找到主要目标窗口，检查是否出现了备用的错误对话框
                HWND hwndErrorDialog = FindWindow(NULL, SELECT_FILE_DIALOG_TITLE);
                if (hwndErrorDialog != nullptr) {
                    PostMessage(hwndErrorDialog, WM_CLOSE, 0, 0);
                }

                // 无论是否找到并关闭了错误对话框，都认为本轮的“查找与关闭”尝试已经结束。
                // 遵循原始逻辑：即使什么都没找到，也要通知Fuzzer并交出控制权。
                EndTurnAndSignalNext();
                break; // 已完成本轮任务，退出内层循环
            }

            // 如果没找到窗口或系统不稳定，短暂休眠，避免CPU空转
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}
double get_current_process_cpu_usage_from_thread() {
    if (!g_monitoring_thread_running) {
        // 可以选择返回一个错误码，或者0，或者尝试启动线程
        // 这里简单返回0，表示监控未运行或未就绪
        return 0.0;
    }
    return g_latest_cpu_usage.load(std::memory_order_relaxed);
}

std::pair<double, double> get_current_process_io_rate_from_thread() {
    if (!g_monitoring_thread_running) {
        return { 0.0, 0.0 };
    }
    return {
        g_latest_io_read_rate.load(std::memory_order_relaxed),
        g_latest_io_write_rate.load(std::memory_order_relaxed)
    };
}


void start_monitoring_background_thread() {
    if (g_monitoring_thread_running) return; // 如果已经在运行，则不重复启动

    g_stop_monitoring_flag = false; // 清除停止标志
    // 确保之前的线程（如果存在且已结束）被正确处理
    if (g_monitoring_thread.joinable()) {
        g_monitoring_thread.join(); // 等待旧线程结束
    }
    g_monitoring_thread = std::thread(monitoring_worker_thread_function);
    // 通常，如果这个监控是DLL生命周期的一部分，你可能想 detach 它，
    // 或者在DLL卸载时有明确的 join 逻辑。
    // g_monitoring_thread.detach(); // 如果不关心其结束状态，并希望它独立运行
}


void stop_monitoring_background_thread() {
    if (g_monitoring_thread_running) {
        g_stop_monitoring_flag = true;
        if (g_monitoring_thread.joinable()) {
            g_monitoring_thread.join(); // 等待线程安全退出
        }
    }
}


void monitoring_worker_thread_function() {
    // 初始化CPU使用率计算所需的静态变量 (这些现在是线程的局部"静态"或成员变量)
    ULARGE_INTEGER last_thread_sys_cpu, last_thread_user_cpu;
    std::chrono::steady_clock::time_point last_thread_cpu_time;
    bool thread_cpu_initialized = false;
    SYSTEM_INFO sys_info_worker;
    GetSystemInfo(&sys_info_worker);
    int num_processors_worker = sys_info_worker.dwNumberOfProcessors > 0 ? sys_info_worker.dwNumberOfProcessors : 1;


    // 初始化I/O使用率计算所需的静态变量
    IO_COUNTERS last_thread_io_counters;
    std::chrono::steady_clock::time_point last_thread_io_time;
    bool thread_io_initialized = false;
    HANDLE self_proc_handle = GetCurrentProcess(); // 获取一次进程句柄

    // 首次初始化CPU
    if (true) { // 模拟初始化块
        FILETIME ftime_init, fsys_init, fuser_init;
        GetProcessTimes(self_proc_handle, &ftime_init, &ftime_init, &fsys_init, &fuser_init);
        memcpy(&last_thread_sys_cpu, &fsys_init, sizeof(FILETIME));
        memcpy(&last_thread_user_cpu, &fuser_init, sizeof(FILETIME));
        last_thread_cpu_time = std::chrono::steady_clock::now();
        thread_cpu_initialized = true;
    }

    // 首次初始化I/O
    if (GetProcessIoCounters(self_proc_handle, &last_thread_io_counters)) {
        last_thread_io_time = std::chrono::steady_clock::now();
        thread_io_initialized = true;
    }

    g_monitoring_thread_running = true;

    while (!g_stop_monitoring_flag) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500)); // 采样间隔，例如500ms

        // --- 计算CPU使用率 ---
        if (thread_cpu_initialized) {
            FILETIME ftime_now, fsys_now, fuser_now;
            ULARGE_INTEGER sys_now, user_now;
            auto current_cpu_time = std::chrono::steady_clock::now();
            std::chrono::duration<double> elapsed_cpu_wall_seconds = current_cpu_time - last_thread_cpu_time;

            GetProcessTimes(self_proc_handle, &ftime_now, &ftime_now, &fsys_now, &fuser_now);
            memcpy(&sys_now, &fsys_now, sizeof(FILETIME));
            memcpy(&user_now, &fuser_now, sizeof(FILETIME));

            if (elapsed_cpu_wall_seconds.count() > 0.001) { // 避免除以过小的值
                ULONGLONG process_time_diff = (sys_now.QuadPart - last_thread_sys_cpu.QuadPart) +
                    (user_now.QuadPart - last_thread_user_cpu.QuadPart);
                double cpu_percent = (static_cast<double>(process_time_diff) / (elapsed_cpu_wall_seconds.count() * 10000000.0)) * 100.0;
                cpu_percent = cpu_percent / num_processors_worker; // 相对于总系统

                if (cpu_percent < 0.0) cpu_percent = 0.0;
                if (cpu_percent > 100.0) cpu_percent = 100.0;
                g_latest_cpu_usage.store(cpu_percent);

                last_thread_sys_cpu = sys_now;
                last_thread_user_cpu = user_now;
                last_thread_cpu_time = current_cpu_time;
            }
            else {
                // 时间间隔太短，可以不更新，或沿用上次的值，或置0
                // g_latest_cpu_usage.store(0.0); // 如果认为短时间没变化或者数据不可靠
            }
        }

        // --- 计算I/O速率 ---
        if (thread_io_initialized) {
            IO_COUNTERS current_io_counters;
            auto current_io_time = std::chrono::steady_clock::now();
            std::chrono::duration<double> elapsed_io_seconds = current_io_time - last_thread_io_time;

            if (GetProcessIoCounters(self_proc_handle, &current_io_counters)) {
                if (elapsed_io_seconds.count() > 0.001) { // 避免除以过小的值
                    ULONGLONG read_bytes_diff = current_io_counters.ReadTransferCount - last_thread_io_counters.ReadTransferCount;
                    ULONGLONG write_bytes_diff = current_io_counters.WriteTransferCount - last_thread_io_counters.WriteTransferCount;

                    double read_rate = static_cast<double>(read_bytes_diff) / elapsed_io_seconds.count();
                    double write_rate = static_cast<double>(write_bytes_diff) / elapsed_io_seconds.count();

                    g_latest_io_read_rate.store(read_rate);
                    g_latest_io_write_rate.store(write_rate);

                    last_thread_io_counters = current_io_counters;
                    last_thread_io_time = current_io_time;
                }
                else {
                    // g_latest_io_read_rate.store(0.0);
                    // g_latest_io_write_rate.store(0.0);
                }
            }
        }
    }
    g_monitoring_thread_running = false;
}


void reply_open_worker() {
    while (g_keepMonitoring) {
        // --- 1. Wait for its turn to work ---
        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [] { return g_isReplyTurn.load(); });
        }

        if (g_pSharedData != NULL) {
            // 将 Fuzzer 的全局状态同步到 DLL 的本地标志
            g_analysis_flag.store(g_pSharedData->analysis_completed);
        }

        ReplyOpen();


        // --- 3. Task complete, switch state and notify monitor thread ---
        {
            std::lock_guard<std::mutex> lock(mtx);
            g_isReplyTurn = false;
            g_ReadTargetFileHandle = NULL;
            cv.notify_one();
        }
    }
}


//如果成功返回非0，如果失败返回0 （linux和windows的成功返回值不一样，linux偏向于成功返回0，windows是失败返回0）
bool initSharedMem() {
    HANDLE hMapFile = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, shm_name);
    if (hMapFile == NULL) {
        //无法连接内存返回0
        return 0;
    }
    g_pSharedData = (SharedData*)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedData));
    if (!g_pSharedData) {
        //失败返回0
        return 0;
    }
    g_hSemFuzzerReady = OpenSemaphore(SEMAPHORE_ALL_ACCESS, FALSE, sem_fuzzer_ready_name);
    g_hSemTargetDone = OpenSemaphore(SEMAPHORE_ALL_ACCESS, FALSE, sem_target_done_name);
    if (!g_hSemFuzzerReady || !g_hSemTargetDone) {
        return 0;
    }


    //已经成功连接
    return 1;
}
// 当使用预编译的头时，需要使用此源文件，编译才能成功。



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
    _In_opt_ LPOVERLAPPED lpOverlapped)
{
    // --- 1. 参数校验 ---
    if (pSharedMemBase == NULL || lpBuffer == NULL || lpNumberOfBytesRead == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    // 总是先将输出字节数清零
    *lpNumberOfBytesRead = 0;

    ULONGLONG readOffset = 0;

    // --- 2. 根据模式计算读取偏移量 ---

    // 情况一: 同步读取 (lpOverlapped为NULL)
    // 偏移量是文件指针的当前位置
    if (lpOverlapped == NULL) {
        LARGE_INTEGER currentPos = { 0 };
        // 通过将移动距离设为0来获取当前文件指针位置
        if (!SetFilePointerEx(hFile, { 0 }, &currentPos, FILE_CURRENT)) {
            // 如果获取失败，直接返回错误
            return FALSE;
        }
        readOffset = currentPos.QuadPart;
    }
    // 情况二: 重叠I/O (lpOverlapped不为NULL)
    // 偏移量在OVERLAPPED结构中明确指定
    else {
        ULARGE_INTEGER overlappedPos;
        overlappedPos.LowPart = lpOverlapped->Offset;
        overlappedPos.HighPart = lpOverlapped->OffsetHigh;
        readOffset = overlappedPos.QuadPart;
    }

    // --- 3. 边界检查和数据拷贝 ---

    // 如果计算出的起始偏移已经等于或超过文件末尾，则读取不到任何数据
    if (readOffset >= fileSize) {
        *lpNumberOfBytesRead = 0;
        return TRUE; // 行为和ReadFile一致：到达文件末尾时返回成功，但读取字节为0
    }

    // 计算从当前偏移到文件末尾还剩下多少字节
    ULONGLONG remainingBytes = fileSize - readOffset;
    // 实际要拷贝的字节数，不能超过请求数，也不能超过剩余数
    DWORD bytesToCopy = (DWORD)min((ULONGLONG)nNumberOfBytesToRead, remainingBytes);

    // 从内存中拷贝数据到目标程序的缓冲区
    memcpy(lpBuffer, (PBYTE)pSharedMemBase + readOffset, bytesToCopy);

    // 更新实际读取的字节数
    *lpNumberOfBytesRead = bytesToCopy;

    // --- 4. 【关键】更新文件指针 (仅限同步模式) ---
    // 在同步模式下，ReadFile会推进文件指针，我们必须模拟这个行为
    if (lpOverlapped == NULL) {
        LARGE_INTEGER newPos;
        newPos.QuadPart = readOffset + bytesToCopy;
        if (!SetFilePointerEx(hFile, newPos, NULL, FILE_BEGIN)) {
            // 如果设置指针失败，这是一个潜在的问题，但数据已经复制，
            // 所以我们仍然可以考虑返回TRUE，或者根据严格性要求返回FALSE
            return FALSE;
        }
    }

    // --- 5. 返回成功 ---
    SetLastError(ERROR_SUCCESS); // 表示操作成功完成
    return TRUE;
}


// =======================================================================
// 日志记录函数 (函数定义保留，方便未来重新启用)
// =======================================================================
void WriteLog(const wchar_t* format, ...) {
    EnterCriticalSection(&g_csLog);

    // 格式化日志消息
    wchar_t buffer[1024];
    va_list args;
    va_start(args, format);
    vswprintf_s(buffer, sizeof(buffer) / sizeof(wchar_t), format, args);
    va_end(args);

    // 添加时间戳
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t finalMessage[2048];
    swprintf_s(finalMessage, L"[%02d:%02d:%02d.%03d] %s\n", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, buffer);

    // 打开（或创建）日志文件并追加内容
    HANDLE hFile = CreateFileW(g_logFilePath, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD bytesWritten;
        WriteFile(hFile, finalMessage, (DWORD)wcslen(finalMessage) * sizeof(wchar_t), &bytesWritten, NULL);
        CloseHandle(hFile);
    }

    LeaveCriticalSection(&g_csLog);
}