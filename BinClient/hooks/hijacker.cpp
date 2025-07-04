#include "pch.h"
#include "SimpleIniParser.h"
// #include "pch.h"
#include "hijacker.h"
#include "detours.h"
#include "FileLogger.h"
#include "MessageFilter.h" // �����µĹ�����ģ��
#include <map>
#include <string>
#include <sstream>
#include <codecvt>
#include <locale>
#include "FileCacheManager.h"
// #include "SimpleIniParser.h"
static LRESULT(WINAPI* TrueDispatchMessageW)(const MSG* lpMsg) = DispatchMessageW;
static BOOL(WINAPI* TrueReadFile)(HANDLE, LPVOID, DWORD, LPDWORD, LPOVERLAPPED) = ReadFile;
//��¼��־�Ƿ�����ԭ�ӱ�־
static std::atomic<bool> g_isLoggingActive = false;
FileCacheManager g_FileCache(1000, 4096); //����1000�����4096
std::string MessageIDToString(UINT msg) {
	// ��ӳ���ֻ���ڵ�һ�ε���ʱ����ʼ��
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

	// 1. ������ӳ����в�����Ϣ
	auto it = msgMap.find(msg);
	if (it != msgMap.end()) {
		return it->second;
	}

	// 2. ���� WM_USER �Զ����û���Ϣ
	if (msg >= WM_USER && msg < WM_APP) {
		std::ostringstream oss;
		oss << "WM_USER+" << (msg - WM_USER);
		return oss.str();
	}

	// 3. ���� WM_APP �Զ���Ӧ����Ϣ
	if (msg >= WM_APP && msg <= 0xBFFF) {
		std::ostringstream oss;
		oss << "WM_APP+" << (msg - WM_APP);
		return oss.str();
	}

	// 4. ������ע��Ĵ�����Ϣ������δ֪��Ϣ
	std::ostringstream oss;
	if (msg >= 0xC000 && msg <= 0xFFFF) {
		oss << "Registered Message (0x" << std::hex << std::uppercase << msg << ")";
	}
	else {
		oss << "Unknown Message (0x" << std::hex << std::uppercase << msg << ")";
	}
	return oss.str();
}

BOOL WINAPI HookedReadFile(
	HANDLE       hFile,
	LPVOID       lpBuffer,
	DWORD        nNumberOfBytesToRead,
	LPDWORD      lpNumberOfBytesRead,
	LPOVERLAPPED lpOverlapped
) {
	if (g_FileCache.TryGetFromCache(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped)) {
		// �������У�ֱ�ӷ��سɹ�
		return TRUE;
	}
	BOOL bSuccess = TrueReadFile(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped);


	if (bSuccess && lpNumberOfBytesRead != nullptr && *lpNumberOfBytesRead > 0) {
		g_FileCache.PutInCache(hFile, lpBuffer, *lpNumberOfBytesRead, lpOverlapped);
	}

	return bSuccess;
	
}

LRESULT WINAPI HackedDispatchMessageW(const MSG* lpMsg) {
	if (g_isLoggingActive && lpMsg != nullptr) {
		// 1. �ӹ�������ȡ��������
		FilterAction action = MessageFilter::GetInstance().check(lpMsg->message);

		// 2. ���ݶ���ִ�в���
		if (action == FilterAction::BLACKLIST_AND_FILTER) {
			// ���ǵ�һ�γ��ޣ��ӻ�������ɾ������Ϣ��������ʷ��¼
			FileLogger::GetInstance().RemoveMessagesFromBuffer(lpMsg->message);
			// ע�⣺��ǰ������Ϣ���ᱻ��¼
		}
		else if (action == FilterAction::LOG) {
			// ������¼������Ϣ
			std::string msgString = MessageIDToString(lpMsg->message);
			std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
			std::wstring msgWideString = converter.from_bytes(msgString);

			FileLogger::GetInstance().Log(
				lpMsg->message, // ������ϢID
				L"HWND: 0x%p, Msg: %s (0x%X), wParam: 0x%p, lParam: 0x%p",
				lpMsg->hwnd,
				msgWideString.c_str(),
				lpMsg->message,
				lpMsg->wParam,
				lpMsg->lParam
			);
		}
		// ��������� FilterAction::FILTER����ʲô������
	}
	return TrueDispatchMessageW(lpMsg);
}

// ���ƺ���ʵ��
void StartMessageLogging() {
	g_isLoggingActive = true;
	FileLogger::GetInstance().Log(0, L"--- Logging Started by Server Command ---");
}

void StopMessageLogging() {
	FileLogger::GetInstance().Log(0, L"--- Logging Stopped by Server Command ---");
	g_isLoggingActive = false;
}

bool AttachHooks() {
	SimpleIniParser parser;
	if (!parser.load("config.ini")) {
        std::cerr << "�޷����������ļ�" << std::endl;
        return 1;
    }
	std::string logFilePath=parser.get("server", "logFilePath");
	try {
		FileLogger::GetInstance().Init(std::wstring(logFilePath.begin(), logFilePath.end()));
		FileLogger::GetInstance().Log(0, L"--- Hooks Attached ---"); // ʹ��һ������ID (0)
		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		DetourAttach(&(PVOID&)TrueDispatchMessageW, HackedDispatchMessageW);
		//DetourAttach(&(PVOID&)TrueReadFile, HookedReadFile);
		if (DetourTransactionCommit() != NO_ERROR) {
			FileLogger::GetInstance().Log(0, L"--- !! HOOK ATTACHMENT FAILED !! ---");
			return false;
		}
	}
	catch (...) { return false; }
	return true;
}

bool DetachHooks() {
	try {
		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		DetourDetach(&(PVOID&)TrueDispatchMessageW, HackedDispatchMessageW);
		//DetourDetach(&(PVOID&)TrueReadFile, HookedReadFile);
		DetourTransactionCommit();
		FileLogger::GetInstance().Log(0, L"--- Hooks Detached ---");
		FileLogger::GetInstance().Shutdown(); // ������Ὣ����ʣ����־ˢ���ļ�
	}
	catch (...) { return false; }
	return true;
}