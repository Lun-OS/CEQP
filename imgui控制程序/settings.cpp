#include "settings.h"
#include "globals.h"
#include "i18n.h"
#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include <windows.h>
#include <dwmapi.h>
#include <sstream>

void ReloadFonts()
{
    // 设备对象安全检查，避免在设备未初始化时重载导致崩溃
    if (!g_pd3dDevice || !g_pd3dDeviceContext)
        return;
    ImGuiIO& io = ImGui::GetIO();
    const ImWchar* chinese_ranges = io.Fonts->GetGlyphRangesChineseFull();
    io.Fonts->Clear();
    ImFont* font = nullptr;
    const float kDefaultUiFontSize = 14.0f;
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
        ImFontConfig cfg; cfg.FontNo = 0; // TTC 取第一个字形
        font = io.Fonts->AddFontFromFileTTF(path, kDefaultUiFontSize, &cfg, chinese_ranges);
        if (font) break;
    }
    if (!font)
    {
        io.Fonts->AddFontDefault();
    }
    if (io.Fonts->Fonts.Size > 0)
        io.FontDefault = io.Fonts->Fonts[0];

    ImGui_ImplDX11_InvalidateDeviceObjects();
    ImGui_ImplDX11_CreateDeviceObjects();
}

void SaveSettings()
{
    std::wstring wpath = I18N::getExeDirW() + L"\\settings.ini";
    std::string content;
    content += "hotkey=" + std::to_string(g_toggleKeyVk) + "\n";
    content += "overlay_visible=" + std::to_string(g_overlayVisible ? 1 : 0) + "\n";
    content += "language=" + std::to_string(I18N::current == I18N::Lang::ZH ? 1 : 0) + "\n";
    content += "theme=" + std::to_string(g_imguiTheme) + "\n";
    content += "exclude_capture=" + std::to_string(g_excludeFromCapture ? 1 : 0) + "\n";
    // 记录主窗口位置（使用虚拟屏坐标）
    if (g_lastUiPos.x >= 0 && g_lastUiPos.y >= 0) {
        content += "ui_pos_x=" + std::to_string((int)g_lastUiPos.x) + "\n";
        content += "ui_pos_y=" + std::to_string((int)g_lastUiPos.y) + "\n";
    }

    // 写 UTF-8 文件
    std::string utf8 = content;
    HANDLE hFile = CreateFileW(wpath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE)
    {
        DWORD written = 0;
        WriteFile(hFile, utf8.data(), (DWORD)utf8.size(), &written, NULL);
        CloseHandle(hFile);
    }
}

void LoadSettings()
{
    std::wstring wpath = I18N::getExeDirW() + L"\\settings.ini";
    std::string content;
    if (!I18N::readFileUtf8(wpath, content)) return;

    auto trim = [](std::string s){
        size_t a = s.find_first_not_of(" \t\r\n");
        size_t b = s.find_last_not_of(" \t\r\n");
        if (a == std::string::npos) return std::string();
        return s.substr(a, b - a + 1);
    };

    std::istringstream iss(content);
    std::string line;
    while (std::getline(iss, line))
    {
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';' || (line.size() >= 2 && line[0] == '/' && line[1] == '/')) continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = trim(line.substr(0, eq));
        std::string val = trim(line.substr(eq + 1));

        if (key == "hotkey") { g_toggleKeyVk = std::stoi(val); }
        else if (key == "overlay_visible") { g_overlayVisible = (std::stoi(val) != 0); }
        else if (key == "language") { g_initialLanguage = std::stoi(val); }
        else if (key == "theme") { g_imguiTheme = std::stoi(val); }
        else if (key == "exclude_capture") { g_excludeFromCapture = (std::stoi(val) != 0); }
        else if (key == "ui_pos_x") { g_savedPosX = std::stoi(val); }
        else if (key == "ui_pos_y") { g_savedPosY = std::stoi(val); }
    }
}

// 运行时应用窗口的禁止截图属性
void ApplyScreenshotExclusion(bool enable)
{
    if (!g_mainHwnd) return;

    // 尝试 DWM 属性：DWMWA_EXCLUDED_FROM_CAPTURE (Windows 10 2004+)
    typedef HRESULT (WINAPI *PFN_DwmSetWindowAttribute)(HWND, DWORD, LPCVOID, DWORD);
    HMODULE hDwm = LoadLibraryW(L"dwmapi.dll");
    if (hDwm)
    {
        PFN_DwmSetWindowAttribute pDwmSetWindowAttribute = (PFN_DwmSetWindowAttribute)GetProcAddress(hDwm, "DwmSetWindowAttribute");
        if (pDwmSetWindowAttribute)
        {
            BOOL val = enable ? TRUE : FALSE;
            DWORD attr = 33; // DWMWA_EXCLUDED_FROM_CAPTURE
#ifdef DWMWA_EXCLUDED_FROM_CAPTURE
            attr = DWMWA_EXCLUDED_FROM_CAPTURE;
#endif
            pDwmSetWindowAttribute(g_mainHwnd, attr, &val, sizeof(val));
        }
        FreeLibrary(hDwm);
    }

    // 再尝试 DisplayAffinity：WDA_EXCLUDEFROMCAPTURE (Windows 10 1903+)，回退到 WDA_MONITOR
#ifndef WDA_MONITOR
#define WDA_MONITOR 0x00000001
#endif
#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011
#endif

    if (enable)
    {
        if (!SetWindowDisplayAffinity(g_mainHwnd, WDA_EXCLUDEFROMCAPTURE))
        {
            SetWindowDisplayAffinity(g_mainHwnd, WDA_MONITOR);
        }
    }
    else
    {
        // 关闭 DisplayAffinity 保护
        SetWindowDisplayAffinity(g_mainHwnd, 0);
        // DWM 属性已在上面设置为 FALSE
        HMODULE hDwm2 = LoadLibraryW(L"dwmapi.dll");
        if (hDwm2)
        {
            PFN_DwmSetWindowAttribute pDwmSetWindowAttribute2 = (PFN_DwmSetWindowAttribute)GetProcAddress(hDwm2, "DwmSetWindowAttribute");
            if (pDwmSetWindowAttribute2)
            {
                BOOL valFalse = FALSE;
                DWORD attr = 33; // DWMWA_EXCLUDED_FROM_CAPTURE
#ifdef DWMWA_EXCLUDED_FROM_CAPTURE
                attr = DWMWA_EXCLUDED_FROM_CAPTURE;
#endif
                pDwmSetWindowAttribute2(g_mainHwnd, attr, &valFalse, sizeof(valFalse));
            }
            FreeLibrary(hDwm2);
        }
    }
}
