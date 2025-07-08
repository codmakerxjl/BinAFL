#include"start_process.h"

bool startAndInjectProcess(PROCESS_INFORMATION& pi, const std::wstring& executablePath, const std::wstring& dllPath, const std::wstring& arguments) {
    printf("BinAFL: ---> Entering startAndInjectProcess function\n");
    printf("BinAFL: Target executable path: %ls\n", executablePath.c_str());
    printf("BinAFL: Target arguments: %ls\n", arguments.c_str());
    printf("BinAFL: DLL to inject path: %ls\n", dllPath.c_str());

    // Check if the DLL file exists
    if (!std::filesystem::exists(dllPath)) {
        printf("BinAFL: FATAL ERROR - DLL file to be injected not found: %ls\n", dllPath.c_str());
        return false;
    }

    STARTUPINFOW si = { sizeof(si) };
    std::wstring commandLine = L"\"" + executablePath + L"\" " + arguments;
    std::vector<wchar_t> cmdLineBuff(commandLine.begin(), commandLine.end());
    cmdLineBuff.push_back(L'\0');

    // 1. Create the process in a suspended state (CREATE_SUSPENDED)
    printf("BinAFL: [Step 1/7] Creating process in suspended mode...\n");
    if (!CreateProcessW(NULL,                   // lpApplicationName, 设为 NULL
        cmdLineBuff.data(),     // lpCommandLine, 传入完整的、可写的命令行
        NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &si, &pi)) {
        printf("BinAFL: CreateProcessW failed, error code: %lu\n", GetLastError());
        printf("          Check: 1. Is the target executable path correct? 2. Do you have execution permissions? 3. Do the Fuzzer and target architectures (x86/x64) match?\n");
        return false;
    }
    printf("BinAFL: Process created successfully (PID: %lu, TID: %lu)\n", pi.dwProcessId, pi.dwThreadId);


    // 2. Allocate memory in the target process for the DLL path
    printf("BinAFL: [Step 2/7] Allocating memory in the target process...\n");
    size_t dllPathSize = (dllPath.length() + 1) * sizeof(wchar_t);
    LPVOID remoteMem = VirtualAllocEx(pi.hProcess, NULL, dllPathSize, MEM_COMMIT, PAGE_READWRITE);
    if (!remoteMem) {
        printf("BinAFL: VirtualAllocEx failed, error code: %lu\n", GetLastError());
        printf("         This is often a permission issue. Try 'Run as Administrator'.\n");
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return false;
    }
    printf("BinAFL: Memory allocated successfully at remote address: %p\n", remoteMem);

    // 3. Write the DLL path into the allocated memory of the target process
    printf("BinAFL: [Step 3/7] Writing DLL path to target process memory...\n");
    if (!WriteProcessMemory(pi.hProcess, remoteMem, dllPath.c_str(), dllPathSize, NULL)) {
        printf("BinAFL: WriteProcessMemory failed, error code: %lu\n", GetLastError());
        VirtualFreeEx(pi.hProcess, remoteMem, 0, MEM_RELEASE);
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return false;
    }
    printf("BinAFL: DLL path written successfully.\n");

    // 4. Get the address of the LoadLibraryW function
    printf("BinAFL: [Step 4/7] Getting the address of LoadLibraryW...\n");
    HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
    if (hKernel32 == NULL) {
        printf("BinAFL: GetModuleHandleW('kernel32.dll') failed, error code: %lu\n", GetLastError());
        VirtualFreeEx(pi.hProcess, remoteMem, 0, MEM_RELEASE);
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return false;
    }
    LPTHREAD_START_ROUTINE pLoadLibraryW = (LPTHREAD_START_ROUTINE)GetProcAddress(hKernel32, "LoadLibraryW");
    if (!pLoadLibraryW) {
        printf("BinAFL: GetProcAddress('LoadLibraryW') failed, error code: %lu\n", GetLastError());
        VirtualFreeEx(pi.hProcess, remoteMem, 0, MEM_RELEASE);
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return false;
    }
    printf("BinAFL: Successfully retrieved LoadLibraryW address: %p\n", (void*)pLoadLibraryW);

    // 5. Create a remote thread in the target process to load the DLL
    printf("BinAFL: [Step 5/7] Creating remote thread (to call LoadLibraryW)...\n");
    HANDLE hRemoteThread = CreateRemoteThread(pi.hProcess, NULL, 0, pLoadLibraryW, remoteMem, 0, NULL);
    if (!hRemoteThread) {
        printf("BinAFL: CreateRemoteThread failed, error code: %lu\n", GetLastError());
        printf("         This is the most common point of failure. Possible causes include:\n");
        printf("         - Antivirus or EDR software blocking the operation.\n");
        printf("         - Insufficient process permissions (try 'Run as Administrator').\n");
        printf("         - Architecture mismatch (e.g., 64-bit Fuzzer injecting into a 32-bit process).\n");
        VirtualFreeEx(pi.hProcess, remoteMem, 0, MEM_RELEASE);
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return false;
    }
    printf("BinAFL: Remote thread created successfully.\n");

    // Wait for the injection thread to complete
    printf("BinAFL: [Step 6/7] Waiting for the remote thread to finish execution...\n");
    DWORD waitResult = WaitForSingleObject(hRemoteThread, 5000); // Wait for a maximum of 5 seconds
    if (waitResult == WAIT_TIMEOUT) {
        printf("BinAFL: WARNING - Timed out waiting for the remote thread.\n");
        printf("         This could mean the DLL's DllMain function is taking too long or is deadlocked.\n");
    }
    else {
        printf("BinAFL: Remote thread has finished execution.\n");
    }

    // Get the exit code of the remote thread to help determine if LoadLibrary succeeded
    DWORD remoteThreadExitCode = 0;
    GetExitCodeThread(hRemoteThread, &remoteThreadExitCode);
    if (remoteThreadExitCode == 0) {
        printf("BinAFL: WARNING - Remote thread exit code is 0. This likely means LoadLibraryW failed in the target process!\n");
        printf("         Check: 1. Do all DLL dependencies exist in the target directory? 2. Did DllMain return FALSE?\n");
    }
    else {
        printf("BinAFL: Remote thread returned a non-zero handle (HMODULE). DLL '%ls' appears to be injected successfully.\n", dllPath.c_str());
    }

    // 6. Clean up resources used during injection
    CloseHandle(hRemoteThread);
    VirtualFreeEx(pi.hProcess, remoteMem, 0, MEM_RELEASE);

    // 7. Resume the main thread to let the target program run normally
    printf("BinAFL: [Step 7/7] Resuming the main thread of the target process...\n");
    ResumeThread(pi.hThread);
    printf("BinAFL: Target process has been resumed (PID: %lu)\n", pi.dwProcessId);
    printf("BinAFL: <--- Injection process complete, function returning true\n");

    return true;
}