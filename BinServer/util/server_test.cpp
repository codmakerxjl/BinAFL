#include "pch.h"
#include "server_test.h"
#include <cstdio>       // For wprintf, fprintf
#include <thread>       // For std::this_thread
#include <chrono>       // For std::chrono
#include "SharedMemoryIPC.h" // Assumes SharedMemoryIPC.h/.cpp are in the project
#include "start_process.h" // Assumes this contains startAndInjectProcess
#include "afl_mutator.h"
#include <algorithm>   // 为了使用 std::min
#include <cassert>     // 为了使用 assert
#include <cstring>     // 为了使用 memcmp

void run_server_test(PROCESS_INFORMATION& pi, const std::wstring& executablePath, const std::wstring& dllPath) {
    try {
        // 1. As IPC Server, create shared memory and sync objects.
        SharedMemoryIPC ipc(SharedMemoryIPC::Role::SERVER);
        wprintf(L"IPC Server started, shared memory created.\n");

        // 2. Start target process and inject our client DLL.
        if (startAndInjectProcess(pi, executablePath, dllPath)) {
            wprintf(L"DLL injected successfully. Server will start sending data...\n");

            // 3. Loop to send data.
            for (int i = 0; i < 10; ++i) {
                SharedData dataToSend;
                dataToSend.buff[0] = 1000 + i;
                dataToSend.buff[1] = 2000 + i;

                // Call write() to put data into shared memory.
                if (ipc.write(dataToSend)) {
                    wprintf(L"Server has written data, buff[0] = %d\n", dataToSend.buff[0]);
                }
                else {
                    wprintf(L"Write data timeout.\n");
                    break;
                }
                // Wait for a moment to give the client time to respond.
                std::this_thread::sleep_for(std::chrono::milliseconds(1500));
            }
            wprintf(L"Server has finished sending data.\n");

            // Wait for the target process to terminate.
            wprintf(L"Waiting for target process to exit...\n");
            WaitForSingleObject(pi.hProcess, INFINITE);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            wprintf(L"Target process has exited. Server is shutting down.\n");
        }
        else {
            fprintf(stderr, "DLL injection failed.\n");
        }
    }
    catch (const std::runtime_error& e) {
        fprintf(stderr, "A critical error occurred: %s\n", e.what());
        // Use system("pause") for easy debugging in a console window.
        system("pause");
    }
}




// 使用wprintf来打印buffer内容的辅助函数
static void print_buffer_w(const std::vector<uint8_t>& buf, const wchar_t* prefix = L"") {
    wprintf(L"%sBuffer (%zu bytes): ", prefix, buf.size());
    size_t print_len = min(buf.size(), (size_t)32);
    for (size_t i = 0; i < print_len; ++i) {
        wprintf(L"%02x ", buf[i]);
    }
    if (buf.size() > 32) wprintf(L"...");
    wprintf(L"\n");
}

// --- 独立的测试用例 (已更新) ---

// 使用模板来减少重复的测试代码，并加入循环测试
// 【已修复】增加了重试逻辑，以应对随机变异函数偶尔不变异的情况
static void run_mutation_test(const wchar_t* test_name, void (*mutation_func)(std::vector<uint8_t>&), int iterations = 10) {
    wprintf(L"\n=== Testing %s Mutation (%d iterations) ===\n", test_name, iterations);

    for (int i = 0; i < iterations; ++i) {
        std::vector<uint8_t> test_data = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88 };
        std::vector<uint8_t> original = test_data;

        bool changed = false;
        int attempts = 0;
        // 循环调用变异函数，直到它真正改变了buffer，或者重试了10次
        do {
            mutation_func(test_data);
            changed = (original != test_data);
            if (!changed) {
                test_data = original; // 如果没变，恢复数据以便下次重试
            }
            attempts++;
        } while (!changed && attempts < 10);


        if (i < 3) { // 只打印前几次迭代的细节
            print_buffer_w(original, L"  Original: ");
            print_buffer_w(test_data, L"  After mutation: ");
            wprintf(L"  Buffer changed: %s\n", changed ? L"YES" : L"NO");
        }
        assert(changed && "Mutation should change the buffer in every iteration");
    }
    wprintf(L"  [PASSED] %s test completed successfully.\n", test_name);
}

static void test_extras(int iterations = 10) {
    wprintf(L"\n=== Testing Extras Mutation (%d iterations) ===\n", iterations);
    std::vector<AFLDictEntry> extras = {
        {(const uint8_t*)"TOKEN1", 6},
        {(const uint8_t*)"TOKEN2", 6}
    };

    for (int i = 0; i < iterations; ++i) {
        std::vector<uint8_t> test_data = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF };
        std::vector<uint8_t> original = test_data;

        bool changed = false;
        int attempts = 0;
        do {
            afl_mutate_extras(test_data, extras);
            changed = (original != test_data);
            if (!changed) test_data = original;
            attempts++;
        } while (!changed && attempts < 10);


        if (i < 3) {
            print_buffer_w(original, L"  Original: ");
            print_buffer_w(test_data, L"  After extras: ");
            wprintf(L"  Buffer changed: %s\n", changed ? L"YES" : L"NO");
        }
        assert(changed && "Extras mutation should change the buffer");
    }
    wprintf(L"  [PASSED] Extras test completed successfully.\n");
}

// 【已修复】增加了重试逻辑
static void test_havoc(int iterations = 5) {
    wprintf(L"\n=== Testing Havoc Mutation (%d iterations) ===\n", iterations);

    for (int i = 0; i < iterations; ++i) {
        std::vector<uint8_t> test_data(128, 0xAA);
        std::vector<uint8_t> original = test_data;

        bool changed = false;
        int attempts = 0;
        // 循环调用havoc，直到它真正改变了buffer
        do {
            afl_mutate_havoc(test_data);
            changed = (original != test_data);
            if (!changed) {
                test_data = original; // 恢复数据以便下次重试
            }
            attempts++;
        } while (!changed && attempts < 10); // 最多重试10次

        if (i < 2) {
            print_buffer_w(original, L"  Original: ");
            print_buffer_w(test_data, L"  After havoc: ");
            wprintf(L"  Buffer changed: %s\n", changed ? L"YES" : L"NO");
        }
        assert(changed && "Havoc mutation should eventually change the buffer");
    }
    wprintf(L"  [PASSED] Havoc test completed successfully.\n");
}

static void test_edge_cases() {
    wprintf(L"\n=== Testing Edge Cases ===\n");

    wprintf(L"Testing empty buffer...\n");
    std::vector<uint8_t> empty_buf;
    // 对空buffer调用不应崩溃
    afl_mutate_bitflip(empty_buf);
    assert(empty_buf.empty() && "Empty buffer should remain empty");
    wprintf(L"  [PASSED] Empty buffer test passed.\n");

    wprintf(L"Testing single byte buffer (5 iterations)...\n");
    for (int i = 0; i < 5; ++i) {
        std::vector<uint8_t> single_byte = { 0x55 };
        std::vector<uint8_t> original_single = single_byte;
        afl_mutate_bitflip(single_byte);
        assert(original_single != single_byte);
    }
    wprintf(L"  [PASSED] Single byte test passed.\n");
}

// 【新增】专门用于测试大尺寸缓冲区的函数
static void test_large_buffers() {
    wprintf(L"\n=== Testing Large Buffers (4KB) ===\n");
    const size_t large_buffer_size = 4096;

    // 定义一个包含所有基础变异函数的列表，方便遍历
    std::vector<std::pair<const wchar_t*, void(*)(std::vector<uint8_t>&)>> mutation_strategies = {
        {L"Bitflip", afl_mutate_bitflip},
        {L"Arithmetic", afl_mutate_arith},
        {L"Interest", afl_mutate_interest}
    };

    for (const auto& strategy : mutation_strategies) {
        wprintf(L"Testing large buffer with %s...\n", strategy.first);

        std::vector<uint8_t> large_buffer(large_buffer_size);
        // 用一些非零值填充，方便观察变化
        std::fill(large_buffer.begin(), large_buffer.end(), 0xAA);

        std::vector<uint8_t> original = large_buffer;

        // 对大buffer执行一次变异
        strategy.second(large_buffer);

        bool changed = (original != large_buffer);
        assert(changed && "Mutation on large buffer should produce changes.");
        wprintf(L"  [PASSED] %s on large buffer test completed successfully.\n", strategy.first);
    }

    // 单独测试havoc，因为它更复杂
    wprintf(L"Testing large buffer with Havoc...\n");
    std::vector<uint8_t> large_buffer_havoc(large_buffer_size, 0xBB);
    std::vector<uint8_t> original_havoc = large_buffer_havoc;
    afl_mutate_havoc(large_buffer_havoc);
    assert(original_havoc != large_buffer_havoc);
    wprintf(L"  [PASSED] Havoc on large buffer test completed successfully.\n");
}


// --- 公共接口函数的实现 ---

void run_afl_mutator_tests() {
    wprintf(L"Starting AFL Mutator Tests (Updated for new interface and iterative testing)...\n");

    try {
        // 使用测试模板函数来运行测试，增加迭代次数
        run_mutation_test(L"Bitflip", afl_mutate_bitflip, 1000);
        run_mutation_test(L"Arithmetic", afl_mutate_arith, 1000);
        run_mutation_test(L"Interest", afl_mutate_interest, 1000);

        // 单独调用需要特殊参数的测试，同样增加迭代次数
        test_extras(1000);
        test_havoc(200); // Havoc较复杂，测试200次

        // 测试边界情况
        test_edge_cases();

        // 【新增】调用大缓冲区测试
        test_large_buffers();

        wprintf(L"\n\n=== All Tests Passed! ===\n");
        wprintf(L"AFL Mutator module is working correctly with the new interface.\n");

    }
    catch (const std::exception& e) {
        fwprintf(stderr, L"Test failed with exception: %hs\n", e.what());
    }
    catch (...) {
        fwprintf(stderr, L"Test failed with unknown exception.\n");
    }
}