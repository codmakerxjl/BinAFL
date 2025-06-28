// pch.cpp: 与预编译标头对应的源文件

#include "pch.h"

bool Initialize() {
		//初始化ipc  
	SharedMemoryIPC ipc(SharedMemoryIPC::Role::CLIENT);

	return 0;
}