
#include "pch.h"

bool Initialize() {
	//��ʼ��ipc 
	SharedMemoryIPC ipc(SharedMemoryIPC::Role::SERVER);

	return 0;
}