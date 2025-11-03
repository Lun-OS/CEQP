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
    // 禁用游戏手柄导航，减少输入处理与后台轮询开销
    // io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

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
    // 记录虚拟屏几何与可见状态，避免每帧重复 SetWindowPos/ShowWindow
    int last_vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int last_vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int last_vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int last_vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    bool last_overlay_visible = g_overlayVisible;

    // 主循环
    while (!done) {
        auto frameStart = std::chrono::steady_clock::now();
        // 仅在虚拟屏参数变化或叠加层从隐藏切换为可见时更新窗口到全屏覆盖
        int nvx = GetSystemMetrics(SM_XVIRTUALSCREEN);
        int nvy = GetSystemMetrics(SM_YVIRTUALSCREEN);
        int nvw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        int nvh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
        bool screen_changed = nvx != last_vx || nvy != last_vy || nvw != last_vw || nvh != last_vh;
        if (screen_changed || (!last_overlay_visible && g_overlayVisible)) {
            ::SetWindowPos(hwnd, HWND_TOPMOST, nvx, nvy, nvw, nvh,
                           SWP_NOACTIVATE | SWP_NOSENDCHANGING);
            last_vx = nvx; last_vy = nvy; last_vw = nvw; last_vh = nvh;
        }

        // 处理 Win32 消息
        MSG msg;
        while (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done) break;

        // 当叠加层不可见时，停止渲染并等待消息，避免忙轮询占用 CPU
        if (!g_overlayVisible) {
            // 确保窗口隐藏，减少 DWM 合成负担
            ShowWindow(hwnd, SW_HIDE);
            // 阻塞等待下一条消息（热键/退出等），CPU 几乎为零占用
            WaitMessage();
            continue;
        } else if (!last_overlay_visible && g_overlayVisible) {
            // 叠加层刚变为可见时显示并允许交互
            ShowWindow(hwnd, SW_SHOWNORMAL);
        }

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
        // 若启用了帧等待对象，则在呈现前等待，避免CPU忙等
        if (g_overlayVisible && g_frameLatencyWaitableObject) {
            HANDLE h = g_frameLatencyWaitableObject;
            // 同时响应系统消息，避免阻塞消息泵
            MsgWaitForMultipleObjects(1, &h, FALSE, 16, QS_ALLEVENTS);
        }
        // 当窗口被遮挡/最小化时 Present 可能返回 DXGI_STATUS_OCCLUDED
        // 关闭VSync并根据支持情况启用撕裂呈现，减少对其他进程的影响
        UINT presentFlags = g_dxgiAllowTearing ? DXGI_PRESENT_ALLOW_TEARING : 0;
        HRESULT hrPresent = g_pSwapChain->Present(0, presentFlags);
        if (hrPresent == DXGI_STATUS_OCCLUDED) {
            // 轻微休眠，避免在遮挡状态下忙循环占用 CPU
            Sleep(10);
        } else {
            // 在可见时限制帧率为 60FPS，兼顾流畅度与占用
            const int targetFrameMs = 16;
            auto frameEnd = std::chrono::steady_clock::now();
            int elapsedMs = (int)std::chrono::duration_cast<std::chrono::milliseconds>(frameEnd - frameStart).count();
            int sleepMs = targetFrameMs - elapsedMs;
            if (sleepMs > 0) Sleep(sleepMs);
        }

        // 更新上一次可见状态（避免重复 ShowWindow/SetWindowPos）
        last_overlay_visible = g_overlayVisible;
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
