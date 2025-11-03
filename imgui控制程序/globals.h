#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <d3d11.h>
#include <chrono>
#include "ImGui/imgui.h"

// Direct3D全局变量声明
extern ID3D11Device* g_pd3dDevice;
extern ID3D11DeviceContext* g_pd3dDeviceContext;
extern IDXGISwapChain* g_pSwapChain;
extern ID3D11RenderTargetView* g_mainRenderTargetView;
// 帧等待对象与撕裂支持（DXGI FLIP 模式优化）
extern HANDLE g_frameLatencyWaitableObject;
extern bool   g_dxgiAllowTearing;

// 全局 UI/设置状态声明
extern bool g_overlayVisible;             // 叠加层是否显示（F7 切换）
extern int  g_toggleKeyVk;               // 热键（默认 F7）
extern HWND g_mainHwnd;                  // 主窗口句柄（用于 RegisterHotKey）
extern HANDLE g_singleInstanceMutex;     // 单实例互斥
extern std::chrono::steady_clock::time_point g_notifyUntil; // 单实例提示到期时间
extern bool g_initialPosApplied;         // 是否已应用初始位置
extern ImVec2 g_lastUiPos;               // 最近一次 UI 位置
extern ImVec2 g_lastUiSize;              // 最近一次 UI 尺寸
extern int g_savedPosX;                  // 保存的 UI X
extern int g_savedPosY;                  // 保存的 UI Y
extern int g_imguiTheme;                 // 主题：0 Dark,1 Light,2 Classic,3 Corporate Gray,4 Cherry
extern int g_initialLanguage;           // 初始语言（0:EN,1:ZH），用于从 settings.ini 读取
extern bool g_excludeFromCapture;       // 禁止截图（默认开启）

// 常量定义
constexpr UINT WM_APP_SINGLE_INSTANCE_NOTIFY = WM_APP + 1; // 多开提示消息
