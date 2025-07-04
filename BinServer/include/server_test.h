#pragma once
#include <windows.h>
#include <string>

/**
 * @brief Runs the entire server-side logic: creates IPC, injects DLL, and sends data.
 * @param pi Reference to PROCESS_INFORMATION structure to be filled by process creation.
 * @param executablePath Path to the target executable.
 * @param dllPath Path to the DLL to be injected.
 */
void run_server_test(PROCESS_INFORMATION& pi, const std::wstring& executablePath, const std::wstring& dllPath);
void run_afl_mutator_tests();