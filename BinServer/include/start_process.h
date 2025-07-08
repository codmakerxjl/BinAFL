#pragma once
#ifndef START_PROCESS_H
#define START_PROCESS_H
#include <Windows.h>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>
bool startAndInjectProcess(PROCESS_INFORMATION& pi, const std::wstring& executablePath, const std::wstring& dllPath, const std::wstring& arguments);
#endif 