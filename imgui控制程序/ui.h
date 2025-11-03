#pragma once

#include "ImGui/imgui.h"
#include "CETCP.h"
#include <vector>
#include <string>
#include <array>
#include <chrono>

// 日志条目结构
struct LogEntry {
    std::string timestamp;
    std::string message;
    ImVec4 color;

    LogEntry(const std::string& msg, ImVec4 col = ImVec4(1, 1, 1, 1));
};

// 应用UI状态结构
struct AppUI {
    // TCP客户端
    CETCP client;

    // 连接设置
    char serverHost[128] = "127.0.0.1";
    int serverPort = 9178;
    bool isConnected = false;

    // 内存操作
    char addressInput[256] = "";
    char lengthInput[64] = "4";
    char valueInput[512] = "";
    std::vector<uint8_t> lastReadData;
    int writeType = 0;     // 0:Hex bytes,1:u8,2:u16,3:u32,4:u64,5:i8,6:i16,7:i32,8:i64,9:f32,10:f64
    int writeBase = 16;    // 10 or 16

    // 指针链
    char ptrBaseInput[256] = "";
    int offsetCount = 2;
    std::vector<std::array<char, 64>> offsetInputs;
    std::vector<int> offsetBaseTypes; // 每个偏移的数值类型（10/16）
    char ptrLengthInput[64] = "4";

    // 指针链高级选项
    int pointerSizeMode = 0;  // 0:Auto, 4:4-byte, 8:8-byte
    bool noFinalDeref = true;

    // 指针链写入
    char ptrValueInput[512] = "";
    int ptrWriteType = 0;     // 0:Hex bytes,1:u8,2:u16,3:u32,4:u64,5:i8,6:i16,7:i32,8:i64,9:f32,10:f64
    int ptrWriteBase = 16;    // 10 or 16

    // 日志系统
    std::vector<LogEntry> logs;
    bool autoScroll = true;
    int maxLogEntries = 1000;

    // UI设置
    int languageSelection = 0;
    int themeSelection = 0;

    // 关于窗口
    bool showAbout = false;

    // 心跳检测
    std::chrono::steady_clock::time_point lastHeartbeat;

    // 日志方法
    void addLog(const std::string& msg, const ImVec4& col);
    void addInfoLog(const std::string& msg);
    void addErrorLog(const std::string& msg);
    void addSuccessLog(const std::string& msg);
};

// UI渲染函数声明
void RenderConnectionPanel(AppUI& ui);
void RenderMemoryTab(AppUI& ui);
void RenderPointerChainTab(AppUI& ui);
void RenderSettingsTab(AppUI& ui);
void RenderSharedDataDisplay(AppUI& ui);
void RenderLogTab(AppUI& ui);
void RenderUI(AppUI& ui);

// 工具函数
int DetectPointerSizeForPid(DWORD pid);