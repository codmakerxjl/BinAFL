


#ifndef PCH_H
#define PCH_H
#include <windows.h>
#include "SharedMemoryIPC.h"
#include <thread>
#include <format>
bool Initialize();

std::string buildAgentPrompt(
    const std::string& logFilePath,
    const std::string& outputDir,
    int serverPort);
#endif //PCH_H
