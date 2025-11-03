#include "dx11_renderer.h"
#include "globals.h"
#include "settings.h"
#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_win32.h"
#include "ImGui/imgui_impl_dx11.h"
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dxgi1_5.h>
#include <chrono>

// Direct3D 设备创建
bool CreateDeviceD3D(HWND hWnd) {
    // 创建 D3D11 设备与上下文
    UINT createDeviceFlags = D3D11_CREATE_DEVICE_SINGLETHREADED;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    if (D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags,
                          featureLevelArray, 2, D3D11_SDK_VERSION,
                          &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext) != S_OK) {
        return false;
    }

    // 若窗口为层叠(WS_EX_LAYERED)，避免使用 FLIP 交换链（会破坏透明），直接走旧模型
    bool isLayered = (GetWindowLongPtr(hWnd, GWL_EXSTYLE) & WS_EX_LAYERED) != 0;

    // 优先在非层叠窗口上尝试创建 FLIP 模式交换链并启用帧等待对象
    if (!isLayered) {
        IDXGIDevice* dxgiDevice = nullptr;
        if (SUCCEEDED(g_pd3dDevice->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDevice))) {
            IDXGIAdapter* adapter = nullptr;
            if (SUCCEEDED(dxgiDevice->GetAdapter(&adapter))) {
                IDXGIFactory2* factory2 = nullptr;
                if (SUCCEEDED(adapter->GetParent(__uuidof(IDXGIFactory2), (void**)&factory2))) {
                    DXGI_SWAP_CHAIN_DESC1 desc1 = {};
                    desc1.Width = 0; desc1.Height = 0;
                    desc1.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                    desc1.SampleDesc.Count = 1; desc1.SampleDesc.Quality = 0;
                    desc1.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
                    desc1.BufferCount = 3; // 三缓冲以提升平滑度
                    desc1.Scaling = DXGI_SCALING_STRETCH;
                    desc1.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
                    desc1.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
                    desc1.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

                    // 检测是否支持撕裂呈现
                    BOOL allowTearing = FALSE;
                    IDXGIFactory5* factory5 = nullptr;
                    if (SUCCEEDED(factory2->QueryInterface(__uuidof(IDXGIFactory5), (void**)&factory5))) {
                        factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing));
                        if (allowTearing) {
                            desc1.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
                            g_dxgiAllowTearing = true;
                        }
                        factory5->Release();
                    }

                    IDXGISwapChain1* swap1 = nullptr;
                    if (SUCCEEDED(factory2->CreateSwapChainForHwnd(g_pd3dDevice, hWnd, &desc1, nullptr, nullptr, &swap1))) {
                        swap1->QueryInterface(__uuidof(IDXGISwapChain), (void**)&g_pSwapChain);
                        IDXGISwapChain2* swap2 = nullptr;
                        if (SUCCEEDED(swap1->QueryInterface(__uuidof(IDXGISwapChain2), (void**)&swap2))) {
                            IDXGIDevice1* dxgiDev1 = nullptr;
                            if (SUCCEEDED(g_pd3dDevice->QueryInterface(__uuidof(IDXGIDevice1), (void**)&dxgiDev1))) {
                                dxgiDev1->SetMaximumFrameLatency(1);
                                dxgiDev1->Release();
                            }
                            g_frameLatencyWaitableObject = swap2->GetFrameLatencyWaitableObject();
                            swap2->Release();
                        }
                        swap1->Release();
                    }
                    factory2->Release();
                }
                adapter->Release();
            }
            dxgiDevice->Release();
        }
    }

    // 若 FLIP 模式创建失败，退回传统交换链
    if (!g_pSwapChain) {
        DXGI_SWAP_CHAIN_DESC sd = {};
        sd.BufferCount = 2;
        sd.BufferDesc.Width = 0;
        sd.BufferDesc.Height = 0;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferDesc.RefreshRate.Numerator = 60;
        sd.BufferDesc.RefreshRate.Denominator = 1;
        sd.Flags = 0;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = hWnd;
        sd.SampleDesc.Count = 1;
        sd.SampleDesc.Quality = 0;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        IDXGIDevice* dxgiDev = nullptr;
        IDXGIAdapter* adapter = nullptr;
        IDXGIFactory* factory = nullptr;
        if (SUCCEEDED(g_pd3dDevice->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDev)) &&
            SUCCEEDED(dxgiDev->GetAdapter(&adapter)) &&
            SUCCEEDED(adapter->GetParent(__uuidof(IDXGIFactory), (void**)&factory))) {
            if (FAILED(factory->CreateSwapChain(g_pd3dDevice, &sd, &g_pSwapChain))) {
                factory->Release(); adapter->Release(); dxgiDev->Release();
                return false;
            }
            factory->Release(); adapter->Release(); dxgiDev->Release();
        } else {
            return false;
        }
        // 限制帧延迟（退回路径）
        IDXGIDevice1* dxgiDev1 = nullptr;
        if (SUCCEEDED(g_pd3dDevice->QueryInterface(__uuidof(IDXGIDevice1), (void**)&dxgiDev1))) {
            dxgiDev1->SetMaximumFrameLatency(1);
            dxgiDev1->Release();
        }
    }

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
            // 根据叠加层可见性切换窗口显示状态，隐藏时不参与合成
            ShowWindow(hWnd, g_overlayVisible ? SW_SHOWNOACTIVATE : SW_HIDE);
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
            ShowWindow(hWnd, g_overlayVisible ? SW_SHOWNORMAL : SW_HIDE);
            return 0;
        }
        break;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}
