#include "MyImGui.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"
#include <iostream>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

bool MyImGui::Init(HWND hWnd, const wchar_t* windowTitle, int width, int height, int flag, float FontSize)
{
    if (windowTitle == nullptr) {
        std::cerr << "Error: Window title cannot be null" << std::endl;
        return false;
    }

    if (width <= 0 || height <= 0) {
        std::cerr << "Error: Window dimensions must be positive (width: " << width << ", height: " << height << ")" << std::endl;
        return false;
    }

    if (width > 7680 || height > 4320) {
        std::cerr << "Warning: Very large window dimensions (width: " << width << ", height: " << height << ")" << std::endl;
    }

    if (FontSize <= 0.0f || FontSize > 72.0f) {
        std::cerr << "Warning: Unusual font size (" << FontSize << "), using default 18.0f" << std::endl;
        FontSize = 18.0f;
    }

    if (hWnd == nullptr)
    {
        ImGui_ImplWin32_EnableDpiAwareness();

        HMONITOR monitor = ::MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);
        if (monitor == nullptr) {
            std::cerr << "Error: Failed to get primary monitor" << std::endl;
            return false;
        }

        float main_scale = ImGui_ImplWin32_GetDpiScaleForMonitor(monitor);
        if (main_scale <= 0.0f) {
            std::cerr << "Warning: Invalid DPI scale factor, using 1.0f" << std::endl;
            main_scale = 1.0f;
        }

        m_wc = {
            sizeof(m_wc),
            CS_CLASSDC,
            WndProc,
            0L,
            0L,
            GetModuleHandle(nullptr),
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            L"ImGui Class",
            nullptr
        };

        if (!::RegisterClassExW(&m_wc)) {
            DWORD error = GetLastError();
            if (error != ERROR_CLASS_ALREADY_EXISTS) {
                std::cerr << "Error: Failed to register window class. Error code: " << error << std::endl;
                return false;
            }
        }

        DWORD windowStyle = WS_POPUP;
        DWORD exWindowStyle = WS_EX_APPWINDOW;

        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);

        int x = 0;
        int y = 0;
        int w = screenWidth;
        int h = screenHeight;

        if (w <= 0 || h <= 0) {
            std::cerr << "Error: Invalid scaled window dimensions (w: " << w << ", h: " << h << ")" << std::endl;
            return false;
        }

        m_hwnd = ::CreateWindowExW(
            exWindowStyle,
            m_wc.lpszClassName,
            windowTitle,
            windowStyle,
            x, y, w, h,
            nullptr,
            nullptr,
            m_wc.hInstance,
            this
        );

        if (m_hwnd == nullptr) {
            DWORD error = GetLastError();
            std::cerr << "Error: Failed to create window. Error code: " << error << std::endl;
            ::UnregisterClassW(m_wc.lpszClassName, m_wc.hInstance);
            return false;
        }

        if (!::ShowWindow(m_hwnd, flag)) {
            std::cerr << "Warning: ShowWindow returned false" << std::endl;
        }

        if (!::UpdateWindow(m_hwnd)) {
            std::cerr << "Warning: UpdateWindow failed" << std::endl;
        }
    }
    else
    {
        if (!::IsWindow(hWnd)) {
            std::cerr << "Error: Provided HWND is not a valid window handle" << std::endl;
            return false;
        }

        m_hwnd = hWnd;
    }

    if (!CreateDeviceD3D())
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(m_wc.lpszClassName, m_wc.hInstance);
        return false;
    }

    IMGUI_CHECKVERSION();
    ImGuiContext* context = ImGui::CreateContext();
    if (!context)
    {
        std::wcerr << L"Error: Failed to create ImGui context" << std::endl;
        CleanupDeviceD3D();
        ::UnregisterClassW(m_wc.lpszClassName, m_wc.hInstance);
        return false;
    }

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(m_hwnd);

    HMONITOR monitor = ::MonitorFromWindow(m_hwnd, MONITOR_DEFAULTTOPRIMARY);
    if (!monitor)
    {
        std::wcerr << L"Warning: Failed to get monitor handle, using default DPI scaling" << std::endl;
    }

    float main_scale = ImGui_ImplWin32_GetDpiScaleForMonitor(monitor);

    if (main_scale <= 0.0f || main_scale > 10.0f)
    {
        std::wcerr << L"Warning: Invalid DPI scale factor (" << main_scale << L"), using default scale of 1.0" << std::endl;
        main_scale = 1.0f;
    }

    ImGui::GetStyle().ScaleAllSizes(main_scale);
    ImGui::GetStyle().FontScaleDpi = main_scale;

    if (!ImGui_ImplWin32_Init(m_hwnd))
    {
        std::wcerr << L"Error: Failed to initialize ImGui Win32 backend" << std::endl;
        ImGui::DestroyContext();
        CleanupDeviceD3D();
        ::UnregisterClassW(m_wc.lpszClassName, m_wc.hInstance);
        return false;
    }

    if (!ImGui_ImplDX11_Init(m_pd3dDevice, m_pd3dDeviceContext))
    {
        std::wcerr << L"Error: Failed to initialize ImGui DirectX 11 backend" << std::endl;
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        CleanupDeviceD3D();
        ::UnregisterClassW(m_wc.lpszClassName, m_wc.hInstance);
        return false;
    }

    io.IniFilename = nullptr;

    const ImWchar* chinese_ranges = io.Fonts->GetGlyphRangesChineseSimplifiedCommon();
    ImFont* Font = nullptr;

    const char* kSystemFontCandidates[] = {
        "C:/Windows/Fonts/msyh.ttc",
        "C:/Windows/Fonts/msyh.ttf",
        "C:/Windows/Fonts/msyhbd.ttc",
        "C:/Windows/Fonts/simhei.ttf",
        "C:/Windows/Fonts/simsun.ttc",
        "C:/Windows/Fonts/Deng.ttf",
        "C:/Windows/Fonts/Dengb.ttf"
    };

    for (const char* path : kSystemFontCandidates)
    {
        ImFontConfig cfg;
        cfg.FontNo = 0;
        Font = io.Fonts->AddFontFromFileTTF(path, FontSize, &cfg, chinese_ranges);
        if (Font)
        {
            std::wcout << L"Loaded system font for Chinese: " << path << std::endl;
            break;
        }
    }

    if (!Font)
    {
        Font = io.Fonts->AddFontDefault();
        std::wcerr << L"Warning: System fonts failed; using ImGui default font." << std::endl;
    }

    if (io.Fonts->Fonts.Size > 0)
        io.FontDefault = io.Fonts->Fonts[0];
    ImGui_ImplDX11_InvalidateDeviceObjects();
    ImGui_ImplDX11_CreateDeviceObjects();

    return true;
}

void MyImGui::Run(DrawFunction drawFunc)
{
    if (!drawFunc)
    {
        std::wcerr << L"Error: Draw function parameter is null" << std::endl;
        return;
    }

    if (!m_hwnd || !m_pd3dDevice || !m_pd3dDeviceContext || !m_pSwapChain)
    {
        std::wcerr << L"Error: MyImGui not properly initialized. Call Init() first." << std::endl;
        return;
    }

    std::wcout << L"Starting main rendering loop..." << std::endl;

    bool done = false;
    while (!done)
    {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done) break;

        if (m_SwapChainOccluded && m_pSwapChain->Present(0, 0) == DXGI_STATUS_OCCLUDED)
        {
            ::Sleep(10);
            continue;
        }
        m_SwapChainOccluded = false;

        if (m_ResizeWidth != 0 && m_ResizeHeight != 0)
        {
            CleanupRenderTarget();
            m_pSwapChain->ResizeBuffers(0, m_ResizeWidth, m_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            m_ResizeWidth = m_ResizeHeight = 0;
            CreateRenderTarget();
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        drawFunc();

        ImGui::Render();

        const float clear_color[4] = { 0.0f, 0.0f, 0.0f, 1.00f };
        m_pd3dDeviceContext->OMSetRenderTargets(1, &m_mainRenderTargetView, nullptr);
        m_pd3dDeviceContext->ClearRenderTargetView(m_mainRenderTargetView, clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        HRESULT hr = m_pSwapChain->Present(0, DXGI_PRESENT_DO_NOT_WAIT);
        m_SwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
    }
}

void MyImGui::Shutdown()
{
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();

    if (m_hwnd)
        ::DestroyWindow(m_hwnd);
    ::UnregisterClassW(m_wc.lpszClassName, m_wc.hInstance);
}

bool MyImGui::CreateDeviceD3D()
{
    if (!m_hwnd || !::IsWindow(m_hwnd))
    {
        std::wcerr << L"Error: Invalid window handle for DirectX device creation" << std::endl;
        return false;
    }

    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = m_hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };

    std::wcout << L"Attempting to create DirectX 11 device with hardware acceleration..." << std::endl;

    HRESULT res = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        createDeviceFlags,
        featureLevelArray, 2,
        D3D11_SDK_VERSION,
        &sd,
        &m_pSwapChain,
        &m_pd3dDevice,
        &featureLevel,
        &m_pd3dDeviceContext
    );

    if (res == DXGI_ERROR_UNSUPPORTED)
    {
        std::wcout << L"Hardware acceleration not supported, falling back to WARP software rendering..." << std::endl;

        res = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_WARP,
            nullptr,
            createDeviceFlags,
            featureLevelArray, 2,
            D3D11_SDK_VERSION,
            &sd,
            &m_pSwapChain,
            &m_pd3dDevice,
            &featureLevel,
            &m_pd3dDeviceContext
        );
    }

    if (FAILED(res))
    {
        std::wcerr << L"Error: Failed to create DirectX 11 device and swap chain. HRESULT: 0x" << std::hex << res << std::dec << std::endl;

        switch (res)
        {
        case DXGI_ERROR_INVALID_CALL:
            std::wcerr << L"  - Invalid parameters passed to D3D11CreateDeviceAndSwapChain" << std::endl;
            break;
        case DXGI_ERROR_SDK_COMPONENT_MISSING:
            std::wcerr << L"  - DirectX SDK component is missing" << std::endl;
            break;
        case E_OUTOFMEMORY:
            std::wcerr << L"  - Insufficient memory to create DirectX device" << std::endl;
            break;
        case E_INVALIDARG:
            std::wcerr << L"  - Invalid argument provided to DirectX" << std::endl;
            break;
        default:
            std::wcerr << L"  - Unknown DirectX error occurred" << std::endl;
            break;
        }
        return false;
    }

    if (!m_pd3dDevice)
    {
        std::wcerr << L"Error: DirectX device creation succeeded but device pointer is null" << std::endl;
        return false;
    }

    if (!m_pd3dDeviceContext)
    {
        std::wcerr << L"Error: DirectX device creation succeeded but device context pointer is null" << std::endl;
        return false;
    }

    if (!m_pSwapChain)
    {
        std::wcerr << L"Error: DirectX device creation succeeded but swap chain pointer is null" << std::endl;
        return false;
    }

    const wchar_t* featureLevelName = L"Unknown";
    switch (featureLevel)
    {
    case D3D_FEATURE_LEVEL_11_1: featureLevelName = L"11.1"; break;
    case D3D_FEATURE_LEVEL_11_0: featureLevelName = L"11.0"; break;
    case D3D_FEATURE_LEVEL_10_1: featureLevelName = L"10.1"; break;
    case D3D_FEATURE_LEVEL_10_0: featureLevelName = L"10.0"; break;
    case D3D_FEATURE_LEVEL_9_3:  featureLevelName = L"9.3";  break;
    case D3D_FEATURE_LEVEL_9_2:  featureLevelName = L"9.2";  break;
    case D3D_FEATURE_LEVEL_9_1:  featureLevelName = L"9.1";  break;
    }

    std::wcout << L"Successfully created DirectX 11 device with feature level " << featureLevelName << std::endl;

    CreateRenderTarget();
    return true;
}

void MyImGui::CleanupDeviceD3D()
{
    CleanupRenderTarget();

    if (m_pSwapChain) { m_pSwapChain->Release(); m_pSwapChain = nullptr; }
    if (m_pd3dDeviceContext) { m_pd3dDeviceContext->Release(); m_pd3dDeviceContext = nullptr; }
    if (m_pd3dDevice) { m_pd3dDevice->Release(); m_pd3dDevice = nullptr; }
}

void MyImGui::CreateRenderTarget()
{
    if (!m_pSwapChain)
    {
        std::wcerr << L"Error: Cannot create render target - swap chain is null" << std::endl;
        return;
    }

    if (!m_pd3dDevice)
    {
        std::wcerr << L"Error: Cannot create render target - D3D device is null" << std::endl;
        return;
    }

    ID3D11Texture2D* pBackBuffer = nullptr;
    HRESULT hr = m_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));

    if (FAILED(hr))
    {
        std::wcerr << L"Error: Failed to get back buffer from swap chain. HRESULT: 0x" << std::hex << hr << std::dec << std::endl;
        return;
    }

    if (!pBackBuffer)
    {
        std::wcerr << L"Error: Back buffer pointer is null after successful GetBuffer call" << std::endl;
        return;
    }

    hr = m_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &m_mainRenderTargetView);

    if (FAILED(hr))
    {
        std::wcerr << L"Error: Failed to create render target view. HRESULT: 0x" << std::hex << hr << std::dec << std::endl;
        pBackBuffer->Release();
        return;
    }

    if (!m_mainRenderTargetView)
    {
        std::wcerr << L"Error: Render target view is null after successful creation" << std::endl;
        pBackBuffer->Release();
        return;
    }

    pBackBuffer->Release();

    std::wcout << L"Successfully created render target view" << std::endl;
}

void MyImGui::CleanupRenderTarget()
{
    if (m_mainRenderTargetView) { m_mainRenderTargetView->Release(); m_mainRenderTargetView = nullptr; }
}

LRESULT WINAPI MyImGui::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_NCCREATE)
    {
        LPCREATESTRUCT cs = (LPCREATESTRUCT)lParam;
        ::SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
    }

    MyImGui* imgui = (MyImGui*)::GetWindowLongPtrW(hWnd, GWLP_USERDATA);

    if (imgui && ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;

        if (imgui) {
            imgui->m_ResizeWidth = (UINT)LOWORD(lParam);
            imgui->m_ResizeHeight = (UINT)HIWORD(lParam);
        }
        return 0;

    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;

    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }

    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}