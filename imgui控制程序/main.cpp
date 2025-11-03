// CEQP Control Program - Main Entry Point
// 重构后的主入口文件，通过调用其他模块实现功能

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_win32.h"
#include "ImGui/imgui_impl_dx11.h"
#include "globals.h"
#include "utils.h"
#include "i18n.h"
#include "themes.h"
#include "settings.h"
#include "dx11_renderer.h"
#include "ui.h"
#include <tchar.h>
#include <chrono>

// Win32 + DX11 主程序入口
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    // DPI 适配
    ImGui_ImplWin32_EnableDpiAwareness();

    // 单实例运行检测
    g_singleInstanceMutex = CreateMutexW(NULL, TRUE, L"CEQPControlApp_SingleInstance");
    if (g_singleInstanceMutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND existing = FindWindowW(L"CEQPControlApp", NULL);
        if (existing) {
            ShowWindow(existing, SW_SHOWNORMAL);
            SetForegroundWindow(existing);
            PostMessage(existing, WM_APP_SINGLE_INSTANCE_NOTIFY, 0, 0);
        }
        return 0;
    }

    // 注册窗口类
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L,
                      GetModuleHandle(NULL), NULL, NULL, NULL, NULL,
                      _T("CEQPControlApp"), NULL };
    RegisterClassEx(&wc);

    // 创建全屏透明窗口
    int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    HWND hwnd = CreateWindowEx(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        wc.lpszClassName, _T("CEQP Control Program"),
        WS_POPUP, vx, vy, vw, vh,
        NULL, NULL, wc.hInstance, NULL);

    // 设置透明度和颜色键
    SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 255, LWA_COLORKEY | LWA_ALPHA);

    // 初始化 Direct3D
    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        UnregisterClass(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // 显示窗口
    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);
    SetWindowPos(hwnd, HWND_TOPMOST, vx, vy, vw, vh, 0);

    // 保存主窗口句柄并加载设置
    g_mainHwnd = hwnd;
    LoadSettings();
    // 应用禁止截图设置（默认开启）
    ApplyScreenshotExclusion(g_excludeFromCapture);

    // 初始化 ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    // 设置 ImGui 样式和主题
    ImGui::StyleColorsDark();
    ApplyImGuiTheme(g_imguiTheme);

    // 设置平台/渲染器后端
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // 加载字体和语言设置
    ReloadFonts();
    I18N::setLang(g_initialLanguage == 1 ? I18N::Lang::ZH : I18N::Lang::EN);

    // 注册全局热键
    RegisterHotKey(g_mainHwnd, 1, 0, g_toggleKeyVk);

    // 初始化应用程序状态
    AppUI appUI;
    appUI.languageSelection = g_initialLanguage;
    appUI.themeSelection = g_imguiTheme;
    bool done = false;

    // 主循环
    while (!done) {
        // 动态维持虚拟屏覆盖
        int nvx = GetSystemMetrics(SM_XVIRTUALSCREEN);
        int nvy = GetSystemMetrics(SM_YVIRTUALSCREEN);
        int nvw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        int nvh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
        ::SetWindowPos(hwnd, HWND_TOPMOST, nvx, nvy, nvw, nvh, 0);

        // 处理 Win32 消息
        MSG msg;
        while (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done) break;

        // 心跳保活检测
        if (appUI.isConnected) {
            auto now = std::chrono::steady_clock::now();
            if (now - appUI.lastHeartbeat >= std::chrono::seconds(2)) {
                if (!appUI.client.ping()) {
                    appUI.addErrorLog(I18N::tr("Heartbeat failed: ").c_str() + appUI.client.getLastError());
                    appUI.isConnected = false;
                }
                appUI.lastHeartbeat = now;
            }
        }

        // 渲染UI（仅在可见时）
        if (g_overlayVisible) {
            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            RenderUI(appUI);

            ImGui::Render();
        }

        // DirectX11 渲染
        const float clear_color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color);
        
        if (g_overlayVisible) {
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        }

        g_pSwapChain->Present(1, 0); // 垂直同步
    }

    // 清理资源
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    DestroyWindow(hwnd);
    UnregisterClass(wc.lpszClassName, wc.hInstance);

    return 0;
}
