#include "pch.h"
#include "client_test.h"
#include <windows.h>
#include <string>
#include "SharedMemoryIPC.h" // Assumes SharedMemoryIPC.h/.cpp are in the project
#include "FileCacheManager.h"  // ����֮ǰ�����Ļ��������ͷ��
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
// SECTION 2: File Cache Benchmark Test (������ģ�黯����)
// =================================================================================

// --- ������������ȫ�ֱ�����Hook�������� ---

// ָ��ԭʼReadFile�ĺ���ָ��
static BOOL(WINAPI* TrueReadFile_ForTest)(HANDLE, LPVOID, DWORD, LPDWORD, LPOVERLAPPED) = nullptr;
// ���������ʵ��
static FileCacheManager g_FileCache_ForTest(100, 65536); // Max 100 entries, cache reads > 64KB

// ���ڲ��Ե�HookedReadFile�汾
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
    // ����ԭʼ����
    BOOL bSuccess = TrueReadFile_ForTest(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped);
    if (bSuccess && lpNumberOfBytesRead != nullptr && *lpNumberOfBytesRead > 0) {
        g_FileCache_ForTest.PutInCache(hFile, lpBuffer, *lpNumberOfBytesRead, lpOverlapped);
    }
    return bSuccess;
}

// �����������������Ͳ����ļ�
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

// --- ��Ҫ�Ļ�׼���Ժ��� ---

void run_file_cache_benchmark() {
    try {
        // --- 1. ׼���׶� ---
        const std::wstring TEST_FILE = L"benchmark_comparison_file.bin";
        const long long FILE_SIZE = 512 * 1024 * 1024; // 512 MB
        const DWORD CHUNK_SIZE = 1 * 1024 * 1024;      // 1 MB
        const int NUM_READS = 500; // �ܶ�ȡ����

        MessageBoxW(NULL, L"������ʼ�ļ���ȡ���ܶԱȲ��ԡ�\n\n���Ա� '�������HookedReadFile' �� 'ԭʼReadFile' ���ظ���ȡͬһ�ļ���ʱ�����ܡ�", L"Benchmark Info", MB_OK | MB_ICONINFORMATION);

        CreateLargeTestFile(TEST_FILE, FILE_SIZE);

        TrueReadFile_ForTest = ::ReadFile; // ����ԭʼ����ָ��

        // --- 2. �������� A: ʹ�����Ǵ������ HookedReadFile ---
        MessageBoxW(NULL, L"��һ�������Դ������ HookedReadFile...", L"Benchmark Step 1", MB_OK | MB_ICONINFORMATION);

        LARGE_INTEGER frequency, start, end;
        QueryPerformanceFrequency(&frequency);

        HANDLE hFile = CreateFileW(TEST_FILE.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, NULL);
        if (hFile == INVALID_HANDLE_VALUE) throw std::runtime_error("Failed to open test file (A).");

        PVOID alignedBuffer = _aligned_malloc(CHUNK_SIZE, 4096);
        if (!alignedBuffer) throw std::runtime_error("Failed to allocate aligned memory.");

        OVERLAPPED overlapped = { 0 };
        overlapped.Offset = 128 * 1024 * 1024; // ��128MB����ȡ

        QueryPerformanceCounter(&start);
        for (int i = 0; i < NUM_READS; ++i) {
            HookedReadFile_ForTest(hFile, alignedBuffer, CHUNK_SIZE, NULL, &overlapped);
        }
        QueryPerformanceCounter(&end);
        double timeWithCache = static_cast<double>(end.QuadPart - start.QuadPart) * 1000.0 / frequency.QuadPart;

        CloseHandle(hFile);

        // --- 3. �������� B: ʹ��ԭʼ�� ReadFile ---
        MessageBoxW(NULL, L"�ڶ���������ԭʼ�� ReadFile...", L"Benchmark Step 2", MB_OK | MB_ICONINFORMATION);

        hFile = CreateFileW(TEST_FILE.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, NULL);
        if (hFile == INVALID_HANDLE_VALUE) throw std::runtime_error("Failed to open test file (B).");

        overlapped.Offset = 256 * 1024 * 1024; // ��256MB����ȡ

        QueryPerformanceCounter(&start);
        for (int i = 0; i < NUM_READS; ++i) {
            TrueReadFile_ForTest(hFile, alignedBuffer, CHUNK_SIZE, NULL, &overlapped);
        }
        QueryPerformanceCounter(&end);
        double timeWithoutCache = static_cast<double>(end.QuadPart - start.QuadPart) * 1000.0 / frequency.QuadPart;

        CloseHandle(hFile);
        _aligned_free(alignedBuffer);

        // --- 4. �������ս�� (ʹ�� std::wstringstream ����) ---
        std::wstringstream oss;
        int speedup = (timeWithCache > 0.0001) ? static_cast<int>(timeWithoutCache / timeWithCache) : 0;

        oss << L"���ܶԱȲ�����ɣ�\n"
            << L"�������ظ���ȡͬһ�ļ��� " << NUM_READS << L" ��\n\n"
            << L"ʹ�á��������HookedReadFile��:\n"
            << L"�ܺ�ʱ: " << timeWithCache << L" ms\n\n"
            << L"ʹ�á�ԭʼReadFile��:\n"
            << L"�ܺ�ʱ: " << timeWithoutCache << L" ms\n\n"
            << L"--------------------------------------------------\n"
            << L"���ۣ��ڴ˳����£����Ļ��淽������Լ " << speedup << L" ����";

        MessageBoxW(NULL, oss.str().c_str(), L"Benchmark Final Results", MB_OK | MB_ICONINFORMATION);

        // --- 5. ���� ---
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
        // --- 1. ׼���׶� ---
        const std::wstring TEST_FILE = L"large_random_access_file.bin";
        // ����һ�������ǵĻ����ö���ļ�
        const long long FILE_SIZE = 2LL * 1024 * 1024 * 1024; // 2 GB
        const DWORD CHUNK_SIZE = 64 * 1024;      // 64 KB, ��������65536�Ļ�����ֵ

        // ���ǹ����ļ��е�100�����ȵ㡱���ݿ�
        const int HOTSPOT_COUNT = 100;
        // ÿ���ȵ���ظ���ȡ10��
        const int REPEATS_PER_HOTSPOT = 1000;
        const int TOTAL_READS = HOTSPOT_COUNT * REPEATS_PER_HOTSPOT; // �ܼ�1000�ζ�ȡ

        MessageBoxW(NULL, L"������ʼ�������ȡ�����ܶԱȲ��ԡ�\n\n��������һ��2GB�Ĵ��ļ��У���100��������ȵ㡱λ�ý���1000�ζ�ȡ��", L"Benchmark Info", MB_OK | MB_ICONINFORMATION);

        CreateLargeTestFile(TEST_FILE, FILE_SIZE);
        TrueReadFile_ForTest = ::ReadFile;

        // --- ����������ȵ�λ�� ---
        std::vector<long long> hotspots;
        hotspots.reserve(HOTSPOT_COUNT);

        std::mt19937 rng(std::random_device{}()); // �����������������
        // ȷ��ƫ�������ļ���Χ���������������
        long long max_offset = FILE_SIZE - CHUNK_SIZE;
        std::uniform_int_distribution<long long> dist(0, max_offset / 512);

        for (int i = 0; i < HOTSPOT_COUNT; ++i) {
            hotspots.push_back(dist(rng) * 512);
        }

        // --- 2. �������� A: ʹ�����Ǵ������ HookedReadFile ---
        MessageBoxW(NULL, L"��һ�������Դ������ HookedReadFile...", L"Benchmark Step 1", MB_OK | MB_ICONINFORMATION);

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

        // --- 3. �������� B: ʹ��ԭʼ�� ReadFile ---
        MessageBoxW(NULL, L"�ڶ���������ԭʼ�� ReadFile... (����ܻ�Ƚ���)", L"Benchmark Step 2", MB_OK | MB_ICONINFORMATION);

        // ������ǵĻ��棬ȷ������B�ǹ�ƽ��
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

        // --- 4. �������ս�� (ʹ�ø���ȷ�İٷֱȼ���) ---
        std::wstringstream oss;

        // �������������ٷֱ�
        double improvement_percentage = 0.0;
        if (timeWithoutCache > 0.0001) { // ���ⱻ����Ϊ��
            improvement_percentage = ((timeWithoutCache - timeWithCache) / timeWithoutCache) * 100.0;
        }

        // ����������Թ̶���ʽ��ʾ��λС��
        oss << std::fixed << std::setprecision(2);

        oss << L"�������ȡ�����ܶԱ���ɣ�\n"
            << L"��������2GB�ļ��ж�100���ȵ���� " << TOTAL_READS << L" �������ȡ\n\n"
            << L"ʹ�á��������HookedReadFile��:\n"
            << L"�ܺ�ʱ: " << timeWithCache << L" ms\n\n"
            << L"ʹ�á�ԭʼReadFile��:\n"
            << L"�ܺ�ʱ: " << timeWithoutCache << L" ms\n\n"
            << L"--------------------------------------------------\n"
            << L"���ۣ��ڴ˳����£����Ļ��淽��ʹ����������Լ " << improvement_percentage << L" %��";

        MessageBoxW(NULL, oss.str().c_str(), L"Benchmark Final Results", MB_OK | MB_ICONINFORMATION);

        // --- 5. ���� ---
        DeleteFileW(TEST_FILE.c_str());
    }
    catch (const std::runtime_error& e) {
        std::string errorMsg = e.what();
        std::wstring wErrorMsg(errorMsg.begin(), errorMsg.end());
        MessageBoxW(NULL, wErrorMsg.c_str(), L"Benchmark Critical Error", MB_OK | MB_ICONERROR);
    }
}

// ����������������һ����������ģ��һ�����ݱ���
static void MutateBuffer(std::vector<char>& buffer) {
    if (buffer.empty()) return;

    // Fuzzing������Ժܸ��ӣ�����������һ���򵥵�ģ�⣺
    // ���ѡ��һ��λ�ã�����ֵ�ĳ�һ������ֽڡ�
    static std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<size_t> pos_dist(0, buffer.size() - 1);
    std::uniform_int_distribution<unsigned int> val_dist(0, 255);

    buffer[pos_dist(rng)] = static_cast<char>(val_dist(rng));
}
static void WriteContentToFile(const std::wstring& fileName, const std::string& content) {
    HANDLE hFile = CreateFileW(fileName.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        // �ڲ��Ժ����У��׳��쳣�Ǳȷ���bool�������ķ�ʽ��ָʾ���ش���
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
        // --- 1. ׼���׶� ---
        const std::wstring SEED_FILE = L"seed.bin";
        const std::wstring FUZZ_TARGET_FILE = L"fuzz_target.bin"; // ����ģʽ��д���Ŀ���ļ�
        const int FILE_SIZE = 64 * 1024; // 64 KB, һ�����͵������ļ���С
        const int ITERATIONS = 10000; // Fuzzingѭ������

        // ����һ����ʼ�ġ������ļ���
        std::vector<char> seed_buffer(FILE_SIZE, 'A');
        WriteContentToFile(SEED_FILE, std::string(seed_buffer.begin(), seed_buffer.end()));

        MessageBoxW(NULL, L"������ʼ��Fuzzing���ʶԱȡ����ԡ�\n\n���Աȡ������ļ����͡��ڴ��С�����ģʽ��10000�α���ѭ���е����ܡ�", L"Fuzzing Test Info", MB_OK | MB_ICONINFORMATION);

        LARGE_INTEGER frequency, start, end;
        QueryPerformanceFrequency(&frequency);

        // --- 2. ���Է���һ: �����ļ���Fuzzing ---
        MessageBoxW(NULL, L"��һ�������Ի����ļ���Fuzzing... (���ǳ���)", L"Fuzzing Test: Step 1", MB_OK | MB_ICONINFORMATION);

        std::vector<char> disk_buffer = seed_buffer;

        QueryPerformanceCounter(&start);
        for (int i = 0; i < ITERATIONS; ++i) {
            // a. ���ڴ��б���
            MutateBuffer(disk_buffer);

            // b. ������д������ļ�
            HANDLE hWrite = CreateFileW(FUZZ_TARGET_FILE.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hWrite == INVALID_HANDLE_VALUE) continue;
            DWORD bytesWritten;
            WriteFile(hWrite, disk_buffer.data(), (DWORD)disk_buffer.size(), &bytesWritten, NULL);
            CloseHandle(hWrite);

            // c. ģ��Ŀ�����Ӵ��̶�ȡ
            HANDLE hRead = CreateFileW(FUZZ_TARGET_FILE.c_str(), GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
            if (hRead == INVALID_HANDLE_VALUE) continue;
            char target_read_buffer[FILE_SIZE]; // Ŀ�����Ķ�ȡ������
            DWORD bytesRead;
            ReadFile(hRead, target_read_buffer, FILE_SIZE, &bytesRead, NULL);
            CloseHandle(hRead);
        }
        QueryPerformanceCounter(&end);
        double timeOnDisk = static_cast<double>(end.QuadPart - start.QuadPart) * 1000.0 / frequency.QuadPart;


        // --- 3. ���Է�����: �ڴ��е�Fuzzing ---
        MessageBoxW(NULL, L"�ڶ����������ڴ��е�Fuzzing...", L"Fuzzing Test: Step 2", MB_OK | MB_ICONINFORMATION);

        std::vector<char> memory_buffer = seed_buffer;

        QueryPerformanceCounter(&start);
        for (int i = 0; i < ITERATIONS; ++i) {
            // a. ֱ�����ڴ��б���
            MutateBuffer(memory_buffer);

            // b. ģ��HookedReadFileֱ�Ӵ��ڴ淵������
            // �Ȿ���Ͼ���һ��memcpy����
            char target_read_buffer[FILE_SIZE];
            memcpy(target_read_buffer, memory_buffer.data(), memory_buffer.size());
        }
        QueryPerformanceCounter(&end);
        double timeInMemory = static_cast<double>(end.QuadPart - start.QuadPart) * 1000.0 / frequency.QuadPart;

        // --- 4. �������ս�� ---
        std::wstringstream oss;
        oss << std::fixed << std::setprecision(2);
        double speedup_factor = (timeInMemory > 0) ? (timeOnDisk / timeInMemory) : 0;

        oss << L"Fuzzing���ʶԱ���ɣ�(ѭ�� " << ITERATIONS << L" ��)\n\n"
            << L"����һ�������ļ���:\n"
            << L"�ܺ�ʱ: " << timeOnDisk << L" ms\n\n"
            << L"���������ڴ��С�:\n"
            << L"�ܺ�ʱ: " << timeInMemory << L" ms\n\n"
            << L"--------------------------------------------------\n"
            << L"���ۣ��ڴ˳����£��ڴ���Fuzzing���ٶ�Լ�ǻ����ļ��� " << static_cast<int>(speedup_factor) << L" ����";

        MessageBoxW(NULL, oss.str().c_str(), L"Fuzzing Test Final Results", MB_OK | MB_ICONINFORMATION);

        // --- 5. ���� ---
        DeleteFileW(SEED_FILE.c_str());
        DeleteFileW(FUZZ_TARGET_FILE.c_str());
    }
    catch (const std::runtime_error& e) {
        // ... ������ ...
    }
}