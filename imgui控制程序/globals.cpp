#include "globals.h"

// Direct3D全局变量定义
ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

// 全局 UI/设置状态定义
bool g_overlayVisible = true;             // 叠加层是否显示（F7 切换）
int  g_toggleKeyVk = VK_F7;               // 热键（默认 F7）
HWND g_mainHwnd = nullptr;                // 主窗口句柄（用于 RegisterHotKey）
HANDLE g_singleInstanceMutex = nullptr;   // 单实例互斥
std::chrono::steady_clock::time_point g_notifyUntil; // 单实例提示到期时间
bool g_initialPosApplied = false;         // 是否已应用初始位置
ImVec2 g_lastUiPos = ImVec2(50, 50);      // 最近一次 UI 位置
int g_savedPosX = -1;                     // 保存的 UI X
int g_savedPosY = -1;                     // 保存的 UI Y
int g_imguiTheme = 0;                     // 主题：0 Dark,1 Light,2 Classic,3 Corporate Gray,4 Cherry
int g_initialLanguage = 0;                // 初始语言（0:EN,1:ZH），用于从 settings.ini 读取
bool g_excludeFromCapture = true;         // 禁止截图（默认开启）
