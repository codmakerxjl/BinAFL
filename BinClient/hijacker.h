#pragma once
#ifndef HIJACKER_H
#define HIJACKER_H
#include <WinUser.h>
#include "detours.h"


// 安装API钩子并初始化日志通道。
bool AttachHooks();

// 卸载API钩子并清理日志通道。
bool DetachHooks();

#endif 