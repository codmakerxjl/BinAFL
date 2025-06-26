#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <random>
#include <chrono>
#include <filesystem> // C++17, 用于创建目录
#include <fstream>
#include "SharedData.h"
#include <filesystem>
// --- 全局唯一名字 ---
const TCHAR shm_name[] = TEXT("MyAdvancedFuzzSharedMemory");
const TCHAR sem_fuzzer_ready_name[] = TEXT("MyAdvancedFuzzSemaphoreFuzzerReady"); // Fuzzer -> Target: "数据已变异好"
const TCHAR sem_target_done_name[] = TEXT("MyAdvancedFuzzSemaphoreTargetDone");   // Target -> Fuzzer: "我处理完了"

// --- Fuzzer配置 ---
const std::string initial_seed_path = "E:\\microKernel\\FuzzReadfileHarness\\x64\\Release\\in\\s.dwg";
const std::string crashes_dir = "E:\\microKernel\\FuzzReadfileHarness\\x64\\Release\\in\\crashes";
const std::string corpus_dir = "E:\\microKernel\\FuzzReadfileHarness\\x64\\Release\\in\\corpus";
const std::string hangs_dir = "E:\\microKernel\\FuzzReadfileHarness\\x64\\Release\\in\\hangs";
//客户端路径
const std::wstring target_executable_path = L"E:\\autodesk\\DWG TrueView 2026 - English\\dwgviewr.exe";
//需要注入DLL的路径
const std::wstring dll_to_inject_path = L"E:\\microKernel\\GFLClient0.1\\x64\\Release\\GFLClient0.1.dll";


SharedData* pSharedData = nullptr; // 全局指针，方便函数访问


// Helper function to convert std::wstring to std::string for logging
std::string wstringToString(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

// Function: Starts the target process and injects a DLL
bool startAndInjectProcess(PROCESS_INFORMATION& pi, const std::wstring& executablePath, const std::wstring& dllPath) {
    std::cout << "Fuzzer: ---> Entering startAndInjectProcess function" << std::endl;
    std::cout << "Fuzzer: Target executable path: " << wstringToString(executablePath) << std::endl;
    std::cout << "Fuzzer: DLL to inject path: " << wstringToString(dllPath) << std::endl;

    // Check if the DLL file exists
    if (!std::filesystem::exists(dllPath)) {
        std::cerr << "Fuzzer: FATAL ERROR - DLL file to be injected not found: " << wstringToString(dllPath) << std::endl;
        return false;
    }

    STARTUPINFOW si = { sizeof(si) };

    // 1. Create the process in a suspended state (CREATE_SUSPENDED)
    std::cout << "Fuzzer: [Step 1/7] Creating process in suspended mode..." << std::endl;
    if (!CreateProcessW(executablePath.c_str(), NULL, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &si, &pi)) {
        std::cerr << "Fuzzer: CreateProcessW failed, error code: " << GetLastError() << std::endl;
        std::cerr << "         Check: 1. Is the target executable path correct? 2. Do you have execution permissions? 3. Do the Fuzzer and target architectures (x86/x64) match?" << std::endl;
        return false;
    }
    std::cout << "Fuzzer: Process created successfully (PID: " << pi.dwProcessId << ", TID: " << pi.dwThreadId << ")" << std::endl;

    // 2. Allocate memory in the target process for the DLL path
    std::cout << "Fuzzer: [Step 2/7] Allocating memory in the target process..." << std::endl;
    size_t dllPathSize = (dllPath.length() + 1) * sizeof(wchar_t);
    LPVOID remoteMem = VirtualAllocEx(pi.hProcess, NULL, dllPathSize, MEM_COMMIT, PAGE_READWRITE);
    if (!remoteMem) {
        std::cerr << "Fuzzer: VirtualAllocEx failed, error code: " << GetLastError() << std::endl;
        std::cerr << "         This is often a permission issue. Try 'Run as Administrator'." << std::endl;
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return false;
    }
    std::cout << "Fuzzer: Memory allocated successfully at remote address: " << remoteMem << std::endl;

    // 3. Write the DLL path into the allocated memory of the target process
    std::cout << "Fuzzer: [Step 3/7] Writing DLL path to target process memory..." << std::endl;
    if (!WriteProcessMemory(pi.hProcess, remoteMem, dllPath.c_str(), dllPathSize, NULL)) {
        std::cerr << "Fuzzer: WriteProcessMemory failed, error code: " << GetLastError() << std::endl;
        VirtualFreeEx(pi.hProcess, remoteMem, 0, MEM_RELEASE);
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return false;
    }
    std::cout << "Fuzzer: DLL path written successfully." << std::endl;

    // 4. Get the address of the LoadLibraryW function
    std::cout << "Fuzzer: [Step 4/7] Getting the address of LoadLibraryW..." << std::endl;
    HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
    if (hKernel32 == NULL) {
        std::cerr << "Fuzzer: GetModuleHandleW('kernel32.dll') failed, error code: " << GetLastError() << std::endl;
        // This is extremely rare
        VirtualFreeEx(pi.hProcess, remoteMem, 0, MEM_RELEASE);
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return false;
    }
    LPTHREAD_START_ROUTINE pLoadLibraryW = (LPTHREAD_START_ROUTINE)GetProcAddress(hKernel32, "LoadLibraryW");
    if (!pLoadLibraryW) {
        std::cerr << "Fuzzer: GetProcAddress('LoadLibraryW') failed, error code: " << GetLastError() << std::endl;
        VirtualFreeEx(pi.hProcess, remoteMem, 0, MEM_RELEASE);
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return false;
    }
    std::cout << "Fuzzer: Successfully retrieved LoadLibraryW address: " << (void*)pLoadLibraryW << std::endl;

    // 5. Create a remote thread in the target process to load the DLL
    std::cout << "Fuzzer: [Step 5/7] Creating remote thread (to call LoadLibraryW)..." << std::endl;
    HANDLE hRemoteThread = CreateRemoteThread(pi.hProcess, NULL, 0, pLoadLibraryW, remoteMem, 0, NULL);
    if (!hRemoteThread) {
        std::cerr << "Fuzzer: CreateRemoteThread failed, error code: " << GetLastError() << std::endl;
        std::cerr << "         This is the most common point of failure. Possible causes include:" << std::endl;
        std::cerr << "         - Antivirus or EDR software blocking the operation." << std::endl;
        std::cerr << "         - Insufficient process permissions (try 'Run as Administrator')." << std::endl;
        std::cerr << "         - Architecture mismatch (e.g., 64-bit Fuzzer injecting into a 32-bit process)." << std::endl;
        VirtualFreeEx(pi.hProcess, remoteMem, 0, MEM_RELEASE);
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return false;
    }
    std::cout << "Fuzzer: Remote thread created successfully." << std::endl;

    // Wait for the injection thread to complete
    std::cout << "Fuzzer: [Step 6/7] Waiting for the remote thread to finish execution..." << std::endl;
    DWORD waitResult = WaitForSingleObject(hRemoteThread, 5000); // Wait for a maximum of 5 seconds
    if (waitResult == WAIT_TIMEOUT) {
        std::cerr << "Fuzzer: WARNING - Timed out waiting for the remote thread." << std::endl;
        std::cerr << "         This could mean the DLL's DllMain function is taking too long or is deadlocked." << std::endl;
        // Continue anyway, but this is a red flag
    }
    else {
        std::cout << "Fuzzer: Remote thread has finished execution." << std::endl;
    }

    // Get the exit code of the remote thread to help determine if LoadLibrary succeeded
    DWORD remoteThreadExitCode = 0;
    GetExitCodeThread(hRemoteThread, &remoteThreadExitCode);
    if (remoteThreadExitCode == 0) {
        std::cerr << "Fuzzer: WARNING - Remote thread exit code is 0. This likely means LoadLibraryW failed in the target process!" << std::endl;
        std::cerr << "         Check: 1. Do all DLL dependencies exist in the target directory? 2. Did DllMain return FALSE?" << std::endl;
    }
    else {
        std::cout << "Fuzzer: Remote thread returned a non-zero handle (HMODULE). DLL '" << wstringToString(dllPath) << "' appears to be injected successfully." << std::endl;
    }

    // 6. Clean up resources used during injection
    CloseHandle(hRemoteThread);
    VirtualFreeEx(pi.hProcess, remoteMem, 0, MEM_RELEASE);

    // 7. Resume the main thread to let the target program run normally
    std::cout << "Fuzzer: [Step 7/7] Resuming the main thread of the target process..." << std::endl;
    ResumeThread(pi.hThread);
    std::cout << "Fuzzer: Target process has been resumed (PID: " << pi.dwProcessId << ")" << std::endl;
    std::cout << "Fuzzer: <--- Injection process complete, function returning true" << std::endl;

    return true;
}



// 函数：将文件内容读入到共享内存的buffer中
bool loadSeedToSharedMemory(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "Writer: 严重错误 - 无法打开初始种子文件: " << filePath << std::endl;
        return false;
    }
    std::streamsize size = file.tellg();
    if (size > SHARED_BUFFER_SIZE) {
        std::cerr << "Writer: 严重错误 - 种子文件大小超过共享内存缓冲区。" << std::endl;
        return false;
    }
    file.seekg(0, std::ios::beg);
    if (!file.read(pSharedData->buffer, size)) {
        std::cerr << "Writer: 严重错误 - 读取种子文件失败。" << std::endl;
        return false;
    }
    pSharedData->data_size = static_cast<int>(size);
    std::cout << "Writer: 成功加载初始种子 (" << size << " 字节)." << std::endl;
    return true;
}

// 函数：将当前共享内存的buffer保存到文件
void saveBufferToFile(const std::string& dir, const std::string& prefix) {
    long long timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    std::string filename = dir + "\\" + prefix + "_" + std::to_string(timestamp) + ".dat";

    std::ofstream file(filename, std::ios::binary);
    if (file) {
        file.write(pSharedData->buffer, pSharedData->data_size);
        std::cout << "Fuzzer: [" << prefix << "] 事件! 已将输入保存到 " << filename << std::endl;
    }
    else {
        std::cerr << "Fuzzer: 错误 - 无法写入文件 " << filename << std::endl;
    }
}

// 函数：对共享内存的buffer进行变异
void mutateSharedMemory() {
    if (pSharedData->data_size <= 0) return;

    std::random_device rd;
    std::mt19937 gen(rd());

    unsigned long long mutation_base_offset = 0;
    unsigned long long mutation_range_size = pSharedData->data_size;

    // 检查是否存在分析数据，以确定变异范围
    if (pSharedData->op_count > 0) {
        size_t last_op_index = pSharedData->op_count - 1;
        unsigned long long last_offset = pSharedData->offsets[last_op_index];
        unsigned long last_size = pSharedData->sizes[last_op_index];

        if (last_size > 0 && last_offset + last_size <= pSharedData->data_size) {
            mutation_base_offset = last_offset;
            mutation_range_size = last_size;
            std::cout << "Writer: [智能变异] 目标区域 -> 最后一次读取操作 (偏移: "
                << mutation_base_offset << ", 大小: " << mutation_range_size << ")\n";
        }
        else {
            std::cout << "Writer: [智能变异] 警告: 最后一次读取操作记录无效，退回至全局随机变异。\n";
        }
    }
    else {
        std::cout << "Writer: [智能变异] 无分析数据，执行全局随机变异。\n";
    }

    // 随机选择一种变异策略 (0-4)
    std::uniform_int_distribution<> strategy_dist(0, 4);
    int chosen_strategy = strategy_dist(gen);

    // 如果目标范围太小，某些策略可能无法执行，则退回到位翻转
    if (mutation_range_size < 16 && chosen_strategy > 1) {
        chosen_strategy = 0; // 范围太小，只执行最简单的位翻转
    }

    switch (chosen_strategy) {
    case 0: { // 策略0: 位翻转 (Bit Flip)
        if (mutation_range_size <= 0) break;
        std::uniform_int_distribution<unsigned long long> pos_dist(0, mutation_range_size - 1);
        unsigned long long final_pos = mutation_base_offset + pos_dist(gen);

        pSharedData->buffer[final_pos] ^= (1 << (gen() % 8));
        std::cout << "Writer: 执行变异 [策略 0: 位翻转] 于位置 " << final_pos << std::endl;
        break;
    }

    case 1: { // 策略1: 字节块插入 (Byte Insertion)
        const char new_data[] = "FUZZ";
        int insert_len = sizeof(new_data) - 1;
        if (pSharedData->data_size + insert_len > SHARED_BUFFER_SIZE) {
            std::cout << "Writer: 变异 [策略 1: 插入] 失败，缓冲区空间不足。" << std::endl;
            break;
        }
        std::uniform_int_distribution<unsigned long long> pos_dist(0, mutation_range_size);
        unsigned long long final_insert_pos = mutation_base_offset + pos_dist(gen);

        memmove(pSharedData->buffer + final_insert_pos + insert_len,
            pSharedData->buffer + final_insert_pos,
            pSharedData->data_size - final_insert_pos);
        memcpy(pSharedData->buffer + final_insert_pos, new_data, insert_len);
        pSharedData->data_size += insert_len;
        std::cout << "Writer: 执行变异 [策略 1: 插入] 于位置 " << final_insert_pos << std::endl;
        break;
    }

          // [新策略]
    case 2: { // 策略2: 字节块删除 (Block Deletion)
        if (mutation_range_size <= 1) break; // 至少要有2字节才能删除
        std::uniform_int_distribution<unsigned long long> pos_dist(0, mutation_range_size - 1);
        unsigned long long del_pos = mutation_base_offset + pos_dist(gen);

        std::uniform_int_distribution<int> len_dist(1, min((unsigned long long)16, mutation_range_size - (del_pos - mutation_base_offset)));
        int del_len = len_dist(gen);

        memmove(pSharedData->buffer + del_pos,
            pSharedData->buffer + del_pos + del_len,
            pSharedData->data_size - del_pos - del_len);

        pSharedData->data_size -= del_len;
        std::cout << "Writer: 执行变异 [策略 2: 删除] 于位置 " << del_pos << ", 长度 " << del_len << std::endl;
        break;
    }

          // [新策略]
    case 3: { // 策略3: “关键数值”覆盖 (Interesting Value Overwrite)
        if (mutation_range_size < 8) break; // 确保有足够空间

        // 随机选择要覆盖的数据长度（1, 2, 4, 8字节）
        std::uniform_int_distribution<> size_sel_dist(0, 3);
        int size_to_overwrite = 1 << size_sel_dist(gen); // 1, 2, 4, 8

        std::uniform_int_distribution<unsigned long long> pos_dist(0, mutation_range_size - size_to_overwrite);
        unsigned long long overwrite_pos = mutation_base_offset + pos_dist(gen);

        // 定义一些“关键”数值
        const int8_t interesting_8[] = { -128, -1, 0, 1, 127 };
        const int16_t interesting_16[] = { -32768, -1, 0, 1, 32767 };
        const int32_t interesting_32[] = { -2147483648, -1, 0, 1, 2147483647 };
        const int64_t interesting_64[] = { INT64_MIN, -1, 0, 1, INT64_MAX };

        switch (size_to_overwrite) {
        case 1: memcpy(pSharedData->buffer + overwrite_pos, &interesting_8[gen() % 5], 1); break;
        case 2: memcpy(pSharedData->buffer + overwrite_pos, &interesting_16[gen() % 5], 2); break;
        case 4: memcpy(pSharedData->buffer + overwrite_pos, &interesting_32[gen() % 5], 4); break;
        case 8: memcpy(pSharedData->buffer + overwrite_pos, &interesting_64[gen() % 5], 8); break;
        }
        std::cout << "Writer: 执行变异 [策略 3: 关键数值覆盖] 于位置 " << overwrite_pos << ", 长度 " << size_to_overwrite << std::endl;
        break;
    }

          // [新策略]
    case 4: { // 策略4: 整数加减运算 (Arithmetic Add/Subtract)
        if (mutation_range_size <= 0) break;
        std::uniform_int_distribution<unsigned long long> pos_dist(0, mutation_range_size - 1);
        unsigned long long arith_pos = mutation_base_offset + pos_dist(gen);

        std::uniform_int_distribution<int> val_dist(-35, 35);
        int delta = val_dist(gen);

        uint8_t original_byte = pSharedData->buffer[arith_pos];
        pSharedData->buffer[arith_pos] += delta; // C++ 的 uint8_t 会自动处理溢出（wrap-around）

        std::cout << "Writer: 执行变异 [策略 4: 整数运算] 于位置 " << arith_pos << ", 值 "
            << (int)original_byte << " " << (delta >= 0 ? "+" : "") << delta
            << " -> " << (int)(uint8_t)pSharedData->buffer[arith_pos] << std::endl;
        break;
    }
    }
}

int main() {
    // --- 1. 初始化 & 创建目录 --- //以及创建好了目录了，不需要在初始化再创建，之后会添加一个目录是否存在的判断逻辑
    std::cout << "--- Fuzzer (服务器) 启动 ---" << std::endl;


   // --- 2. 创建共享内存和信号量 ---
    HANDLE hMapFile = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(SharedData), shm_name);
    if (hMapFile == NULL) { /* 错误处理 */ return 1; }
    pSharedData = (SharedData*)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedData));
    if (pSharedData == NULL) { /* 错误处理 */ CloseHandle(hMapFile); return 1; }

    HANDLE hSemFuzzerReady = CreateSemaphore(NULL, 0, 1, sem_fuzzer_ready_name);
    HANDLE hSemTargetDone = CreateSemaphore(NULL, 0, 1, sem_target_done_name);
    if (hSemFuzzerReady == NULL || hSemTargetDone == NULL) { /* 错误处理 */ return 1; }

    memset(pSharedData, 0, sizeof(SharedData));

    // 设定初始命令为“分析模式”，等待客户端的链接
    pSharedData->analysis_completed = false;
    std::cout << "[分析模式], 等待客户端连接...\n";

    // --- 3. 加载初始种子文件 ---
    if (!loadSeedToSharedMemory(initial_seed_path)) {
        return 1;
    }

    PROCESS_INFORMATION pi = { 0 };

    //启动目标程序，并且注入dll到目标程序中
    if (!startAndInjectProcess(pi, target_executable_path, dll_to_inject_path)) {
        std::cerr << "Fuzzer :初始化进程启动失败，程序即将退出。" << std::endl;

        return 1;

    }

    // 第一次，我们不进行变异，直接用初始种子测试
    ReleaseSemaphore(hSemFuzzerReady, 1, NULL);

    long long iteration = 0;
    // --- 4. Fuzzing 主循环 ---
    while (true) {
        iteration++;

        HANDLE wait_handles[] = { pi.hProcess, hSemTargetDone };
        const DWORD timeout_ms = 50000;

        std::cout << "\n--- [迭代 #" << iteration << ", PID: " << pi.dwProcessId << "] 等待响应..." << std::endl;
        DWORD wait_result = WaitForMultipleObjects(2, wait_handles, FALSE, timeout_ms);

        bool restart_process = false;
        bool continue_fuzzing = false;

        switch (wait_result)
        {
        case WAIT_OBJECT_0 + 0: // 进程崩溃
            std::cerr << "Fuzzer:检测到的目标程序意外终止！[CRASH]" << std::endl;
            saveBufferToFile(crashes_dir, "crash");
            restart_process = true;
            break;

        case WAIT_OBJECT_0 + 1: // 目标正常完成
            std::cout << "Fuzzer:目标进程正常完成本次处理。" << std::endl;
            // 在这里处理新覆盖率等逻辑
            if (pSharedData->result == FuzzProtocol::RESULT_NEW_COVERAGE) {
                saveBufferToFile(corpus_dir, "corpus");
            }
            // 读取客户端状态，决定下一步
            if (pSharedData->command == FuzzProtocol::CMD_EXIT) {
                std::cout << "Fuzzer:收到退出指令，关闭Fuzzer" << std::endl;
                continue_fuzzing = false; // 准备退出主循环
            }
            else {
                continue_fuzzing = true; // 准备下一轮变异
            }
            break;

        case WAIT_TIMEOUT: // 超时
            std::cerr << "Fuzzer：检测到目标进程超时！[HANG]" << std::endl;
            saveBufferToFile(hangs_dir, "hang");
            TerminateProcess(pi.hProcess, 1);
            restart_process = true;
            break;

        default: // 等待出错
            std::cerr << "Fuzzer:WaitForMultipleObjects 失败，错误代码： " << GetLastError() << std::endl;
            TerminateProcess(pi.hProcess, 1);
            restart_process = true;
            break;
        }

        // 如果需要重启
        if (restart_process) {
            std::cout << "Fuzzer:正在重启目标进程..." << std::endl;
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            if (!startAndInjectProcess(pi, target_executable_path, dll_to_inject_path)) {
                std::cerr << "Fuzzer:重启进程失败，程序即将退出。" << std::endl;
                break; // 无法重启，退出主循环
            }
            continue_fuzzing = true; // 重启后准备进行下一次测试
        }

        if (!continue_fuzzing) {
            break; // 退出主循环
        }

        // --- 执行下一轮Fuzzing ---
        // 【关键修改】此时目标已经完成，可以直接进行变异并通知它

        // ... 在这里处理分析模式切换等逻辑 ...
        if (pSharedData->command == FuzzProtocol::CMD_INIT && !pSharedData->analysis_completed) {
            std::cout << "初始化完成，之后会进行Fuzzing变异\n";
            pSharedData->analysis_completed = true;
        }

        mutateSharedMemory();
        ReleaseSemaphore(hSemFuzzerReady, 1, NULL); // 通知目标可以开始处理新数据了
    }
    // --- 5. 清理 ---
    UnmapViewOfFile(pSharedData);
    CloseHandle(hMapFile);
    CloseHandle(hSemFuzzerReady);
    CloseHandle(hSemTargetDone);

    return 0;
}