/**
 * @file MyImGui.cpp
 * @brief Implementation of the MyImGui class for Windows external window ImGui integration
 * 
 * This file contains the complete implementation of a Windows application framework
 * that integrates ImGui with DirectX 11 rendering. The implementation demonstrates
 * best practices for:
 * - Window creation and management
 * - DirectX 11 initialization and resource management
 * - ImGui setup with proper DPI scaling
 * - Message handling and input processing
 * - Resource cleanup and error handling
 */

#include "MyImGui.h"

#pragma comment(lib, "d3d11.lib")

// Forward declaration of ImGui's Win32 message handler
// This function processes ImGui-specific input events (mouse, keyboard, etc.)
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

bool MyImGui::Init(HWND hWnd, const wchar_t* windowTitle, int width, int height, int flag, float FontSize)
{
    // =============================================================================
    // INPUT VALIDATION
    // =============================================================================
    
    // Validate input parameters
    if (windowTitle == nullptr) {
        std::cerr << "Error: Window title cannot be null" << std::endl;
        return false;
    }
    
    if (width <= 0 || height <= 0) {
        std::cerr << "Error: Window dimensions must be positive (width: " << width << ", height: " << height << ")" << std::endl;
        return false;
    }
    
    if (width > 7680 || height > 4320) {  // Reasonable maximum for 8K displays
        std::cerr << "Warning: Very large window dimensions (width: " << width << ", height: " << height << ")" << std::endl;
    }
    
    if (FontSize <= 0.0f || FontSize > 72.0f) {
        std::cerr << "Warning: Unusual font size (" << FontSize << "), using default 18.0f" << std::endl;
        FontSize = 18.0f;
    }

    // =============================================================================
    // STEP 1: WINDOW CREATION OR ATTACHMENT
    // =============================================================================
    
    // Check if we need to create a new window or use an existing one
    if (hWnd == nullptr)
    {
        // Enable DPI awareness for proper scaling on high-DPI displays
        // This ensures the application looks crisp on 4K monitors and similar
        ImGui_ImplWin32_EnableDpiAwareness();
        // Note: This function returns void, so we can't check for success/failure
        
        // Get the DPI scale factor for the primary monitor
        // This will be used to scale window dimensions appropriately
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

        // Register a window class for our application
        // CS_CLASSDC: Allocates a unique device context for each window in the class
        m_wc = { 
            sizeof(m_wc),           // cbSize: Size of this structure
            CS_CLASSDC,             // style: Class styles
            WndProc,                // lpfnWndProc: Pointer to window procedure
            0L,                     // cbClsExtra: Extra bytes for class
            0L,                     // cbWndExtra: Extra bytes for window instance
            GetModuleHandle(nullptr), // hInstance: Handle to module instance
            nullptr,                // hIcon: Handle to class icon
            nullptr,                // hCursor: Handle to class cursor
            nullptr,                // hbrBackground: Handle to background brush
            nullptr,                // lpszMenuName: Menu resource name
            L"ImGui Class",         // lpszClassName: Class name
            nullptr                 // hIconSm: Handle to small icon
        };
        
        if (!::RegisterClassExW(&m_wc)) {
            DWORD error = GetLastError();
            if (error != ERROR_CLASS_ALREADY_EXISTS) {
                std::cerr << "Error: Failed to register window class. Error code: " << error << std::endl;
                return false;
            }
        }

        // Use popup window style for a borderless window
        // WS_POPUP creates a window without title bar, borders, or system menu
        DWORD windowStyle = WS_POPUP;
        DWORD exWindowStyle = WS_EX_APPWINDOW;

        // Get screen dimensions for fullscreen
        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);

        // Calculate window position and size with DPI scaling
        int x = 0;  // X position (top-left corner)
        int y = 0;  // Y position (top-left corner)
        int w = screenWidth;   // Scaled width
        int h = screenHeight;  // Scaled height

        // Validate scaled dimensions
        if (w <= 0 || h <= 0) {
            std::cerr << "Error: Invalid scaled window dimensions (w: " << w << ", h: " << h << ")" << std::endl;
            return false;
        }

        // Create the application window
        m_hwnd = ::CreateWindowExW(
            exWindowStyle,          // Extended window style
            m_wc.lpszClassName,     // Class name
            windowTitle,            // Window title
            windowStyle,            // Window style
            x, y, w, h,            // Position and size
            nullptr,                // Parent window handle
            nullptr,                // Menu handle
            m_wc.hInstance,         // Instance handle
            this                    // Additional application data (pointer to this instance)
        );
        
        // Remove transparency - window should be fully opaque
        // ::SetLayeredWindowAttributes(m_hwnd, 0, 128, LWA_ALPHA);
        
        // Validate window creation
        if (m_hwnd == nullptr) {
            DWORD error = GetLastError();
            std::cerr << "Error: Failed to create window. Error code: " << error << std::endl;
            ::UnregisterClassW(m_wc.lpszClassName, m_wc.hInstance);
            return false;
        }
        
        // Show the window with the specified show state
        if (!::ShowWindow(m_hwnd, flag)) {
            std::cerr << "Warning: ShowWindow returned false" << std::endl;
        }
        
        // Force a paint message to be sent to the window
        if (!::UpdateWindow(m_hwnd)) {
            std::cerr << "Warning: UpdateWindow failed" << std::endl;
        }
    }
    else
    {
        // Validate the provided window handle
        if (!::IsWindow(hWnd)) {
            std::cerr << "Error: Provided HWND is not a valid window handle" << std::endl;
            return false;
        }
        
        // Use the provided existing window handle
        m_hwnd = hWnd;
    }

    // ?????Direct3D
    if (!CreateDeviceD3D())
    {
        // If DirectX initialization fails, clean up and return error
        CleanupDeviceD3D();
        ::UnregisterClassW(m_wc.lpszClassName, m_wc.hInstance);
        return false;
    }

    // =============================================================================
    // STEP 3: IMGUI CONTEXT SETUP
    // =============================================================================
    
    // Verify ImGui version compatibility
    IMGUI_CHECKVERSION();
    
    // Create the main ImGui context
    ImGuiContext* context = ImGui::CreateContext();
    if (!context)
    {
        std::wcerr << L"Error: Failed to create ImGui context" << std::endl;
        CleanupDeviceD3D();
        ::UnregisterClassW(m_wc.lpszClassName, m_wc.hInstance);
        return false;
    }
    
    // Get reference to ImGui IO configuration
    ImGuiIO& io = ImGui::GetIO();
    
    // Enable navigation with keyboard (Tab, Arrow keys, etc.)
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Setup style
    ImGui::StyleColorsDark();
    // ImGui::StyleColorsClassic();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(m_hwnd);
    
    // Get DPI scale factor for the window's monitor
    HMONITOR monitor = ::MonitorFromWindow(m_hwnd, MONITOR_DEFAULTTOPRIMARY);
    if (!monitor)
    {
        std::wcerr << L"Warning: Failed to get monitor handle, using default DPI scaling" << std::endl;
    }
    
    float main_scale = ImGui_ImplWin32_GetDpiScaleForMonitor(monitor);
    
    // Validate DPI scale factor
    if (main_scale <= 0.0f || main_scale > 10.0f)
    {
        std::wcerr << L"Warning: Invalid DPI scale factor (" << main_scale 
                   << L"), using default scale of 1.0" << std::endl;
        main_scale = 1.0f;
    }
    
    // Scale all UI elements (padding, borders, etc.) for high-DPI displays
    ImGui::GetStyle().ScaleAllSizes(main_scale);
    
    // Set font scaling factor for DPI
    ImGui::GetStyle().FontScaleDpi = main_scale;

    // =============================================================================
    // STEP 5: PLATFORM BACKEND INITIALIZATION
    // =============================================================================
    
    // Initialize Win32 platform backend (handles input, clipboard, etc.)
    if (!ImGui_ImplWin32_Init(m_hwnd))
    {
        std::wcerr << L"Error: Failed to initialize ImGui Win32 backend" << std::endl;
        ImGui::DestroyContext();
        CleanupDeviceD3D();
        ::UnregisterClassW(m_wc.lpszClassName, m_wc.hInstance);
        return false;
    }
    
    // Initialize DirectX 11 rendering backend
    if (!ImGui_ImplDX11_Init(m_pd3dDevice, m_pd3dDeviceContext))
    {
        std::wcerr << L"Error: Failed to initialize ImGui DirectX 11 backend" << std::endl;
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        CleanupDeviceD3D();
        ::UnregisterClassW(m_wc.lpszClassName, m_wc.hInstance);
        return false;
    }

    // =============================================================================
    // STEP 6: FONT CONFIGURATION
    // =============================================================================
    
    // Disable ImGui's default .ini file saving (we handle settings manually)
    io.IniFilename = nullptr;

    // Try to load a system font that supports Chinese by default
    // This ensures Chinese texts render correctly without requiring bundled fonts.
    const ImWchar* chinese_ranges = io.Fonts->GetGlyphRangesChineseSimplifiedCommon();
    ImFont* Font = nullptr;

    // Preferred Windows system fonts for Simplified Chinese
    const char* kSystemFontCandidates[] = {
        "C:/Windows/Fonts/msyh.ttc",   // Microsoft YaHei (TTC)
        "C:/Windows/Fonts/msyh.ttf",   // Microsoft YaHei (TTF)
        "C:/Windows/Fonts/msyhbd.ttc", // Microsoft YaHei Bold (TTC)
        "C:/Windows/Fonts/simhei.ttf", // SimHei
        "C:/Windows/Fonts/simsun.ttc", // SimSun (Song)
        "C:/Windows/Fonts/Deng.ttf",   // DengXian
        "C:/Windows/Fonts/Dengb.ttf"   // DengXian Bold
    };

    // Attempt to load the first available system font
    for (const char* path : kSystemFontCandidates)
    {
        ImFontConfig cfg; // default: FontDataOwnedByAtlas = true
        cfg.FontNo = 0;   // For TTC collections, pick first face
        Font = io.Fonts->AddFontFromFileTTF(path, FontSize, &cfg, chinese_ranges);
        if (Font)
        {
            std::wcout << L"Loaded system font for Chinese: " << path << std::endl;
            break;
        }
    }

    // No bundled test font fallback: simplify to default font
    // 如果系统字体均加载失败，则退回到 ImGui 默认字体
    // 同时避免依赖任何测试用内嵌字体数据

    // Final fallback: default ImGui font
    if (!Font)
    {
        Font = io.Fonts->AddFontDefault();
        std::wcerr << L"Warning: System fonts failed; using ImGui default font." << std::endl;
    }

    // 设置默认字体并重建 DX11 设备对象以应用新字体贴图
    if (io.Fonts->Fonts.Size > 0)
        io.FontDefault = io.Fonts->Fonts[0];
    ImGui_ImplDX11_InvalidateDeviceObjects();
    ImGui_ImplDX11_CreateDeviceObjects();

    return true;
}

void MyImGui::Run(DrawFunction drawFunc)
{
    // =============================================================================
    // PARAMETER VALIDATION
    // =============================================================================
    
    // Validate draw function parameter
    if (!drawFunc)
    {
        std::wcerr << L"Error: Draw function parameter is null" << std::endl;
        return;
    }
    
    // Validate that initialization was successful
    if (!m_hwnd || !m_pd3dDevice || !m_pd3dDeviceContext || !m_pSwapChain)
    {
        std::wcerr << L"Error: MyImGui not properly initialized. Call Init() first." << std::endl;
        return;
    }
    
    std::wcout << L"Starting main rendering loop..." << std::endl;

    // =============================================================================
    // MAIN RENDERING LOOP
    // =============================================================================
    
    bool done = false;
    while (!done)
    {
        // =============================================================================
        // STEP 1: WINDOWS MESSAGE PROCESSING
        // =============================================================================
        
        // Process all pending Windows messages (input events, window events, etc.)
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            // Translate virtual-key messages into character messages
            ::TranslateMessage(&msg);
            
            // Dispatch message to window procedure (WndProc)
            ::DispatchMessage(&msg);
            
            // Check for application quit message
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done) break;

        // =============================================================================
        // STEP 2: HANDLE WINDOW OCCLUSION
        // =============================================================================
        
        // Handle swap chain occlusion (window minimized or hidden)
        // If occluded, try to present and sleep briefly to avoid busy waiting
        if (m_SwapChainOccluded && m_pSwapChain->Present(0, 0) == DXGI_STATUS_OCCLUDED)
        {
            ::Sleep(10);  // Sleep 10ms to reduce CPU usage when minimized
            continue;
        }
        m_SwapChainOccluded = false;

        // =============================================================================
        // STEP 3: HANDLE WINDOW RESIZE
        // =============================================================================
        
        // Check if window has been resized and needs buffer recreation
        if (m_ResizeWidth != 0 && m_ResizeHeight != 0)
        {
            // Clean up old render target
            CleanupRenderTarget();
            
            // Resize swap chain buffers to new dimensions
            m_pSwapChain->ResizeBuffers(0, m_ResizeWidth, m_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            
            // Reset resize dimensions
            m_ResizeWidth = m_ResizeHeight = 0;
            
            // Create new render target with new size
            CreateRenderTarget();
        }

        // =============================================================================
        // STEP 4: IMGUI FRAME SETUP
        // =============================================================================
        
        // Start new ImGui frame for DirectX 11 backend
        ImGui_ImplDX11_NewFrame();
        
        // Start new ImGui frame for Win32 backend (processes input)
        ImGui_ImplWin32_NewFrame();
        
        // Begin new ImGui frame (prepares for drawing commands)
        ImGui::NewFrame();

        // =============================================================================
        // STEP 5: USER DRAWING FUNCTION
        // =============================================================================
        
        // Call user-provided drawing function
        // This is where all ImGui windows, widgets, and UI elements are defined
        drawFunc();

        // =============================================================================
        // STEP 6: RENDERING AND PRESENTATION
        // =============================================================================
        
        // Finalize ImGui frame and prepare draw data
        ImGui::Render();
        
        // Set clear color (black background)
        const float clear_color[4] = { 0.0f, 0.0f, 0.0f, 1.00f };
        
        // Set render target for DirectX 11
        m_pd3dDeviceContext->OMSetRenderTargets(1, &m_mainRenderTargetView, nullptr);
        
        // Clear the render target with the specified color
        m_pd3dDeviceContext->ClearRenderTargetView(m_mainRenderTargetView, clear_color);
        
        // Render ImGui draw data to DirectX 11
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        // Present the rendered frame to the screen
        // First parameter (1) enables VSync for smooth animation
        // Second parameter (0) is for additional present flags
        HRESULT hr = m_pSwapChain->Present(1, 0);
        
        // Check if swap chain became occluded during present
        m_SwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
    }
}

void MyImGui::Shutdown()
{
    // =============================================================================
    // CLEANUP AND SHUTDOWN SEQUENCE
    // =============================================================================
    
    // Shutdown ImGui backends in reverse order of initialization
    ImGui_ImplDX11_Shutdown();      // Cleanup DirectX 11 backend
    ImGui_ImplWin32_Shutdown();     // Cleanup Win32 backend
    ImGui::DestroyContext();        // Destroy ImGui context and free memory

    // Cleanup DirectX 11 resources
    CleanupDeviceD3D();

    // Destroy window and unregister window class
    if (m_hwnd)
        ::DestroyWindow(m_hwnd);
    ::UnregisterClassW(m_wc.lpszClassName, m_wc.hInstance);
}

bool MyImGui::CreateDeviceD3D()
{
    // =============================================================================
    // DIRECTX 11 DEVICE AND SWAP CHAIN CREATION
    // =============================================================================
    
    // Validate window handle before proceeding
    if (!m_hwnd || !::IsWindow(m_hwnd))
    {
        std::wcerr << L"Error: Invalid window handle for DirectX device creation" << std::endl;
        return false;
    }
    
    // Configure swap chain descriptor
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;                                    // Double buffering for smooth rendering
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;   // 32-bit RGBA format
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;     // Use as render target
    sd.OutputWindow = m_hwnd;                             // Target window handle
    sd.SampleDesc.Count = 1;                              // No multisampling
    sd.Windowed = TRUE;                                   // Windowed mode (not fullscreen)
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;            // Discard buffer contents after present

    // Set device creation flags
    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;      // Enable debug layer in debug builds
#endif

    // Define supported feature levels (DirectX 11.0 and 10.0)
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };

    // =============================================================================
    // ATTEMPT HARDWARE ACCELERATION FIRST
    // =============================================================================
    
    std::wcout << L"Attempting to create DirectX 11 device with hardware acceleration..." << std::endl;
    
    // Try to create D3D11 device with hardware acceleration
    HRESULT res = D3D11CreateDeviceAndSwapChain(
        nullptr,                    // Use default adapter
        D3D_DRIVER_TYPE_HARDWARE,   // Hardware acceleration
        nullptr,                    // No software module
        createDeviceFlags,          // Creation flags
        featureLevelArray, 2,       // Feature levels array
        D3D11_SDK_VERSION,          // SDK version
        &sd,                        // Swap chain descriptor
        &m_pSwapChain,             // Output: swap chain
        &m_pd3dDevice,             // Output: D3D11 device
        &featureLevel,             // Output: selected feature level
        &m_pd3dDeviceContext       // Output: device context
    );

    // =============================================================================
    // FALLBACK TO SOFTWARE RENDERING IF HARDWARE FAILS
    // =============================================================================
    
    // If hardware acceleration is not supported, fall back to WARP (software rendering)
    if (res == DXGI_ERROR_UNSUPPORTED)
    {
        std::wcout << L"Hardware acceleration not supported, falling back to WARP software rendering..." << std::endl;
        
        res = D3D11CreateDeviceAndSwapChain(
            nullptr,                    // Use default adapter
            D3D_DRIVER_TYPE_WARP,      // WARP software renderer
            nullptr,                    // No software module
            createDeviceFlags,          // Creation flags
            featureLevelArray, 2,       // Feature levels array
            D3D11_SDK_VERSION,          // SDK version
            &sd,                        // Swap chain descriptor
            &m_pSwapChain,             // Output: swap chain
            &m_pd3dDevice,             // Output: D3D11 device
            &featureLevel,             // Output: selected feature level
            &m_pd3dDeviceContext       // Output: device context
        );
    }

    // =============================================================================
    // VALIDATE DEVICE CREATION RESULTS
    // =============================================================================
    
    // Check if device creation was successful
    if (FAILED(res))
    {
        std::wcerr << L"Error: Failed to create DirectX 11 device and swap chain. HRESULT: 0x" 
                   << std::hex << res << std::dec << std::endl;
        
        // Provide specific error messages for common failure cases
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
    
    // Validate that all required objects were created successfully
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
    
    // Log successful creation with feature level information
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
    
    std::wcout << L"Successfully created DirectX 11 device with feature level " 
               << featureLevelName << std::endl;

    // Create render target view from swap chain back buffer
    CreateRenderTarget();
    return true;
}

void MyImGui::CleanupDeviceD3D()
{
    // =============================================================================
    // DIRECTX 11 RESOURCE CLEANUP
    // =============================================================================
    
    // Clean up render target view first
    CleanupRenderTarget();
    
    // Release DirectX 11 resources in reverse order of creation
    if (m_pSwapChain) { m_pSwapChain->Release(); m_pSwapChain = nullptr; }
    if (m_pd3dDeviceContext) { m_pd3dDeviceContext->Release(); m_pd3dDeviceContext = nullptr; }
    if (m_pd3dDevice) { m_pd3dDevice->Release(); m_pd3dDevice = nullptr; }
}

void MyImGui::CreateRenderTarget()
{
    // =============================================================================
    // RENDER TARGET VIEW CREATION
    // =============================================================================
    
    // Validate prerequisites
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
    
    // Get back buffer from swap chain
    ID3D11Texture2D* pBackBuffer = nullptr;
    HRESULT hr = m_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    
    if (FAILED(hr))
    {
        std::wcerr << L"Error: Failed to get back buffer from swap chain. HRESULT: 0x" 
                   << std::hex << hr << std::dec << std::endl;
        return;
    }
    
    if (!pBackBuffer)
    {
        std::wcerr << L"Error: Back buffer pointer is null after successful GetBuffer call" << std::endl;
        return;
    }
    
    // Create render target view from back buffer
    hr = m_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &m_mainRenderTargetView);
    
    if (FAILED(hr))
    {
        std::wcerr << L"Error: Failed to create render target view. HRESULT: 0x" 
                   << std::hex << hr << std::dec << std::endl;
        pBackBuffer->Release();
        return;
    }
    
    if (!m_mainRenderTargetView)
    {
        std::wcerr << L"Error: Render target view is null after successful creation" << std::endl;
        pBackBuffer->Release();
        return;
    }
    
    // Release back buffer reference (render target view holds its own reference)
    pBackBuffer->Release();
    
    std::wcout << L"Successfully created render target view" << std::endl;
}

void MyImGui::CleanupRenderTarget()
{
    // =============================================================================
    // RENDER TARGET VIEW CLEANUP
    // =============================================================================
    
    // Release render target view
    if (m_mainRenderTargetView) { m_mainRenderTargetView->Release(); m_mainRenderTargetView = nullptr; }
}

LRESULT WINAPI MyImGui::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // =============================================================================
    // WINDOWS MESSAGE PROCEDURE
    // =============================================================================
    
    // =============================================================================
    // STEP 1: ASSOCIATE MYIMGUI INSTANCE WITH WINDOW
    // =============================================================================
    
    // During window creation, store the MyImGui instance pointer in window data
    if (msg == WM_NCCREATE)
    {
        // Extract creation parameters containing MyImGui instance pointer
        LPCREATESTRUCT cs = (LPCREATESTRUCT)lParam;
        
        // Store instance pointer in window's user data for later retrieval
        ::SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
    }
    
    // Retrieve MyImGui instance from window's user data
    MyImGui* imgui = (MyImGui*)::GetWindowLongPtrW(hWnd, GWLP_USERDATA);

    // =============================================================================
    // STEP 2: IMGUI INPUT PROCESSING
    // =============================================================================
    
    // Let ImGui process the message first for input handling
    // This handles mouse, keyboard, and other input events for ImGui widgets
    if (imgui && ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;  // Message was handled by ImGui

    // =============================================================================
    // STEP 3: CUSTOM MESSAGE HANDLING
    // =============================================================================
    
    // Handle specific Windows messages that affect our application
    switch (msg)
    {
    case WM_SIZE:
        // Handle window resize events
        if (wParam == SIZE_MINIMIZED) 
            return 0;  // Ignore minimize events
        
        if (imgui) {
            // Store new window dimensions for swap chain resize
            imgui->m_ResizeWidth = (UINT)LOWORD(lParam);   // New width
            imgui->m_ResizeHeight = (UINT)HIWORD(lParam);  // New height
        }
        return 0;
        
    case WM_SYSCOMMAND:
        // Handle system commands
        if ((wParam & 0xfff0) == SC_KEYMENU) 
            return 0;  // Disable ALT menu activation
        break;
        
    case WM_DESTROY:
        // Handle window destruction
        ::PostQuitMessage(0);  // Signal application to exit
        return 0;
    }
    
    // =============================================================================
    // STEP 4: DEFAULT MESSAGE PROCESSING
    // =============================================================================
    
    // Let Windows handle any unprocessed messages with default behavior
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

