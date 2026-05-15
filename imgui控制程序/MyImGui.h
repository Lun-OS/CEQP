#pragma once
#ifndef MYIMGUI_H
#define MYIMGUI_H

#include <windows.h>
#include <d3d11.h>
#include <functional>

struct ImGuiContext;
struct ImGuiIO;

class MyImGui
{
public:
    using DrawFunction = std::function<void()>;

    bool Init(HWND hWnd, const wchar_t* windowTitle, int width, int height, int flag, float FontSize = 18.0f);
    void Run(DrawFunction drawFunc);
    void Shutdown();

private:
    bool CreateDeviceD3D();
    void CleanupDeviceD3D();
    void CreateRenderTarget();
    void CleanupRenderTarget();
    static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    HWND m_hwnd = nullptr;
    WNDCLASSEXW m_wc = {};
    ID3D11Device* m_pd3dDevice = nullptr;
    ID3D11DeviceContext* m_pd3dDeviceContext = nullptr;
    IDXGISwapChain* m_pSwapChain = nullptr;
    ID3D11RenderTargetView* m_mainRenderTargetView = nullptr;
    UINT m_ResizeWidth = 0;
    UINT m_ResizeHeight = 0;
    bool m_SwapChainOccluded = false;
};

#endif // MYIMGUI_H