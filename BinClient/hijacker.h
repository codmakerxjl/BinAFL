#pragma once
#ifndef HIJACKER_H
#define HIJACKER_H
#include <WinUser.h>
#include "detours.h"


// ��װAPI���Ӳ���ʼ����־ͨ����
bool AttachHooks();

// ж��API���Ӳ�������־ͨ����
bool DetachHooks();

#endif 