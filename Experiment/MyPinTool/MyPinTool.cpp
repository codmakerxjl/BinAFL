#include "pin.H"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <list>
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <ctime>
#include <sstream>
#include <map>

// ʹ�� WD �����ռ�������� Pin �ĺ��ͻ
namespace WD {
#include <Windows.h>
#include <wtypes.h>
#include <CommCtrl.h>
#include <Shlwapi.h>
};

using std::string;
using std::endl;
using std::vector;
using std::list;

// --- ȫ�ֱ��� ---
std::ofstream g_log_file;
PIN_LOCK g_lock;

// ���ڴ洢ÿ�����ڵ� WndProc ���໯��
std::map<WD::HWND, list<ADDRINT>> g_subclass_chains;
PIN_LOCK g_chain_lock;

// MFC ������RVA������Ҫ����Ŀ��mfc*.dll������IDA��ȷ��
const ADDRINT AFXCALLWNDPROC_RVA = 0x2966F0;

// Ҫ׷�ٵ�Ŀ����Ϣ������ID
const UINT TARGET_MSG = WM_COMMAND;
const UINT TARGET_CMD_ID = 0xE110;

// ���������߳�׷��״̬��һ��TLS Key��
static TLS_KEY g_tls_key;

// ��չTraceState�ṹ�壬����������Ҫ����Ϣ
struct TraceState {
    BOOL isActive;             // �Ƿ񼤻�׷��
    WD::WPARAM saved_wParam;   // �����wParam������������������
    WD::LPARAM saved_lParam;   // �����lParam
    ADDRINT    target_c_wnd_ptr; //����Ŀ��CWnd*��ָ��
};

// --- �����п��� ---
KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
    "o", "wndproc_chain_log.txt", "Specify the output log file");
KNOB<BOOL> KnobTraceIndirectOnly(KNOB_MODE_WRITEONCE, "pintool", "indirect_only", "0", "Enable to trace only indirect calls/jmps.");

// --- �������� ---
string GetTimestamp() {
    WD::SYSTEMTIME st;
    WD::GetLocalTime(&st);
    std::ostringstream oss;
    oss << st.wYear << "-" << std::setw(2) << std::setfill('0') << st.wMonth << "-" << std::setw(2) << std::setfill('0') << st.wDay << " "
        << std::setw(2) << std::setfill('0') << st.wHour << ":" << std::setw(2) << std::setfill('0') << st.wMinute << ":" << std::setw(2) << std::setfill('0') << st.wSecond;
    return oss.str();
}

#define LOG(msg) g_log_file << "[" << GetTimestamp() << "] " << msg << endl

string ResolveAddress(ADDRINT addr) {
    if (addr == 0) return "NULL";
    std::stringstream ss;
    PIN_LockClient();
    IMG img = IMG_FindByAddress(addr);
    if (IMG_Valid(img)) {
        string module_name = IMG_Name(img);
        size_t last_slash = module_name.find_last_of("/\\");
        if (std::string::npos != last_slash) {
            module_name = module_name.substr(last_slash + 1);
        }
        ADDRINT offset = addr - IMG_LowAddress(img);
        ss << std::setw(25) << std::left << module_name << " + 0x" << std::hex << offset;
    }
    else {
        ss << "Unknown Location (0x" << std::hex << addr << ")";
    }
    PIN_UnlockClient();
    return ss.str();
}

// --- API ���� (Proxy)  ---
typedef WD::ULONG_PTR(WINAPI* GetWindowLongPtrW_t)(WD::HWND, int);
GetWindowLongPtrW_t origGetWindowLongPtrW = NULL;
WD::ULONG_PTR WINAPI ProxyGetWindowLongPtrW(WD::HWND hWnd, int nIndex) {
    if (origGetWindowLongPtrW) return origGetWindowLongPtrW(hWnd, nIndex);
    return 0;
}

// --- ���Ĳ�׮�ص����� ---

// SetWindowLongPtrW �ص� 
VOID SetWindowLongPtrW_Before(WD::HWND hwnd, int nIndex, WD::LONG_PTR dwNewLong)
{
    if (nIndex == GWLP_WNDPROC)
    {
        ADDRINT newWndProc = (ADDRINT)dwNewLong;
        ADDRINT oldWndProc = (ADDRINT)ProxyGetWindowLongPtrW(hwnd, GWLP_WNDPROC);
        if (newWndProc == oldWndProc) return;

        PIN_GetLock(&g_chain_lock, 1);
        auto it = g_subclass_chains.find(hwnd);
        if (it == g_subclass_chains.end()) {
            list<ADDRINT> chain;
            chain.push_back(newWndProc);
            chain.push_back(oldWndProc);
            g_subclass_chains[hwnd] = chain;
        }
        else {
            it->second.push_front(newWndProc);
        }
        PIN_ReleaseLock(&g_chain_lock);
    }
}

// DispatchMessageW �ص� 
VOID DispatchMessage_Before(THREADID threadId, const WD::MSG* pMsg)
{
    if (pMsg && pMsg->message == TARGET_MSG && pMsg->wParam == TARGET_CMD_ID) {
        LOG("\n================================================================================\n"
            << "[MESSAGE] [TID:" << threadId << "] DispatchMessage for WM_COMMAND on HWND 0x" << std::hex << pMsg->hwnd << std::dec << "\n"
            << "    wParam: 0x" << std::hex << pMsg->wParam << ", lParam: 0x" << pMsg->lParam << std::dec);
        PIN_GetLock(&g_chain_lock, 1);
        auto it = g_subclass_chains.find(pMsg->hwnd);
        if (it != g_subclass_chains.end()) {
            LOG("    >> Found Subclassing Chain for this HWND:");
            int i = 0;
            for (ADDRINT procAddr : it->second) {
                if (i == 0) {
                    LOG("       [" << i << "] " << ResolveAddress(procAddr) << " <== CURRENT ENTRY POINT");
                }
                else {
                    LOG("       [" << i << "] " << ResolveAddress(procAddr));
                }
                i++;
            }
        }
        else {
            ADDRINT currentWndProc = (ADDRINT)ProxyGetWindowLongPtrW(pMsg->hwnd, GWLP_WNDPROC);
            LOG("    >> No recorded subclassing history. Current WndProc is:");
            LOG("       [0] " << ResolveAddress(currentWndProc) << " <== CURRENT ENTRY POINT");
        }
        PIN_ReleaseLock(&g_chain_lock);
        LOG("================================================================================\n");
    }
}

// �� AfxCallWndProc ��ڴ��Ļص�
VOID AfxCallWndProc_Entry(THREADID tid, ADDRINT c_wnd_ptr, UINT msg, WD::WPARAM wParam, WD::LPARAM lParam) {
    if (msg == TARGET_MSG && wParam == TARGET_CMD_ID) {
        // �������� CWnd* ָ��� TraceState
        TraceState* state = new TraceState{ TRUE, wParam, lParam, c_wnd_ptr };
        // ʹ��ͳһ�� TLS Key
        PIN_SetThreadData(g_tls_key, state, tid);

        LOG("\n[CONTEXT SET] Starting trace for CWnd* 0x" << std::hex << c_wnd_ptr);
        LOG("                Tracking wParam=0x" << std::hex << wParam << ", lParam=0x" << lParam);
    }
}

// �� AfxCallWndProc ���ڴ��Ļص�
VOID AfxCallWndProc_Exit(THREADID tid) {
    // ʹ��ͳһ�� TLS Key
    TraceState* state = static_cast<TraceState*>(PIN_GetThreadData(g_tls_key, tid));
    if (state != NULL) {
        if (state->isActive) {
            LOG("[CONTEXT CLEARED] Trace finished.\n");
        }
        delete state;
        PIN_SetThreadData(g_tls_key, NULL, tid);
    }
}


VOID OnDirectCall(THREADID tid, ADDRINT target_addr)
{
    // ���׷���Ƿ񼤻�
    TraceState* state = static_cast<TraceState*>(PIN_GetThreadData(g_tls_key, tid));
    if (state != NULL && state->isActive) {
        LOG("    >> [Direct Call]   -> " << ResolveAddress(target_addr));
    }
}


VOID OnIndirectCall(THREADID tid, ADDRINT target_addr, ADDRINT rcx_val)
{
    TraceState* state = static_cast<TraceState*>(PIN_GetThreadData(g_tls_key, tid));
    if (state != NULL && state->isActive) {
        LOG("    >> [INDIRECT Call] -> " << ResolveAddress(target_addr));

        // ������ʾ��Ŀ��CWnd*�ϵĵ���
        if (state->target_c_wnd_ptr == rcx_val) {
            LOG("        ^---- BINGO! Call on target CWnd* object: 0x" << std::hex << rcx_val);
        }
    }
}

// ȫ·��׷��ģʽ�ĺ��ģ�����������������
VOID OnRoutineEntry(THREADID tid, ADDRINT routine_addr, ADDRINT arg0, ADDRINT arg1, ADDRINT arg2, ADDRINT arg3) {
    TraceState* state = static_cast<TraceState*>(PIN_GetThreadData(g_tls_key, tid));
    if (state == NULL || !state->isActive) {
        return;
    }

    // ������������ֻ��ʾ���ǹ��ĵ�ģ���еĺ�������
    bool should_log = false;
    PIN_LockClient();
    IMG img = IMG_FindByAddress(routine_addr);
    if (IMG_Valid(img)) {
        string module_name_lower = IMG_Name(img);
        std::transform(module_name_lower.begin(), module_name_lower.end(), module_name_lower.begin(), ::tolower);

        // ���ģ�����в�������Щϵͳ/�ײ�⣬���Ǿ���Ϊ�������ǹ��ĵ�
        if (module_name_lower.find("ntdll.dll") == string::npos &&
            module_name_lower.find("kernel32.dll") == string::npos &&
            module_name_lower.find("kernelbase.dll") == string::npos &&
            module_name_lower.find("ucrtbase.dll") == string::npos &&
            module_name_lower.find("vcruntime") == string::npos && // ƥ�� VCRUNTIME140.dll ��
            module_name_lower.find("win32u.dll") == string::npos)
        {
            should_log = true;
        }
    }
    else {
        should_log = true; // ����Ҳ���ģ����Ϣ������JIT���룩��Ҳ��¼����
    }
    PIN_UnlockClient();

    if (!should_log) {
        return; // �����ϰ�������ֱ�ӷ���
    }

    // --- ��־��¼�߼� ---
    string log_line = "    [TRACE PATH] -> " + ResolveAddress(routine_addr);
    bool dependency_found = false;
    if (arg0 == state->saved_wParam || arg1 == state->saved_wParam || arg2 == state->saved_wParam || arg3 == state->saved_wParam) {
        dependency_found = true;
    }

    if (dependency_found) {
        log_line += "  <== [DATA DEPENDENCY HINT!]";
    }
    LOG(log_line);
}

// --- ��׮���� ---
VOID InstrumentImage(IMG img, VOID* v)
{
    string img_name_lower = IMG_Name(img);
    std::transform(img_name_lower.begin(), img_name_lower.end(), img_name_lower.begin(), ::tolower);

    // MFC ���ĺ��� Hook
    if (img_name_lower.find("mfc140u.dll") != std::string::npos) {
        ADDRINT base_addr = IMG_LowAddress(img);
        ADDRINT absolute_addr = base_addr + AFXCALLWNDPROC_RVA;
        RTN rtn = RTN_CreateAt(absolute_addr, "AfxCallWndProc_ByRVA");
        if (RTN_Valid(rtn)) {
            LOG("[INFO] Successfully found and instrumented AfxCallWndProc in " << IMG_Name(img));
            RTN_Open(rtn);
            // Hook ���
            RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)AfxCallWndProc_Entry,
                IARG_THREAD_ID,
                IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                IARG_FUNCARG_ENTRYPOINT_VALUE, 2,
                IARG_FUNCARG_ENTRYPOINT_VALUE, 3,
                IARG_FUNCARG_ENTRYPOINT_VALUE, 4,
                IARG_END);
            // Hook ����
            RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)AfxCallWndProc_Exit,
                IARG_THREAD_ID,
                IARG_END);
            RTN_Close(rtn);
        }
    }

    // USER32 API Hook
    if (img_name_lower.find("user32.dll") != std::string::npos)
    {
        RTN rtn = RTN_FindByName(img, "DispatchMessageW");
        if (RTN_Valid(rtn)) {
            RTN_Open(rtn);
            RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)DispatchMessage_Before, IARG_THREAD_ID, IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_END);
            RTN_Close(rtn);
        }
        rtn = RTN_FindByName(img, "SetWindowLongPtrW");
        if (RTN_Valid(rtn)) {
            RTN_Open(rtn);
            RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)SetWindowLongPtrW_Before, IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_FUNCARG_ENTRYPOINT_VALUE, 1, IARG_FUNCARG_ENTRYPOINT_VALUE, 2, IARG_END);
            RTN_Close(rtn);
        }
        // �滻 GetWindowLongPtrW
        RTN rtnGetLong = RTN_FindByName(img, "GetWindowLongPtrW");
        if (RTN_Valid(rtnGetLong) && origGetWindowLongPtrW == NULL) {
            origGetWindowLongPtrW = (GetWindowLongPtrW_t)RTN_Replace(rtnGetLong, (AFUNPTR)ProxyGetWindowLongPtrW);
        }
    }
}


// 
VOID InstrumentInstruction(INS ins, VOID* v)
{
    // �����������͵� CALL ָ��
    if (INS_IsCall(ins))
    {
        // �����ֱ�ӵ��� (call sub_XXXX)
        if (INS_IsDirectCall(ins)) {
            ADDRINT target_addr = INS_DirectBranchOrCallTargetAddress(ins);
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)OnDirectCall,
                IARG_THREAD_ID,
                IARG_ADDRINT, target_addr,
                IARG_END);
        }
        // ����Ǽ�ӵ��� (call rax, call [mem])
        else {
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)OnIndirectCall,
                IARG_THREAD_ID,
                IARG_BRANCH_TARGET_ADDR,
                IARG_REG_VALUE, REG_RCX,
                IARG_END);
        }
    }
}

// ���̼���׮����
VOID InstrumentRoutine(RTN rtn, VOID* v) {
    RTN_Open(rtn);
    RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)OnRoutineEntry,
        IARG_THREAD_ID,
        IARG_ADDRINT, RTN_Address(rtn),
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 3,
        IARG_END);
    RTN_Close(rtn);
}

// --- Pin ��ʼ������β ---
VOID Fini(INT32 code, VOID* v) {
    LOG("[INFO] Pintool analysis finished.");
    if (g_log_file.is_open()) {
        g_log_file.close();
    }
}

int main(int argc, char* argv[]) {
    PIN_InitSymbols();
    if (PIN_Init(argc, argv)) { return 1; }

    g_log_file.open(KnobOutputFile.Value().c_str());
    if (!g_log_file.is_open()) { return 1; }

    // ֻ����һ��TLS Key
    g_tls_key = PIN_CreateThreadDataKey(0);
    if (g_tls_key == INVALID_TLS_KEY) {
        LOG("[ERROR] Failed to create TLS key. Aborting.");
        g_log_file.close();
        return 1;
    }

    LOG("================================================================================");
    LOG("[Pintool Start] MFC Combined Path Tracer Initialized.");

    // ���������п���ѡ��ע���ĸ�׷�ٺ���
    if (KnobTraceIndirectOnly.Value()) {
        LOG("[CONFIG] Mode: Indirect Call Tracing enabled (-indirect_only 1).");
        INS_AddInstrumentFunction(InstrumentInstruction, 0); // ע��ָ���׮
    }
    else {
        LOG("[CONFIG] Mode: Full Routine Path Tracing enabled (default).");
        RTN_AddInstrumentFunction(InstrumentRoutine, 0); // ע�����̼���׮
    }

    LOG("================================================================================\n");

    IMG_AddInstrumentFunction(InstrumentImage, 0);
    PIN_AddFiniFunction(Fini, 0);

    PIN_StartProgram();
    return 0;
}