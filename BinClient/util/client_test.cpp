#include "pch.h"
#include "client_test.h"
#include <windows.h>
#include <string>
#include "SharedMemoryIPC.h" // Assumes SharedMemoryIPC.h/.cpp are in the project
#include "FileCacheManager.h"  // 我们之前创建的缓存管理器头文
#include <sstream>
#include <random>
#include <algorithm>
#include <iomanip>
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




// =================================================================================
// SECTION 2: File Cache Benchmark Test (新增的模块化测试)
// =================================================================================

// --- 缓存测试所需的全局变量和Hook函数定义 ---

// 指向原始ReadFile的函数指针
static BOOL(WINAPI* TrueReadFile_ForTest)(HANDLE, LPVOID, DWORD, LPDWORD, LPOVERLAPPED) = nullptr;
// 缓存管理器实例
static FileCacheManager g_FileCache_ForTest(100, 65536); // Max 100 entries, cache reads > 64KB

// 用于测试的HookedReadFile版本
BOOL WINAPI HookedReadFile_ForTest(
    HANDLE       hFile,
    LPVOID       lpBuffer,
    DWORD        nNumberOfBytesToRead,
    LPDWORD      lpNumberOfBytesRead,
    LPOVERLAPPED lpOverlapped
) {
    if (g_FileCache_ForTest.TryGetFromCache(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped)) {
        return TRUE;
    }
    // 调用原始函数
    BOOL bSuccess = TrueReadFile_ForTest(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped);
    if (bSuccess && lpNumberOfBytesRead != nullptr && *lpNumberOfBytesRead > 0) {
        g_FileCache_ForTest.PutInCache(hFile, lpBuffer, *lpNumberOfBytesRead, lpOverlapped);
    }
    return bSuccess;
}

// 辅助函数：创建大型测试文件
static bool CreateLargeTestFile(const std::wstring& fileName, long long fileSize) {
    HANDLE hFile = CreateFileW(fileName.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("Failed to create test file for benchmark.");
    }
    std::vector<char> buffer(1024 * 1024, 'B'); // 1MB buffer
    long long bytesWritten = 0;
    DWORD bytesWrittenThisCall;
    while (bytesWritten < fileSize) {
        long long remaining = fileSize - bytesWritten;
        DWORD bytesToWrite = (remaining > buffer.size()) ? static_cast<DWORD>(buffer.size()) : static_cast<DWORD>(remaining);
        if (!WriteFile(hFile, buffer.data(), bytesToWrite, &bytesWrittenThisCall, NULL)) {
            CloseHandle(hFile);
            throw std::runtime_error("Failed to write to test file for benchmark.");
        }
        bytesWritten += bytesWrittenThisCall;
    }
    CloseHandle(hFile);
    return true;
}

// --- 主要的基准测试函数 ---

void run_file_cache_benchmark() {
    try {
        // --- 1. 准备阶段 ---
        const std::wstring TEST_FILE = L"benchmark_comparison_file.bin";
        const long long FILE_SIZE = 512 * 1024 * 1024; // 512 MB
        const DWORD CHUNK_SIZE = 1 * 1024 * 1024;      // 1 MB
        const int NUM_READS = 500; // 总读取次数

        MessageBoxW(NULL, L"即将开始文件读取性能对比测试。\n\n将对比 '带缓存的HookedReadFile' 与 '原始ReadFile' 在重复读取同一文件块时的性能。", L"Benchmark Info", MB_OK | MB_ICONINFORMATION);

        CreateLargeTestFile(TEST_FILE, FILE_SIZE);

        TrueReadFile_ForTest = ::ReadFile; // 设置原始函数指针

        // --- 2. 测试序列 A: 使用我们带缓存的 HookedReadFile ---
        MessageBoxW(NULL, L"第一步：测试带缓存的 HookedReadFile...", L"Benchmark Step 1", MB_OK | MB_ICONINFORMATION);

        LARGE_INTEGER frequency, start, end;
        QueryPerformanceFrequency(&frequency);

        HANDLE hFile = CreateFileW(TEST_FILE.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, NULL);
        if (hFile == INVALID_HANDLE_VALUE) throw std::runtime_error("Failed to open test file (A).");

        PVOID alignedBuffer = _aligned_malloc(CHUNK_SIZE, 4096);
        if (!alignedBuffer) throw std::runtime_error("Failed to allocate aligned memory.");

        OVERLAPPED overlapped = { 0 };
        overlapped.Offset = 128 * 1024 * 1024; // 从128MB处读取

        QueryPerformanceCounter(&start);
        for (int i = 0; i < NUM_READS; ++i) {
            HookedReadFile_ForTest(hFile, alignedBuffer, CHUNK_SIZE, NULL, &overlapped);
        }
        QueryPerformanceCounter(&end);
        double timeWithCache = static_cast<double>(end.QuadPart - start.QuadPart) * 1000.0 / frequency.QuadPart;

        CloseHandle(hFile);

        // --- 3. 测试序列 B: 使用原始的 ReadFile ---
        MessageBoxW(NULL, L"第二步：测试原始的 ReadFile...", L"Benchmark Step 2", MB_OK | MB_ICONINFORMATION);

        hFile = CreateFileW(TEST_FILE.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, NULL);
        if (hFile == INVALID_HANDLE_VALUE) throw std::runtime_error("Failed to open test file (B).");

        overlapped.Offset = 256 * 1024 * 1024; // 从256MB处读取

        QueryPerformanceCounter(&start);
        for (int i = 0; i < NUM_READS; ++i) {
            TrueReadFile_ForTest(hFile, alignedBuffer, CHUNK_SIZE, NULL, &overlapped);
        }
        QueryPerformanceCounter(&end);
        double timeWithoutCache = static_cast<double>(end.QuadPart - start.QuadPart) * 1000.0 / frequency.QuadPart;

        CloseHandle(hFile);
        _aligned_free(alignedBuffer);

        // --- 4. 报告最终结果 (使用 std::wstringstream 修正) ---
        std::wstringstream oss;
        int speedup = (timeWithCache > 0.0001) ? static_cast<int>(timeWithoutCache / timeWithCache) : 0;

        oss << L"性能对比测试完成！\n"
            << L"场景：重复读取同一文件块 " << NUM_READS << L" 次\n\n"
            << L"使用【带缓存的HookedReadFile】:\n"
            << L"总耗时: " << timeWithCache << L" ms\n\n"
            << L"使用【原始ReadFile】:\n"
            << L"总耗时: " << timeWithoutCache << L" ms\n\n"
            << L"--------------------------------------------------\n"
            << L"结论：在此场景下，您的缓存方案快了约 " << speedup << L" 倍。";

        MessageBoxW(NULL, oss.str().c_str(), L"Benchmark Final Results", MB_OK | MB_ICONINFORMATION);

        // --- 5. 清理 ---
        DeleteFileW(TEST_FILE.c_str());
    }
    catch (const std::runtime_error& e) {
        std::string errorMsg = e.what();
        std::wstring wErrorMsg(errorMsg.begin(), errorMsg.end());
        MessageBoxW(NULL, wErrorMsg.c_str(), L"Benchmark Critical Error", MB_OK | MB_ICONERROR);
    }
}

void run_random_access_benchmark() {
    try {
        // --- 1. 准备阶段 ---
        const std::wstring TEST_FILE = L"large_random_access_file.bin";
        // 创建一个比我们的缓存大得多的文件
        const long long FILE_SIZE = 2LL * 1024 * 1024 * 1024; // 2 GB
        const DWORD CHUNK_SIZE = 64 * 1024;      // 64 KB, 满足我们65536的缓存阈值

        // 我们关心文件中的100个“热点”数据块
        const int HOTSPOT_COUNT = 100;
        // 每个热点块重复读取10次
        const int REPEATS_PER_HOTSPOT = 1000;
        const int TOTAL_READS = HOTSPOT_COUNT * REPEATS_PER_HOTSPOT; // 总计1000次读取

        MessageBoxW(NULL, L"即将开始【随机读取】性能对比测试。\n\n场景：在一个2GB的大文件中，对100个随机“热点”位置进行1000次读取。", L"Benchmark Info", MB_OK | MB_ICONINFORMATION);

        CreateLargeTestFile(TEST_FILE, FILE_SIZE);
        TrueReadFile_ForTest = ::ReadFile;

        // --- 生成随机的热点位置 ---
        std::vector<long long> hotspots;
        hotspots.reserve(HOTSPOT_COUNT);

        std::mt19937 rng(std::random_device{}()); // 高质量随机数生成器
        // 确保偏移量在文件范围内且是扇区对齐的
        long long max_offset = FILE_SIZE - CHUNK_SIZE;
        std::uniform_int_distribution<long long> dist(0, max_offset / 512);

        for (int i = 0; i < HOTSPOT_COUNT; ++i) {
            hotspots.push_back(dist(rng) * 512);
        }

        // --- 2. 测试序列 A: 使用我们带缓存的 HookedReadFile ---
        MessageBoxW(NULL, L"第一步：测试带缓存的 HookedReadFile...", L"Benchmark Step 1", MB_OK | MB_ICONINFORMATION);

        LARGE_INTEGER frequency, start, end;
        QueryPerformanceFrequency(&frequency);

        HANDLE hFile = CreateFileW(TEST_FILE.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) throw std::runtime_error("Failed to open test file (A).");

        std::vector<BYTE> buffer(CHUNK_SIZE);
        OVERLAPPED overlapped = { 0 };

        QueryPerformanceCounter(&start);
        for (int i = 0; i < REPEATS_PER_HOTSPOT; ++i) {
            for (long long offset : hotspots) {
                overlapped.Offset = (DWORD)(offset & 0xFFFFFFFF);
                overlapped.OffsetHigh = (DWORD)(offset >> 32);
                HookedReadFile_ForTest(hFile, buffer.data(), CHUNK_SIZE, NULL, &overlapped);
            }
        }
        QueryPerformanceCounter(&end);
        double timeWithCache = static_cast<double>(end.QuadPart - start.QuadPart) * 1000.0 / frequency.QuadPart;

        CloseHandle(hFile);

        // --- 3. 测试序列 B: 使用原始的 ReadFile ---
        MessageBoxW(NULL, L"第二步：测试原始的 ReadFile... (这可能会比较慢)", L"Benchmark Step 2", MB_OK | MB_ICONINFORMATION);

        // 清空我们的缓存，确保测试B是公平的
        g_FileCache_ForTest.clear();

        hFile = CreateFileW(TEST_FILE.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) throw std::runtime_error("Failed to open test file (B).");

        QueryPerformanceCounter(&start);
        for (int i = 0; i < REPEATS_PER_HOTSPOT; ++i) {
            for (long long offset : hotspots) {
                overlapped.Offset = (DWORD)(offset & 0xFFFFFFFF);
                overlapped.OffsetHigh = (DWORD)(offset >> 32);
                TrueReadFile_ForTest(hFile, buffer.data(), CHUNK_SIZE, NULL, &overlapped);
            }
        }
        QueryPerformanceCounter(&end);
        double timeWithoutCache = static_cast<double>(end.QuadPart - start.QuadPart) * 1000.0 / frequency.QuadPart;

        CloseHandle(hFile);

        // --- 4. 报告最终结果 (使用更精确的百分比计算) ---
        std::wstringstream oss;

        // 计算性能提升百分比
        double improvement_percentage = 0.0;
        if (timeWithoutCache > 0.0001) { // 避免被除数为零
            improvement_percentage = ((timeWithoutCache - timeWithCache) / timeWithoutCache) * 100.0;
        }

        // 设置输出流以固定格式显示两位小数
        oss << std::fixed << std::setprecision(2);

        oss << L"【随机读取】性能对比完成！\n"
            << L"场景：在2GB文件中对100个热点进行 " << TOTAL_READS << L" 次随机读取\n\n"
            << L"使用【带缓存的HookedReadFile】:\n"
            << L"总耗时: " << timeWithCache << L" ms\n\n"
            << L"使用【原始ReadFile】:\n"
            << L"总耗时: " << timeWithoutCache << L" ms\n\n"
            << L"--------------------------------------------------\n"
            << L"结论：在此场景下，您的缓存方案使性能提升了约 " << improvement_percentage << L" %。";

        MessageBoxW(NULL, oss.str().c_str(), L"Benchmark Final Results", MB_OK | MB_ICONINFORMATION);

        // --- 5. 清理 ---
        DeleteFileW(TEST_FILE.c_str());
    }
    catch (const std::runtime_error& e) {
        std::string errorMsg = e.what();
        std::wstring wErrorMsg(errorMsg.begin(), errorMsg.end());
        MessageBoxW(NULL, wErrorMsg.c_str(), L"Benchmark Critical Error", MB_OK | MB_ICONERROR);
    }
}

// 辅助函数，用于在一个缓冲区中模拟一次数据变异
static void MutateBuffer(std::vector<char>& buffer) {
    if (buffer.empty()) return;

    // Fuzzing变异可以很复杂，这里我们做一个简单的模拟：
    // 随机选择一个位置，将其值改成一个随机字节。
    static std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<size_t> pos_dist(0, buffer.size() - 1);
    std::uniform_int_distribution<unsigned int> val_dist(0, 255);

    buffer[pos_dist(rng)] = static_cast<char>(val_dist(rng));
}
static void WriteContentToFile(const std::wstring& fileName, const std::string& content) {
    HANDLE hFile = CreateFileW(fileName.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        // 在测试函数中，抛出异常是比返回bool更清晰的方式来指示严重错误
        throw std::runtime_error("Test Error: Failed to open file for writing in WriteContentToFile.");
    }
    DWORD bytesWritten;
    if (!WriteFile(hFile, content.c_str(), static_cast<DWORD>(content.length()), &bytesWritten, NULL)) {
        CloseHandle(hFile);
        throw std::runtime_error("Test Error: Failed to write content to file.");
    }
    CloseHandle(hFile);
}

void run_fuzzing_speed_test() {
    try {
        // --- 1. 准备阶段 ---
        const std::wstring SEED_FILE = L"seed.bin";
        const std::wstring FUZZ_TARGET_FILE = L"fuzz_target.bin"; // 磁盘模式下写入的目标文件
        const int FILE_SIZE = 64 * 1024; // 64 KB, 一个典型的种子文件大小
        const int ITERATIONS = 10000; // Fuzzing循环次数

        // 创建一个初始的“种子文件”
        std::vector<char> seed_buffer(FILE_SIZE, 'A');
        WriteContentToFile(SEED_FILE, std::string(seed_buffer.begin(), seed_buffer.end()));

        MessageBoxW(NULL, L"即将开始【Fuzzing速率对比】测试。\n\n将对比“基于文件”和“内存中”两种模式在10000次变异循环中的性能。", L"Fuzzing Test Info", MB_OK | MB_ICONINFORMATION);

        LARGE_INTEGER frequency, start, end;
        QueryPerformanceFrequency(&frequency);

        // --- 2. 测试方案一: 基于文件的Fuzzing ---
        MessageBoxW(NULL, L"第一步：测试基于文件的Fuzzing... (这会非常慢)", L"Fuzzing Test: Step 1", MB_OK | MB_ICONINFORMATION);

        std::vector<char> disk_buffer = seed_buffer;

        QueryPerformanceCounter(&start);
        for (int i = 0; i < ITERATIONS; ++i) {
            // a. 在内存中变异
            MutateBuffer(disk_buffer);

            // b. 将变异写入磁盘文件
            HANDLE hWrite = CreateFileW(FUZZ_TARGET_FILE.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hWrite == INVALID_HANDLE_VALUE) continue;
            DWORD bytesWritten;
            WriteFile(hWrite, disk_buffer.data(), (DWORD)disk_buffer.size(), &bytesWritten, NULL);
            CloseHandle(hWrite);

            // c. 模拟目标程序从磁盘读取
            HANDLE hRead = CreateFileW(FUZZ_TARGET_FILE.c_str(), GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
            if (hRead == INVALID_HANDLE_VALUE) continue;
            char target_read_buffer[FILE_SIZE]; // 目标程序的读取缓冲区
            DWORD bytesRead;
            ReadFile(hRead, target_read_buffer, FILE_SIZE, &bytesRead, NULL);
            CloseHandle(hRead);
        }
        QueryPerformanceCounter(&end);
        double timeOnDisk = static_cast<double>(end.QuadPart - start.QuadPart) * 1000.0 / frequency.QuadPart;


        // --- 3. 测试方案二: 内存中的Fuzzing ---
        MessageBoxW(NULL, L"第二步：测试内存中的Fuzzing...", L"Fuzzing Test: Step 2", MB_OK | MB_ICONINFORMATION);

        std::vector<char> memory_buffer = seed_buffer;

        QueryPerformanceCounter(&start);
        for (int i = 0; i < ITERATIONS; ++i) {
            // a. 直接在内存中变异
            MutateBuffer(memory_buffer);

            // b. 模拟HookedReadFile直接从内存返回数据
            // 这本质上就是一个memcpy操作
            char target_read_buffer[FILE_SIZE];
            memcpy(target_read_buffer, memory_buffer.data(), memory_buffer.size());
        }
        QueryPerformanceCounter(&end);
        double timeInMemory = static_cast<double>(end.QuadPart - start.QuadPart) * 1000.0 / frequency.QuadPart;

        // --- 4. 报告最终结果 ---
        std::wstringstream oss;
        oss << std::fixed << std::setprecision(2);
        double speedup_factor = (timeInMemory > 0) ? (timeOnDisk / timeInMemory) : 0;

        oss << L"Fuzzing速率对比完成！(循环 " << ITERATIONS << L" 次)\n\n"
            << L"方案一【基于文件】:\n"
            << L"总耗时: " << timeOnDisk << L" ms\n\n"
            << L"方案二【内存中】:\n"
            << L"总耗时: " << timeInMemory << L" ms\n\n"
            << L"--------------------------------------------------\n"
            << L"结论：在此场景下，内存中Fuzzing的速度约是基于文件的 " << static_cast<int>(speedup_factor) << L" 倍！";

        MessageBoxW(NULL, oss.str().c_str(), L"Fuzzing Test Final Results", MB_OK | MB_ICONINFORMATION);

        // --- 5. 清理 ---
        DeleteFileW(SEED_FILE.c_str());
        DeleteFileW(FUZZ_TARGET_FILE.c_str());
    }
    catch (const std::runtime_error& e) {
        // ... 错误处理 ...
    }
}