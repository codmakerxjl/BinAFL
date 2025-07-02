// pch.cpp: 与预编译标头对应的源文件

#include "pch.h"
#include <tlhelp32.h>
#include <locale.h> // 需要包含这个头文件
#include <intrin.h>
//用测试重放速度定义的两个窗口

HWND hwndProcessTop = NULL;
HWND hwndReplyWindow = NULL;
void sendOpenMessage(){
	PostMessage(hwndReplyWindow, WM_COMMAND, autoCAD_replay_lparam, autoCAD_replay_wparam);
}
// Function to resume all threads of a process by its PID
bool resumeProcess(DWORD processId) {
	HANDLE hThreadSnap = INVALID_HANDLE_VALUE;
	THREADENTRY32 te32;
	bool all_resumed = true;

	// Take a snapshot of all running threads
	hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	if (hThreadSnap == INVALID_HANDLE_VALUE) {
		std::cerr << "CreateToolhelp32Snapshot (of threads) failed. Error: " << GetLastError() << std::endl;
		return false;
	}

	// Retrieve information about the first thread
	te32.dwSize = sizeof(THREADENTRY32);
	if (!Thread32First(hThreadSnap, &te32)) {
		std::cerr << "Thread32First failed. Error: " << GetLastError() << std::endl;
		CloseHandle(hThreadSnap);
		return false;
	}

	// Now walk the thread list and resume threads for the target process
	do {
		if (te32.th32OwnerProcessID == processId) {
			HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te32.th32ThreadID);
			if (hThread == NULL) {
				std::cerr << "OpenThread failed for thread " << te32.th32ThreadID << ". Error: " << GetLastError() << std::endl;
				all_resumed = false;
				continue;
			}

			DWORD suspendCount = ResumeThread(hThread);
			if (suspendCount == (DWORD)-1) {
				std::cerr << "ResumeThread failed for thread " << te32.th32ThreadID << ". Error: " << GetLastError() << std::endl;
				all_resumed = false;
			}
			else {
				std::cout << "Resumed thread " << te32.th32ThreadID << ". Previous suspend count: " << suspendCount << std::endl;
			}
			CloseHandle(hThread);
		}
	} while (Thread32Next(hThreadSnap, &te32));

	CloseHandle(hThreadSnap);
	return all_resumed;
}

void ResumeProcess(DWORD processID) {
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	if (hSnapshot == INVALID_HANDLE_VALUE) return;

	THREADENTRY32 te;
	te.dwSize = sizeof(THREADENTRY32);

	if (Thread32First(hSnapshot, &te)) {
		do {
			if (te.th32OwnerProcessID == processID) {
				HANDLE hThread = OpenThread(THREAD_ALL_ACCESS, FALSE, te.th32ThreadID);
				if (hThread) {
					ResumeThread(hThread);
					CloseHandle(hThread);
				}
			}
		} while (Thread32Next(hSnapshot, &te));
	}

	CloseHandle(hSnapshot);
}

int fork()
{

	OutputDebugStringA("[Debugxxxxxxxx]: success execute to fork func");
	DWORD parent_pid = GetCurrentProcessId();

	HMODULE mod;
	RtlCloneUserProcess_f clone_p;
	PRTL_USER_PROCESS_INFORMATION process_info =
		(PRTL_USER_PROCESS_INFORMATION)malloc(sizeof(RTL_USER_PROCESS_INFORMATION));
	if (!process_info) {
		return -ENOMEM;  // 内存不足处理
	}
	memset(process_info, 0, sizeof(RTL_USER_PROCESS_INFORMATION));
	process_info->Size = sizeof(RTL_USER_PROCESS_INFORMATION);
	NTSTATUS result;

	mod = GetModuleHandle(L"ntdll.dll");
	if (!mod)
		return -ENOSYS;

	clone_p = (RtlCloneUserProcess_f)GetProcAddress(mod, "RtlCloneUserProcess");
	if (clone_p == NULL)
		return -ENOSYS;
	
	/* lets do this */
	result = clone_p(RTL_CLONE_PROCESS_FLAGS_CREATE_SUSPENDED | RTL_CLONE_PROCESS_FLAGS_INHERIT_HANDLES, NULL, NULL, NULL, process_info);
	if (result == RTL_CLONE_PARENT)
	{
		HANDLE me = 0, hp = 0, ht = 0;
		DWORD pi, ti;
		FILE* logFile = NULL; // 用于写入日志的文件指针
		errno_t fileErr;
		DWORD lastError; // 用于存储 GetLastError() 的返回值

		// 提前打开日志文件，使用 "w" 模式会覆盖旧内容
		// 如果希望追加内容，可以使用 "a" 模式
		fileErr = fopen_s(&logFile, "E:\\ICT\\masterRecording\\harness\\DLL-Injector\\Source\\x64\\Release\\process_thread_info.txt", "w");
		if (fileErr != 0 || logFile == NULL) {
			// 如果文件打开失败，后续的日志信息会尝试通过 printf 输出到控制台
			printf("Can not open file errno: %d。\n", fileErr);
			logFile = NULL; // 确保 logFile 为 NULL
		}
		else {
			fprintf(logFile, "open file sucess 。\n");
		}

		me = GetCurrentProcess(); // 这行似乎没有直接用于后续逻辑，但保留
		pi = (DWORD)process_info->ClientId.UniqueProcess;
		ti = (DWORD)process_info->ClientId.UniqueThread;

		if (logFile) {
			fprintf(logFile, "child:ID (pi): %lu, child thread ID (ti): %lu\n", pi, ti);
		}
		else {
			printf("child:ID (pi): %lu, child thread ID (ti): %lu\n", pi, ti);
		}

		// 尝试打开子进程
		hp = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pi);
		if (hp == NULL) {
			lastError = GetLastError();
			if (logFile) {
				fprintf(logFile, " OpenProcess failed %lu\n", lastError);
				fclose(logFile); // 关闭已打开的文件
			}
			else {
				printf(" OpenProcess failed  %lu\n", lastError);
			}
			return -1; // 示例错误返回
		}
		if (logFile) {
			fprintf(logFile, " OpenProcess sucess (PID: %lu, Handle: %p)\n", pi, hp);
		}
		else {
			printf("OpenProcess sucess (PID: %lu, Handle: %p)\n", pi, hp);
		}

		// 尝试打开子进程的主线程
		ht = OpenThread(THREAD_ALL_ACCESS, FALSE, ti);
		if (ht == NULL) {
			lastError = GetLastError();
			if (logFile) {
				fprintf(logFile, "OpenThread failed errno: %lu\n", lastError);
			}
			else {
				printf("OpenThread failed errno: %lu\n", lastError);
			}
			CloseHandle(hp);
			if (logFile) fclose(logFile); // 关闭已打开的文件
			return -1; // 示例错误返回
		}
		if (logFile) {
			fprintf(logFile, "OpenThread sucess (TID: %lu, Handle: %p)\n", ti, ht);
		}
		else {
			printf("OpenThread sucess (TID: %lu, Handle: %p)\n", ti, ht);
		}

		// 尝试恢复子线程
		if (logFile) {
			fprintf(logFile, "try to resume child (TID: %lu, Handle: %p)\n", ti, ht);
		}
		else {
			printf("try to resume child (TID: %lu, Handle: %p)\n", ti, ht);
		}

		DWORD resumeResult = ResumeThread(ht); // ResumeThread 返回的是之前的挂起计数，或者 (DWORD)-1 表示失败
		if (resumeResult == (DWORD)-1) {
			lastError = GetLastError();
			if (logFile) {
				fprintf(logFile, "ResumeThread failed errno %lu\n", lastError);
			}
			else {
				printf("ResumeThread failed errno %lu\n", lastError);
			}
			// 即使 ResumeThread 失败，也应该按流程关闭句柄
		}
		else {
			// resumeResult 是线程之前的挂起计数。如果为0，表示线程未挂起或已运行。
			// 如果为1，表示线程被挂起一次，现已运行。如果大于1，表示多次挂起，现减少一次。
			if (logFile) {
				fprintf(logFile, "ResumeThread sucess.resume result: %lu\n", resumeResult);
			}
			else {
				printf("ResumeThread sucess.resume result: %lu\n", resumeResult);
			}
		}

		// 原始文件写入部分（现在是写入到已经打开的 logFile）
		if (logFile) {
			fprintf(logFile, "target\n");
			fprintf(logFile, "child Process ID : %lu\n", pi);
			fprintf(logFile, "child Thread ID : %lu\n", ti);
			fprintf(logFile, "----end----\n");
		}
		// 注意：此时 logFile 仍然是打开的，将在最后统一关闭

		CloseHandle(ht);
		CloseHandle(hp);

		if (logFile != NULL) {
			fclose(logFile); // 完成所有操作后关闭日志文件
		}


		return (int)pi;
	}
	 if (result == RTL_CLONE_CHILD)
	{
		OutputDebugStringA("[Debugxxxxxxxxxx]:I am a child");
		if (!ConnectCsrChild()) { //把子进程链接到csrss
			OutputDebugStringA("[Debugxxxxxxxxxx]:failed to link csrss");
			ExitProcess(1);
		}
		

		return 0;

	}
	else {
		 DebugPrint("[Debugxxxxxxx ]Result by clone: 0x%08X\n", result);  // 打印为十六进制格式
		return -1;
	}


	/* NOTREACHED */
	return -1;
}


void DebugPrint(const char* format, ...) {
	char buffer[256];  // 缓冲区，用于存储格式化字符串
	va_list args;

	va_start(args, format);  // 初始化参数列表
	vsnprintf(buffer, sizeof(buffer), format, args);  // 格式化字符串
	va_end(args);

	OutputDebugStringA(buffer);  // 输出到调试器
}




#include <winnt.h> // For IMAGE_NT_HEADERS64, IMAGE_SECTION_HEADER, etc.

// 函数：修改进程所有段的内存保护为可读、可写、可执行 (64-bit)
bool MakeAllSectionsExecuteReadWrite64() {
	// 1. 获取当前进程的模块句柄 (即进程的基地址)
	HMODULE hModule = GetModuleHandle(NULL);
	if (hModule == NULL) {
		// 错误处理: 无法获取模块句柄
		return false;
	}

	// 2. 获取DOS头
	PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)hModule;
	if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
		// 错误处理: 无效的DOS头
		return false;
	}

	// 3. 获取NT头 (64-bit)
	PIMAGE_NT_HEADERS64 pNtHeaders = (PIMAGE_NT_HEADERS64)((BYTE*)hModule + pDosHeader->e_lfanew);
	if (pNtHeaders->Signature != IMAGE_NT_SIGNATURE) {
		// 错误处理: 无效的NT头
		return false;
	}

	// 检查是否真的是64位PE文件 (可选，但推荐)
	if (pNtHeaders->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
		// 错误处理: 不是一个64位PE文件的可选头
		return false;
	}

	// 4. 获取第一个节表项 (Section Header)
	PIMAGE_SECTION_HEADER pSectionHeader = IMAGE_FIRST_SECTION(pNtHeaders);

	// 5. 遍历所有节表项
	for (WORD i = 0; i < pNtHeaders->FileHeader.NumberOfSections; ++i) {
		DWORD_PTR sectionVA = (DWORD_PTR)hModule + pSectionHeader[i].VirtualAddress;
		DWORD sectionSize = pSectionHeader[i].Misc.VirtualSize;

		if (sectionSize == 0) {
			continue;
		}

		// 6. 修改内存保护为可读、可写、可执行
		DWORD oldProtect;
		DWORD newProtect = PAGE_EXECUTE_READWRITE; // <--- 核心修改点

		// 打印调试信息 (可选)
		// char sectionName[IMAGE_SIZEOF_SHORT_NAME + 1] = {0};
		// memcpy(sectionName, pSectionHeader[i].Name, IMAGE_SIZEOF_SHORT_NAME);
		// std::cout << "Attempting to change protection for section: " << sectionName
		//           << " at VA: 0x" << std::hex << sectionVA
		//           << " Size: 0x" << std::hex << sectionSize
		//           << " to PAGE_EXECUTE_READWRITE (0x" << std::hex << newProtect << ")" << std::endl;

		if (VirtualProtect((LPVOID)sectionVA, (SIZE_T)sectionSize, newProtect, &oldProtect)) {
			// 成功修改保护
			// std::cout << "Successfully changed protection for " << sectionName
			//           << ". Old protect: 0x" << std::hex << oldProtect << std::endl;
		}
		else {
			// 错误处理: VirtualProtect 失败
			// DWORD lastError = GetLastError();
			// std::cerr << "Failed to change protection for " << sectionName
			//           << ". Error code: " << lastError << std::endl;
			// 根据需求，可以选择在这里返回 false，或者继续尝试修改其他节
		}
	}

	return true;
}



BOOL ConnectCsrChild()
{

	printf("FORKLIB: De-initialize ntdll csr data\n");
	HMODULE ntdll = GetModuleHandleA("ntdll.dll");
	void* pCsrData = (void*)((uintptr_t)ntdll + csrDataRva_x64); //csr相对偏移
	printf("FORKLIB: Csr data = %p\n", pCsrData);
	memset(pCsrData, 0, csrDataSize_x64);  //csr相对偏移大小


	DWORD session_id;
	wchar_t ObjectDirectory[100];
	ProcessIdToSessionId(GetProcessId(GetCurrentProcess()), &session_id);
	swprintf(ObjectDirectory, 100, L"\\Sessions\\%d\\Windows", session_id);
	printf("FORKLIB: Session_id: %d\n", session_id);
	OutputDebugStringA("Debugxxxxxxxx:FORKLIB: Session_id: %d");

	// Not required?
	printf("FORKLIB: Link Console subsystem...\n");
	OutputDebugStringA("Debugxxxxxxxx:FORKLIB: Link Console subsystem...");
	void* pCtrlRoutine = (void*)GetProcAddress(GetModuleHandleA("kernelbase"), "CtrlRoutine"); //获得指向控制台代码的函数指针
	BOOLEAN trash;

	if (!NT_SUCCESS(CsrClientConnectToServer(ObjectDirectory, 1, &pCtrlRoutine, 8, &trash)))
	{
		printf("FORKLIB: CsrClientConnectToServer failed!\n");
		OutputDebugStringA("Debugxxxxxxxx:FORKLIB: CsrClientConnectToServer failed!");
		return FALSE;
	}

	printf("FORKLIB: Link Windows subsystem...\n");
	OutputDebugStringA("Debugxxxxxxxx:FORKLIB: Link Windows subsystem...");
	// passing &gfServerProcess is not necessary, actually? passing &trash is okay?
	char buf[0x248]; // this seem to just be all zero everytime?
	memset(buf, 0, sizeof(buf));
	//if (!NT_SUCCESS(CsrClientConnectToServer(L"\\Sessions\\" CSRSS_SESSIONID L"\\Windows", 3, buf, 0x248, &trash)))
	if (!NT_SUCCESS(CsrClientConnectToServer(ObjectDirectory, 3, buf, 0x248, &trash)))  //现在的版本好想buff得为0x248了
	{
		printf("FORKLIB: CsrClientConnectToServer failed!\n");
		OutputDebugStringA("Debugxxxxxxxx:FORKLIB: CsrClientConnectToServer failed!");
		return FALSE;
	}

	printf("FORKLIB: Connect to Csr...\n");
	if (!NT_SUCCESS(RtlRegisterThreadWithCsrss()))
	{
		printf("FORKLIB: RtlRegisterThreadWithCsrss failed!\n");
		OutputDebugStringA("Debugxxxxxxxx:FORKLIB: RtlRegisterThreadWithCsrss failed!");
		return FALSE;
	}
	
	printf("FORKLIB: Connected to Csr!\n");
	OutputDebugStringA("Debugxxxxxxxx:FORKLIB: Connected to Csr!");
	return TRUE;
}


// 查找并返回包含全局变量的内存区域
std::vector<MemoryRegion> find_global_data_regions() {
	std::vector<MemoryRegion> foundRegions;
	HMODULE hModule = GetModuleHandle(NULL);

	if (!hModule) {
		return foundRegions;
	}

	PBYTE pModuleBase = reinterpret_cast<PBYTE>(hModule);
	PIMAGE_DOS_HEADER pDosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(pModuleBase);
	if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
		return foundRegions;
	}

	PIMAGE_NT_HEADERS pNtHeaders = reinterpret_cast<PIMAGE_NT_HEADERS>(pModuleBase + pDosHeader->e_lfanew);
	if (pNtHeaders->Signature != IMAGE_NT_SIGNATURE) {
		return foundRegions;
	}

	PIMAGE_SECTION_HEADER pSectionHeader = IMAGE_FIRST_SECTION(pNtHeaders);

	for (int i = 0; i < pNtHeaders->FileHeader.NumberOfSections; ++i, ++pSectionHeader) {
		// 使用更安全的方式处理节区名
		std::string sectionName(reinterpret_cast<const char*>(pSectionHeader->Name),
			strnlen_s(reinterpret_cast<const char*>(pSectionHeader->Name), IMAGE_SIZEOF_SHORT_NAME));

		// 定位 .data, .bss, 和 .rdata 节区
		if (sectionName == ".data" || sectionName == ".bss" || sectionName == ".rdata") {
			MemoryRegion region;
			region.name = sectionName;
			region.startAddress = pModuleBase + pSectionHeader->VirtualAddress;
			region.size = pSectionHeader->Misc.VirtualSize;
			foundRegions.push_back(region);
		}
	}
	return foundRegions;
}


BOOL CALLBACK EnumThreadWndProc(HWND hwnd, LPARAM lParam) {
	// lParam 是我们传递进来的自定义数据，这里我们把它解释为
	// 一个指向 HWND 集合的指针 (用set可以自动去重)。
	auto pWindowHandles = reinterpret_cast<std::set<HWND>*>(lParam);
	if (pWindowHandles) {
		pWindowHandles->insert(hwnd);
	}
	return TRUE; // 返回TRUE表示继续枚举
}

// EnumChildWindows的回调函数，用于将子窗口句柄添加到集合中
BOOL CALLBACK EnumChildProc(HWND hwnd, LPARAM lParam) {
	auto pHandles = reinterpret_cast<std::set<HWND>*>(lParam);
	pHandles->insert(hwnd);
	return TRUE; // 继续枚举
}

// EnumWindows的回调函数
BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
	EnumData* pData = reinterpret_cast<EnumData*>(lParam);
	DWORD windowProcessId = 0;

	// 获取窗口所属的进程ID
	GetWindowThreadProcessId(hwnd, &windowProcessId);

	// 如果该窗口属于我们的目标进程
	if (windowProcessId == pData->processId) {
		// 1. 将这个顶层窗口的句柄加入集合
		pData->pHandles->insert(hwnd);
		// 2. 枚举这个顶层窗口下的所有子窗口
		EnumChildWindows(hwnd, EnumChildProc, reinterpret_cast<LPARAM>(pData->pHandles));
	}

	return TRUE; // 继续枚举其他顶层窗口
}


/**
 * @brief 所有被hook的线程都会执行这个包装函数。
 * 它负责调用原始线程函数，并在线程结束后自动清理注册信息。
 */
DWORD WINAPI UniversalThreadWrapper(LPVOID lpParam) {
	DWORD threadId = GetCurrentThreadId();
	ManagedThreadInfo* pInfo = nullptr;

	// 从全局管理器中获取自己的信息
	{
		std::lock_guard<std::mutex> lock(g_threadMapMutex);
		auto it = g_managedThreads.find(threadId);
		if (it != g_managedThreads.end()) {
			pInfo = &(it->second);
		}
	}

	if (pInfo == nullptr) {
		// 理论上不应该发生，如果发生了，直接退出
		return (DWORD)-1;
	}

	// 调用原始的线程函数
	DWORD exitCode = pInfo->pOriginalThreadProc(pInfo->pOriginalParameter);

	// 线程正常结束后，从全局管理器中注销自己
	{
		std::lock_guard<std::mutex> lock(g_threadMapMutex);
		CloseHandle(pInfo->hStopEvent); // 关闭事件句柄
		// 线程句柄由创建者管理，这里不关闭
		g_managedThreads.erase(threadId);
	}

	return exitCode;
}


std::string MessageIDToString(UINT msg) {
	// The map is initialized only once due to the 'static' keyword.
	static const std::map<UINT, std::string> msgMap = {
		// General Window Messages (0x0000 - 0x002E)
		{WM_NULL, "WM_NULL"},
		{WM_CREATE, "WM_CREATE"},
		{WM_DESTROY, "WM_DESTROY"},
		{WM_MOVE, "WM_MOVE"},
		{WM_SIZE, "WM_SIZE"},
		{WM_ACTIVATE, "WM_ACTIVATE"},
		{WM_SETFOCUS, "WM_SETFOCUS"},
		{WM_KILLFOCUS, "WM_KILLFOCUS"},
		{WM_ENABLE, "WM_ENABLE"},
		{WM_SETREDRAW, "WM_SETREDRAW"},
		{WM_SETTEXT, "WM_SETTEXT"},
		{WM_GETTEXT, "WM_GETTEXT"},
		{WM_GETTEXTLENGTH, "WM_GETTEXTLENGTH"},
		{WM_PAINT, "WM_PAINT"},
		{WM_CLOSE, "WM_CLOSE"},
		{WM_QUERYENDSESSION, "WM_QUERYENDSESSION"},
		{WM_QUERYOPEN, "WM_QUERYOPEN"},
		{WM_ENDSESSION, "WM_ENDSESSION"},
		{WM_QUIT, "WM_QUIT"},
		{WM_ERASEBKGND, "WM_ERASEBKGND"},
		{WM_SYSCOLORCHANGE, "WM_SYSCOLORCHANGE"},
		{WM_SHOWWINDOW, "WM_SHOWWINDOW"},
		{WM_WININICHANGE, "WM_WININICHANGE"}, // Also WM_SETTINGCHANGE
		{WM_DEVMODECHANGE, "WM_DEVMODECHANGE"},
		{WM_ACTIVATEAPP, "WM_ACTIVATEAPP"},
		{WM_FONTCHANGE, "WM_FONTCHANGE"},
		{WM_TIMECHANGE, "WM_TIMECHANGE"},
		{WM_CANCELMODE, "WM_CANCELMODE"},
		{WM_SETCURSOR, "WM_SETCURSOR"},
		{WM_MOUSEACTIVATE, "WM_MOUSEACTIVATE"},
		{WM_CHILDACTIVATE, "WM_CHILDACTIVATE"},
		{WM_QUEUESYNC, "WM_QUEUESYNC"},
		{WM_GETMINMAXINFO, "WM_GETMINMAXINFO"},
		{WM_PAINTICON, "WM_PAINTICON"},
		{WM_ICONERASEBKGND, "WM_ICONERASEBKGND"},
		{WM_NEXTDLGCTL, "WM_NEXTDLGCTL"},
		{WM_SPOOLERSTATUS, "WM_SPOOLERSTATUS"},
		{WM_DRAWITEM, "WM_DRAWITEM"},
		{WM_MEASUREITEM, "WM_MEASUREITEM"},
		{WM_DELETEITEM, "WM_DELETEITEM"},
		{WM_VKEYTOITEM, "WM_VKEYTOITEM"},
		{WM_CHARTOITEM, "WM_CHARTOITEM"},

		// Window Positioning and Creation (0x0046 - 0x00A0)
		{WM_WINDOWPOSCHANGING, "WM_WINDOWPOSCHANGING"},
		{WM_WINDOWPOSCHANGED, "WM_WINDOWPOSCHANGED"},
		{WM_NCCREATE, "WM_NCCREATE"},
		{WM_NCDESTROY, "WM_NCDESTROY"},
		{WM_NCCALCSIZE, "WM_NCCALCSIZE"},
		{WM_NCHITTEST, "WM_NCHITTEST"},
		{WM_NCPAINT, "WM_NCPAINT"},
		{WM_NCACTIVATE, "WM_NCACTIVATE"},
		{WM_GETDLGCODE, "WM_GETDLGCODE"},
		{WM_SYNCPAINT, "WM_SYNCPAINT"},

		// Non-Client Area Mouse Messages (0x00A0 - 0x00AE)
		{WM_NCMOUSEMOVE, "WM_NCMOUSEMOVE"},
		{WM_NCLBUTTONDOWN, "WM_NCLBUTTONDOWN"},
		{WM_NCLBUTTONUP, "WM_NCLBUTTONUP"},
		{WM_NCLBUTTONDBLCLK, "WM_NCLBUTTONDBLCLK"},
		{WM_NCRBUTTONDOWN, "WM_NCRBUTTONDOWN"},
		{WM_NCRBUTTONUP, "WM_NCRBUTTONUP"},
		{WM_NCRBUTTONDBLCLK, "WM_NCRBUTTONDBLCLK"},
		{WM_NCMBUTTONDOWN, "WM_NCMBUTTONDOWN"},
		{WM_NCMBUTTONUP, "WM_NCMBUTTONUP"},
		{WM_NCMBUTTONDBLCLK, "WM_NCMBUTTONDBLCLK"},
		{WM_NCXBUTTONDOWN, "WM_NCXBUTTONDOWN"},
		{WM_NCXBUTTONUP, "WM_NCXBUTTONUP"},
		{WM_NCXBUTTONDBLCLK, "WM_NCXBUTTONDBLCLK"},
		{WM_NCMOUSEHOVER, "WM_NCMOUSEHOVER"},
		{WM_NCMOUSELEAVE, "WM_NCMOUSELEAVE"},

		// Keyboard Messages (0x0100 - 0x0109)
		{WM_KEYDOWN, "WM_KEYDOWN"},
		{WM_KEYUP, "WM_KEYUP"},
		{WM_CHAR, "WM_CHAR"},
		{WM_DEADCHAR, "WM_DEADCHAR"},
		{WM_SYSKEYDOWN, "WM_SYSKEYDOWN"},
		{WM_SYSKEYUP, "WM_SYSKEYUP"},
		{WM_SYSCHAR, "WM_SYSCHAR"},
		{WM_SYSDEADCHAR, "WM_SYSDEADCHAR"},
		{WM_UNICHAR, "WM_UNICHAR"},
		{WM_KEYLAST, "WM_KEYLAST"},
		{WM_IME_STARTCOMPOSITION, "WM_IME_STARTCOMPOSITION"},
		{WM_IME_ENDCOMPOSITION, "WM_IME_ENDCOMPOSITION"},
		{WM_IME_COMPOSITION, "WM_IME_COMPOSITION"},

		// Command and System Messages (0x0111 - 0x0112)
		{WM_INITDIALOG, "WM_INITDIALOG"},
		{WM_COMMAND, "WM_COMMAND"},
		{WM_SYSCOMMAND, "WM_SYSCOMMAND"},

		// Timer and Scroll Messages (0x0113 - 0x0115)
		{WM_TIMER, "WM_TIMER"},
		{WM_HSCROLL, "WM_HSCROLL"},
		{WM_VSCROLL, "WM_VSCROLL"},

		// Menu Messages (0x0116 - 0x0126)
		{WM_INITMENU, "WM_INITMENU"},
		{WM_INITMENUPOPUP, "WM_INITMENUPOPUP"},
		{WM_GESTURE, "WM_GESTURE"},
		{WM_GESTURENOTIFY, "WM_GESTURENOTIFY"},
		{WM_MENUSELECT, "WM_MENUSELECT"},
		{WM_MENUCHAR, "WM_MENUCHAR"},
		{WM_ENTERIDLE, "WM_ENTERIDLE"},
		{WM_MENURBUTTONUP, "WM_MENURBUTTONUP"},
		{WM_MENUDRAG, "WM_MENUDRAG"},
		{WM_MENUGETOBJECT, "WM_MENUGETOBJECT"},
		{WM_UNINITMENUPOPUP, "WM_UNINITMENUPOPUP"},
		{WM_MENUCOMMAND, "WM_MENUCOMMAND"},
		{WM_CHANGEUISTATE, "WM_CHANGEUISTATE"},
		{WM_UPDATEUISTATE, "WM_UPDATEUISTATE"},
		{WM_QUERYUISTATE, "WM_QUERYUISTATE"},

		// Control Messages (0x0132 - 0x0138)
		{WM_CTLCOLORMSGBOX, "WM_CTLCOLORMSGBOX"},
		{WM_CTLCOLOREDIT, "WM_CTLCOLOREDIT"},
		{WM_CTLCOLORLISTBOX, "WM_CTLCOLORLISTBOX"},
		{WM_CTLCOLORBTN, "WM_CTLCOLORBTN"},
		{WM_CTLCOLORDLG, "WM_CTLCOLORDLG"},
		{WM_CTLCOLORSCROLLBAR, "WM_CTLCOLORSCROLLBAR"},
		{WM_CTLCOLORSTATIC, "WM_CTLCOLORSTATIC"},

		// Client Area Mouse Messages (0x0200 - 0x0215)
		{WM_MOUSEMOVE, "WM_MOUSEMOVE"},
		{WM_LBUTTONDOWN, "WM_LBUTTONDOWN"},
		{WM_LBUTTONUP, "WM_LBUTTONUP"},
		{WM_LBUTTONDBLCLK, "WM_LBUTTONDBLCLK"},
		{WM_RBUTTONDOWN, "WM_RBUTTONDOWN"},
		{WM_RBUTTONUP, "WM_RBUTTONUP"},
		{WM_RBUTTONDBLCLK, "WM_RBUTTONDBLCLK"},
		{WM_MBUTTONDOWN, "WM_MBUTTONDOWN"},
		{WM_MBUTTONUP, "WM_MBUTTONUP"},
		{WM_MBUTTONDBLCLK, "WM_MBUTTONDBLCLK"},
		{WM_MOUSEWHEEL, "WM_MOUSEWHEEL"},
		{WM_XBUTTONDOWN, "WM_XBUTTONDOWN"},
		{WM_XBUTTONUP, "WM_XBUTTONUP"},
		{WM_XBUTTONDBLCLK, "WM_XBUTTONDBLCLK"},
		{WM_MOUSEHWHEEL, "WM_MOUSEHWHEEL"},
		{WM_MOUSEHOVER, "WM_MOUSEHOVER"},
		{WM_MOUSELEAVE, "WM_MOUSELEAVE"},

		// MDI Messages (0x0220 - 0x0233)
		{WM_PARENTNOTIFY, "WM_PARENTNOTIFY"},
		{WM_ENTERMENULOOP, "WM_ENTERMENULOOP"},
		{WM_EXITMENULOOP, "WM_EXITMENULOOP"},
		{WM_NEXTMENU, "WM_NEXTMENU"},
		{WM_SIZING, "WM_SIZING"},
		{WM_CAPTURECHANGED, "WM_CAPTURECHANGED"},
		{WM_MOVING, "WM_MOVING"},
		{WM_POWERBROADCAST, "WM_POWERBROADCAST"},
		{WM_DEVICECHANGE, "WM_DEVICECHANGE"},
		{WM_MDICREATE, "WM_MDICREATE"},
		{WM_MDIDESTROY, "WM_MDIDESTROY"},
		{WM_MDIACTIVATE, "WM_MDIACTIVATE"},
		{WM_MDIRESTORE, "WM_MDIRESTORE"},
		{WM_MDINEXT, "WM_MDINEXT"},
		{WM_MDIMAXIMIZE, "WM_MDIMAXIMIZE"},
		{WM_MDITILE, "WM_MDITILE"},
		{WM_MDICASCADE, "WM_MDICASCADE"},
		{WM_MDIICONARRANGE, "WM_MDIICONARRANGE"},
		{WM_MDIGETACTIVE, "WM_MDIGETACTIVE"},
		{WM_MDISETMENU, "WM_MDISETMENU"},
		{WM_ENTERSIZEMOVE, "WM_ENTERSIZEMOVE"},
		{WM_EXITSIZEMOVE, "WM_EXITSIZEMOVE"},
		{WM_DROPFILES, "WM_DROPFILES"},
		{WM_MDIREFRESHMENU, "WM_MDIREFRESHMENU"},

		// Newer Touch, Pointer, and DPI Messages
		{WM_POINTERDEVICECHANGE, "WM_POINTERDEVICECHANGE"},
		{WM_POINTERDEVICEINRANGE, "WM_POINTERDEVICEINRANGE"},
		{WM_POINTERDEVICEOUTOFRANGE, "WM_POINTERDEVICEOUTOFRANGE"},
		{WM_TOUCH, "WM_TOUCH"},
		{WM_NCPOINTERUPDATE, "WM_NCPOINTERUPDATE"},
		{WM_NCPOINTERDOWN, "WM_NCPOINTERDOWN"},
		{WM_NCPOINTERUP, "WM_NCPOINTERUP"},
		{WM_POINTERUPDATE, "WM_POINTERUPDATE"},
		{WM_POINTERDOWN, "WM_POINTERDOWN"},
		{WM_POINTERUP, "WM_POINTERUP"},
		{WM_POINTERENTER, "WM_POINTERENTER"},
		{WM_POINTERLEAVE, "WM_POINTERLEAVE"},
		{WM_POINTERACTIVATE, "WM_POINTERACTIVATE"},
		{WM_POINTERCAPTURECHANGED, "WM_POINTERCAPTURECHANGED"},
		{WM_TOUCHHITTESTING, "WM_TOUCHHITTESTING"},
		{WM_POINTERWHEEL, "WM_POINTERWHEEL"},
		{WM_POINTERHWHEEL, "WM_POINTERHWHEEL"},
		{WM_DPICHANGED, "WM_DPICHANGED"},

		// Clipboard Messages (0x0300 - 0x0312)
		{WM_CUT, "WM_CUT"},
		{WM_COPY, "WM_COPY"},
		{WM_PASTE, "WM_PASTE"},
		{WM_CLEAR, "WM_CLEAR"},
		{WM_UNDO, "WM_UNDO"},
		{WM_RENDERFORMAT, "WM_RENDERFORMAT"},
		{WM_RENDERALLFORMATS, "WM_RENDERALLFORMATS"},
		{WM_DESTROYCLIPBOARD, "WM_DESTROYCLIPBOARD"},
		{WM_DRAWCLIPBOARD, "WM_DRAWCLIPBOARD"},
		{WM_PAINTCLIPBOARD, "WM_PAINTCLIPBOARD"},
		{WM_VSCROLLCLIPBOARD, "WM_VSCROLLCLIPBOARD"},
		{WM_SIZECLIPBOARD, "WM_SIZECLIPBOARD"},
		{WM_ASKCBFORMATNAME, "WM_ASKCBFORMATNAME"},
		{WM_CHANGECBCHAIN, "WM_CHANGECBCHAIN"},
		{WM_HSCROLLCLIPBOARD, "WM_HSCROLLCLIPBOARD"},
		{WM_QUERYNEWPALETTE, "WM_QUERYNEWPALETTE"},
		{WM_PALETTEISCHANGING, "WM_PALETTEISCHANGING"},
		{WM_PALETTECHANGED, "WM_PALETTECHANGED"},

		// Hotkey and AppCommand Messages
		{WM_HOTKEY, "WM_HOTKEY"},
		{WM_APPCOMMAND, "WM_APPCOMMAND"},

		// Notification message
		{WM_NOTIFY, "WM_NOTIFY"},

		// Theme change
		{WM_THEMECHANGED, "WM_THEMECHANGED"}
	};

	// 1. Try to find the message in our map
	auto it = msgMap.find(msg);
	if (it != msgMap.end()) {
		return it->second;
	}

	// 2. Handle WM_USER for custom user-defined messages
	if (msg >= WM_USER && msg < WM_APP) {
		std::ostringstream oss;
		oss << "WM_USER+" << (msg - WM_USER);
		return oss.str();
	}

	// 3. Handle WM_APP for application-defined messages
	if (msg >= WM_APP && msg <= 0xBFFF) {
		std::ostringstream oss;
		oss << "WM_APP+" << (msg - WM_APP);
		return oss.str();
	}

	// 4. Handle Registered Window Messages or other unknown messages
	// Registered messages are in the range 0xC000 to 0xFFFF
	std::ostringstream oss;
	if (msg >= 0xC000 && msg <= 0xFFFF) {
		oss << "Registered Message (0x" << std::hex << std::uppercase << msg << ")";
	}
	else {
		oss << "Unknown Message (0x" << std::hex << std::uppercase << msg << ")";
	}
	return oss.str();
}

//消息过滤白名单
bool IsUserActionMessage(UINT msg)
{
	switch (msg)
	{
		// --- 键盘输入 ---
		// 用户按下、释放或输入字符
	case WM_KEYDOWN:
	case WM_KEYUP:
	case WM_CHAR:
	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
	case WM_SYSCHAR:
		return true;

		// --- 鼠标输入 (客户端区域) ---
		// 用户移动、点击、双击、滚动滚轮

	//case WM_MOUSEMOVE:
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_LBUTTONDBLCLK:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_RBUTTONDBLCLK:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_MBUTTONDBLCLK:
	case WM_MOUSEWHEEL:
	case WM_XBUTTONDOWN:
	case WM_XBUTTONUP:
	case WM_XBUTTONDBLCLK:
		return true;

		// --- 鼠标输入 (非客户端区域) ---
		// 用户点击窗口的标题栏、边框、菜单等
	case WM_NCLBUTTONDOWN:
	case WM_NCLBUTTONUP:
	case WM_NCLBUTTONDBLCLK:
	case WM_NCRBUTTONDOWN:
	case WM_NCRBUTTONUP:
	case WM_NCRBUTTONDBLCLK:
	case WM_NCMBUTTONDOWN:
	case WM_NCMBUTTONUP:
	case WM_NCMBUTTONDBLCLK:
	case WM_NCXBUTTONDOWN:
	case WM_NCXBUTTONUP:
	case WM_NCXBUTTONDBLCLK:
		return true;

		// --- 控件与菜单交互 ---
		// 用户点击按钮、菜单项，或与控件交互时产生的核心通知
	case WM_COMMAND:
	case WM_SYSCOMMAND: // 例如，点击关闭、最大化按钮，或选择系统菜单
		return true;

		// --- 通用控件通知 ---
		// 由较新的通用控件（如ListView, TreeView等）发送的通知  notify是广播 其实意义不大
	//case WM_NOTIFY:   
	//	return true;

		// --- 窗口操作 ---
		// 用户通过拖拽或点击按钮关闭窗口
	case WM_CLOSE:  // 用户点击关闭按钮 (X)
	case WM_MOVE:   // 用户拖动窗口
	case WM_SIZE:   // 用户调整窗口大小
		return true;

		// --- 默认情况 ---
		// 其他所有消息 (如WM_PAINT, WM_TIMER, WM_SETCURSOR等) 都被视为非用户操作
	default:
		return false;
	}
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

HWND FindDescendantWindow(HWND hParent, LPCWSTR title) {
	FindDescendantData data = { title, NULL };
	EnumChildWindows(hParent, EnumDescendantProc, (LPARAM)&data);
	return data.foundHwnd;
}
void init() {

	//初始化寻找子父窗口的全角变量
	while (hwndProcessTop == NULL || hwndReplyWindow == NULL) {
		hwndProcessTop = FindWindow(NULL, L"Autodesk DWG TrueView 2026 - [Start]");
		HWND t = FindDescendantWindow(hwndProcessTop, L"Start");
		hwndReplyWindow = GetWindow(t, GW_HWNDLAST);

	}
}


