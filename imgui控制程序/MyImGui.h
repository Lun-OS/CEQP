#pragma once

#include <windows.h>
#include <d3d11.h>
#include <functional>
#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_win32.h"
#include "ImGui/imgui_impl_dx11.h"

typedef std::function<void()> DrawFunction;

class MyImGui
{
public:
    MyImGui() = default;
    ~MyImGui() = default;

    bool Init(HWND hWnd, const wchar_t* windowTitle, int width, int height, int flag, float FontSize);
    void Run(DrawFunction drawFunc);
    void Shutdown();

private:
    bool CreateDeviceD3D();
    void CleanupDeviceD3D();
    void CreateRenderTarget();
    void CleanupRenderTarget();
    static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    HWND m_hwnd = nullptr;
    WNDCLASSEXW m_wc{};
    ID3D11Device* m_pd3dDevice = nullptr;
    ID3D11DeviceContext* m_pd3dDeviceContext = nullptr;
    IDXGISwapChain* m_pSwapChain = nullptr;
    ID3D11RenderTargetView* m_mainRenderTargetView = nullptr;
    UINT m_ResizeWidth = 0;
    UINT m_ResizeHeight = 0;
    bool m_SwapChainOccluded = false;
};
