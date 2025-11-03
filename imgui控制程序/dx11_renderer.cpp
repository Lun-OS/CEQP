#include "dx11_renderer.h"
#include "globals.h"
#include "settings.h"
#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_win32.h"
#include "ImGui/imgui_impl_dx11.h"
#include <d3d11.h>
#include <chrono>

// Direct3D 设备创建
bool CreateDeviceD3D(HWND hWnd) {
    // 设置交换链
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    if (D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext) != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = NULL; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = NULL; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }
}

void CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer = nullptr;
    HRESULT hr = g_pSwapChain ? g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer)) : E_FAIL;
    if (SUCCEEDED(hr) && pBackBuffer) {
        HRESULT hrRTV = g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
        if (FAILED(hrRTV)) {
            g_mainRenderTargetView = nullptr;
        }
        pBackBuffer->Release();
    } else {
        g_mainRenderTargetView = nullptr;
    }
}

void CleanupRenderTarget() {
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = NULL; }
}

// 前向声明 ImGui Win32 处理函数
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 消息处理
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // 热键优先处理（WM_HOTKEY 不会被 ImGui 接管）
    if (msg == WM_HOTKEY)
    {
        if (wParam == 1) { // 我们注册的热键 ID
            g_overlayVisible = !g_overlayVisible;
            return 0;
        }
    }

    // 当叠加层可见时，才让 ImGui 接管输入
    if (g_overlayVisible)
    {
        if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
            return true;
    }

    switch (msg) {
    case WM_APP_SINGLE_INSTANCE_NOTIFY:
        g_overlayVisible = true;
        g_notifyUntil = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        ShowWindow(hWnd, SW_SHOWNORMAL);
        SetForegroundWindow(hWnd);
        return 0;
    case WM_SIZE:
        if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // 禁用 ALT 应用程序菜单
            return 0;
        break;
    case WM_DESTROY:
        // 注销热键
        UnregisterHotKey(hWnd, 1);
        // 保存设置
        SaveSettings();
        if (g_singleInstanceMutex) { CloseHandle(g_singleInstanceMutex); g_singleInstanceMutex = nullptr; }
        PostQuitMessage(0);
        return 0;
    case WM_KEYDOWN:
        // 作为备用：窗口聚焦时也可用
        if ((int)wParam == g_toggleKeyVk) {
            g_overlayVisible = !g_overlayVisible;
            return 0;
        }
        break;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}