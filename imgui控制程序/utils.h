#pragma once
#include <string>
#include <windows.h>

// 字符串转换工具函数
std::string WStringToUtf8(const std::wstring& w);
std::wstring Utf8ToWString(const std::string& s);

// 窗口查找相关函数
BOOL IsMainWindow(HWND handle);
HWND FindMainWindow(DWORD process_id);
DWORD FindProcessId(const std::wstring& processName);

// 窗口枚举回调数据结构
struct EnumWindowsData {
    DWORD process_id;
    HWND window_handle;
};

// 窗口枚举回调函数
BOOL CALLBACK EnumWindowsCallback(HWND handle, LPARAM lParam);