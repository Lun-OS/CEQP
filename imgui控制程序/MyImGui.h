#pragma once

// Standard library includes
#include <functional>
#include <iostream>

// Windows API includes
#include <windows.h>
#include <tchar.h>

// DirectX 11 includes for hardware-accelerated rendering
#include <d3d11.h>
#include <dxgi.h>

// ImGui core and platform-specific implementation includes
#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_win32.h"
#include "ImGui/imgui_impl_dx11.h"


/**
 * @brief Callback function type for custom drawing operations
 * 
 * This function type is used to define user-provided drawing functions
 * that will be called during each frame of the rendering loop.
 */
using DrawFunction = std::function<void()>;

/**
 * @class MyImGui
 * @brief A comprehensive wrapper class for ImGui integration with Windows and DirectX 11
 * 
 * This class provides a complete solution for creating external Windows applications
 * with ImGui integration. It handles:
 * - Window creation and management
 * - DirectX 11 device initialization and cleanup
 * - ImGui context setup and rendering
 * - Input event handling
 * - DPI awareness and scaling
 * - Font loading with Chinese character support
 * 
 * Key Features:
 * - Can work with existing windows (by passing HWND) or create new ones
 * - Automatic DPI scaling for high-DPI displays
 * - Hardware-accelerated rendering via DirectX 11
 * - Fallback to WARP software rendering if hardware is unavailable
 * - Proper resource cleanup and error handling
 * - Support for window resizing and minimization
 */
class MyImGui
{
public:
    /**
     * @brief Initialize the ImGui framework with DirectX 11 rendering
     * 
     * This method sets up the complete rendering pipeline including:
     * - Window creation (if hWnd is nullptr) or attachment to existing window
     * - DirectX 11 device and swap chain creation
     * - ImGui context initialization with proper styling and DPI scaling
     * - Font loading with Chinese character support
     * 
     * @param hWnd Optional existing window handle. If nullptr, creates a new window
     * @param windowTitle Title for the window (only used if creating new window)
     * @param width Initial window width in logical pixels
     * @param height Initial window height in logical pixels
     * @param flag Window show state (SW_SHOW, SW_HIDE, etc.)
     * @param FontSize Font size in points for the UI
     * @return true if initialization succeeded, false otherwise
     * 
     * @note If initialization fails, you should call Shutdown() to clean up partial state
     */
    bool Init(HWND hWnd = nullptr, const wchar_t* windowTitle = L"ImGui Window", 
              int width = 1280, int height = 800, int flag = SW_SHOWDEFAULT, float FontSize = 17.0f);

    /**
     * @brief Run the main rendering loop with custom drawing function
     * 
     * This method enters the main application loop and handles:
     * - Windows message processing (input events, window events)
     * - ImGui frame setup and rendering
     * - DirectX 11 rendering and presentation
     * - Window occlusion and resize handling
     * - VSync and frame timing
     * 
     * @param drawFunc User-provided function that contains ImGui drawing commands
     *                 This function is called once per frame after ImGui::NewFrame()
     *                 and before ImGui::Render()
     * 
     * @note This function blocks until the window is closed (WM_QUIT message received)
     * @note The drawFunc should contain all your ImGui UI code (windows, widgets, etc.)
     */
    void Run(DrawFunction drawFunc);

    /**
     * @brief Clean up all resources and shut down the framework
     * 
     * This method performs complete cleanup:
     * - ImGui context destruction
     * - DirectX 11 device and resources cleanup
     * - Window destruction (if created by this class)
     * - Window class unregistration
     * 
     * @note Always call this method before program termination
     * @note Safe to call multiple times or after failed initialization
     */
    void Shutdown();

    /**
     * @brief Get the window handle for this ImGui instance
     * 
     * @return HWND The window handle, or nullptr if not initialized
     * 
     * @note Useful for integrating with other Windows APIs or libraries
     */
    HWND GetHWND() const { return m_hwnd; }

private:
    // =============================================================================
    // PRIVATE MEMBER VARIABLES
    // =============================================================================
    
    /** @brief Handle to the application window */
    HWND m_hwnd = nullptr;
    
    /** @brief DirectX 11 device interface for creating resources */
    ID3D11Device* m_pd3dDevice = nullptr;
    
    /** @brief DirectX 11 device context for rendering operations */
    ID3D11DeviceContext* m_pd3dDeviceContext = nullptr;
    
    /** @brief DXGI swap chain for presenting rendered frames */
    IDXGISwapChain* m_pSwapChain = nullptr;
    
    /** @brief Render target view for the back buffer */
    ID3D11RenderTargetView* m_mainRenderTargetView = nullptr;
    
    /** @brief Flag indicating if the swap chain is occluded (minimized/hidden) */
    bool m_SwapChainOccluded = false;
    
    /** @brief Pending resize dimensions (width and height) */
    UINT m_ResizeWidth = 0, m_ResizeHeight = 0;
    
    /** @brief Window class structure for window registration */
    WNDCLASSEXW m_wc = {};

    // =============================================================================
    // PRIVATE HELPER METHODS
    // =============================================================================
    
    /**
     * @brief Create DirectX 11 device and swap chain
     * 
     * Attempts to create hardware-accelerated D3D11 device first,
     * falls back to WARP software rendering if hardware is unavailable.
     * 
     * @return true if device creation succeeded, false otherwise
     */
    bool CreateDeviceD3D();
    
    /**
     * @brief Clean up DirectX 11 device and related resources
     * 
     * Safely releases all DirectX interfaces and resets pointers to nullptr.
     * Safe to call multiple times.
     */
    void CleanupDeviceD3D();
    
    /**
     * @brief Create render target view from swap chain back buffer
     * 
     * Gets the back buffer from the swap chain and creates a render target view
     * that can be used for rendering operations.
     */
    void CreateRenderTarget();
    
    /**
     * @brief Clean up render target view
     * 
     * Releases the render target view and resets pointer to nullptr.
     * Safe to call multiple times.
     */
    void CleanupRenderTarget();
    
    /**
     * @brief Static window procedure for handling Windows messages
     * 
     * This is the main message handler that processes:
     * - ImGui input events (mouse, keyboard, etc.)
     * - Window resize events
     * - System commands (minimize, close, etc.)
     * - Application termination
     * 
     * @param hWnd Handle to the window
     * @param msg Message identifier
     * @param wParam Additional message information
     * @param lParam Additional message information
     * @return Result of message processing
     * 
     * @note This function retrieves the MyImGui instance from window user data
     *       and forwards ImGui-related messages to ImGui's message handler
     */
    static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
};

