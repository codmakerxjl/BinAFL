


#ifndef PCH_H
#define PCH_H
#include <windows.h>
#include "SharedMemoryIPC.h"
#include <thread>
extern std::atomic<bool> g_bExitLogThread;
bool Initialize();
void log_writer_thread_proc();
#endif //PCH_H
