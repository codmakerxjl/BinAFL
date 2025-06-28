// pch.h: 这是预编译标头文件。
// 下方列出的文件仅编译一次，提高了将来生成的生成性能。
// 这还将影响 IntelliSense 性能，包括代码完成和许多代码浏览功能。


#ifndef PCH_H
#define PCH_H
#include <windows.h>
#include "SharedMemoryIPC.h"
bool Initialize();

#endif //PCH_H
