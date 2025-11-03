#include "ui.h"
#include "globals.h"
#include "i18n.h"
#include "themes.h"
#include "settings.h"
#include "utils.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <string>
#include <tlhelp32.h>

// LogEntry构造函数实现
LogEntry::LogEntry(const std::string& msg, ImVec4 col) : message(msg), color(col) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    struct tm timeinfo;
    if (localtime_s(&timeinfo, &time_t) == 0) {
        ss << std::put_time(&timeinfo, "%H:%M:%S");
    } else {
        ss << "??:??:??";
    }
    ss << "." << std::setfill('0') << std::setw(3) << ms.count();
    timestamp = ss.str();
}

// AppUI日志方法实现
void AppUI::addLog(const std::string& msg, const ImVec4& col) {
    logs.emplace_back(msg, col);
    if ((int)logs.size() > maxLogEntries) {
        logs.erase(logs.begin());
    }
}

void AppUI::addInfoLog(const std::string& msg) {
    addLog(msg, ImVec4(0.8f, 0.8f, 1.0f, 1.0f));
}

void AppUI::addErrorLog(const std::string& msg) {
    addLog(msg, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
}

void AppUI::addSuccessLog(const std::string& msg) {
    addLog(msg, ImVec4(0.4f, 1.0f, 0.4f, 1.0f));
}

// 检测进程指针大小
int DetectPointerSizeForPid(DWORD pid) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!hProcess) return 0;

    BOOL isWow64 = FALSE;
    if (IsWow64Process(hProcess, &isWow64)) {
        CloseHandle(hProcess);
        return isWow64 ? 4 : 8;
    }

    CloseHandle(hProcess);
    return 0;
}

// 渲染连接面板
void RenderConnectionPanel(AppUI& ui) {
    ImGui::Text("%s", I18N::tr("Server:").c_str());
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120);
    ImGui::InputText("##host", ui.serverHost, IM_ARRAYSIZE(ui.serverHost));
    ImGui::SameLine();
    ImGui::Text(":");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80);  // 增加端口输入框宽度从60到80
    ImGui::InputInt("##port", &ui.serverPort);

    ImGui::SameLine();
    if (ui.isConnected) {
        if (ImGui::Button(I18N::tr("Disconnect").c_str())) {
            ui.client.disconnect();
            ui.isConnected = false;
            ui.addInfoLog(I18N::tr("Disconnected from server").c_str());
        }
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "%s", I18N::tr("Connected").c_str());
    } else {
        if (ImGui::Button(I18N::tr("Connect").c_str())) {
            if (ui.client.connect(ui.serverHost, ui.serverPort)) {
                ui.isConnected = true;
                ui.addSuccessLog(I18N::tr("Connected to ").c_str() + std::string(ui.serverHost) + ":" + std::to_string(ui.serverPort));
                ui.lastHeartbeat = std::chrono::steady_clock::now();
            } else {
                ui.addErrorLog(I18N::tr("Failed to connect: ").c_str() + ui.client.getLastError());
            }
        }
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "%s", I18N::tr("Disconnected").c_str());
    }

    // 心跳检测
    if (ui.isConnected) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - ui.lastHeartbeat).count();
        if (elapsed >= 5) {
            if (ui.client.ping()) {
                ui.lastHeartbeat = now;
            } else {
                ui.isConnected = false;
                ui.addErrorLog(I18N::tr("Connection lost (ping failed)").c_str());
            }
        }
    }
}

// 渲染内存标签页
void RenderMemoryTab(AppUI& ui) {
    ImGui::Text("%s", I18N::tr("Address:").c_str());
    ImGui::InputText("##address", ui.addressInput, IM_ARRAYSIZE(ui.addressInput));
    ImGui::SameLine();
    ImGui::TextDisabled(I18N::tr("(e.g: 0x50000000 or libEngine.dll+013781B0)").c_str());

    ImGui::Text("%s", I18N::tr("Length:").c_str());
    ImGui::InputText("##length", ui.lengthInput, IM_ARRAYSIZE(ui.lengthInput));

    if (ImGui::Button(I18N::tr("Read Memory").c_str(), ImVec2(100, 0))) {
        if (!ui.isConnected) {
            ui.addErrorLog("Not connected to server");
        } else {
            try {
                // 支持模块名+偏移（仅十六进制，且模块名不允许带引号）或纯地址
                uint64_t addr = 0;
                std::string addrStr = ui.addressInput;
                size_t plusPos = addrStr.find('+');
                if (plusPos != std::string::npos) {
                    std::string moduleName = addrStr.substr(0, plusPos);
                    std::string offStr = addrStr.substr(plusPos + 1);

                    // 修剪首尾空白（不移除中间空格，避免破坏真实模块名）
                    {
                        size_t a = moduleName.find_first_not_of(" \t\r\n");
                        size_t b = moduleName.find_last_not_of(" \t\r\n");
                        if (a != std::string::npos) moduleName = moduleName.substr(a, b - a + 1); else moduleName.clear();
                    }
                    offStr.erase(std::remove_if(offStr.begin(), offStr.end(), 
                        [](char c){ return std::isspace((unsigned char)c); }), offStr.end());
                    // 去除包裹的引号（兼容用户误输入）
                    moduleName.erase(std::remove(moduleName.begin(), moduleName.end(), '"'), moduleName.end());
                    moduleName.erase(std::remove(moduleName.begin(), moduleName.end(), '\''), moduleName.end());

                    // 仅支持十六进制偏移，允许可选0x前缀
                    if (offStr.size() >= 2 && offStr[0] == '0' && (offStr[1] == 'x' || offStr[1] == 'X')) {
                        offStr = offStr.substr(2);
                    }
                    if (offStr.empty() || offStr.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos) {
                        throw std::invalid_argument("Offset after '+' must be hexadecimal");
                    }

                    uint64_t modBase = 0;
                    if (!ui.client.getModuleBase(moduleName, modBase)) {
                        ui.addErrorLog(I18N::tr("Failed to get module base: ").c_str() + moduleName);
                        return;
                    }
                    uint64_t moduleOffset = std::stoull(offStr, nullptr, 16);
                    addr = modBase + moduleOffset;
                } else {
                    addr = CETCP::parseAddress(ui.addressInput);
                }
                uint32_t len = std::stoul(ui.lengthInput);
                
                std::vector<uint8_t> buffer;
                if (ui.client.readMemory(addr, len, buffer)) {
                    std::string hexStr = CETCP::bytesToHex(buffer);
                    strncpy_s(ui.valueInput, hexStr.c_str(), sizeof(ui.valueInput) - 1);
                    ui.lastReadData = buffer;
                    ui.addSuccessLog(I18N::tr("Read ").c_str() + std::to_string(buffer.size()) + I18N::tr(" bytes from 0x").c_str() + 
                                   std::to_string(addr));
                } else {
                    ui.addErrorLog(std::string(I18N::tr("Read failed: ").c_str()) + ui.client.getLastError());
                }
            } catch (const std::exception& e) {
                ui.addErrorLog(std::string(I18N::tr("Parse error: ").c_str()) + e.what());
            }
        }
    }

    // 添加值输入框
    ImGui::Text("%s", I18N::tr("Value:").c_str());
    if (ui.writeType == 0) {
        // Hex bytes - 使用多行输入框
        ImGui::InputTextMultiline("##writevalue", ui.valueInput, IM_ARRAYSIZE(ui.valueInput), ImVec2(-1, 100));
    } else {
        // 其他类型 - 使用单行输入框
        ImGui::InputText("##writevalue", ui.valueInput, IM_ARRAYSIZE(ui.valueInput));
    }

    ImGui::SameLine();
    if (ImGui::Button(I18N::tr("Write Memory").c_str(), ImVec2(100, 0))) {
        if (!ui.isConnected) {
            ui.addErrorLog("Not connected to server");
        } else {
            try {
                // 支持模块名+偏移（仅十六进制，且模块名不允许带引号）或纯地址
                uint64_t addr = 0;
                std::string addrStr = ui.addressInput;
                size_t plusPos = addrStr.find('+');
                if (plusPos != std::string::npos) {
                    std::string moduleName = addrStr.substr(0, plusPos);
                    std::string offStr = addrStr.substr(plusPos + 1);

                    // 修剪首尾空白（不移除中间空格，避免破坏真实模块名）
                    {
                        size_t a = moduleName.find_first_not_of(" \t\r\n");
                        size_t b = moduleName.find_last_not_of(" \t\r\n");
                        if (a != std::string::npos) moduleName = moduleName.substr(a, b - a + 1); else moduleName.clear();
                    }
                    offStr.erase(std::remove_if(offStr.begin(), offStr.end(), 
                        [](char c){ return std::isspace((unsigned char)c); }), offStr.end());
                    // 去除包裹的引号（兼容用户误输入）
                    moduleName.erase(std::remove(moduleName.begin(), moduleName.end(), '"'), moduleName.end());
                    moduleName.erase(std::remove(moduleName.begin(), moduleName.end(), '\''), moduleName.end());

                    // 仅支持十六进制偏移，允许可选0x前缀
                    if (offStr.size() >= 2 && offStr[0] == '0' && (offStr[1] == 'x' || offStr[1] == 'X')) {
                        offStr = offStr.substr(2);
                    }
                    if (offStr.empty() || offStr.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos) {
                        throw std::invalid_argument("Offset after '+' must be hexadecimal");
                    }

                    uint64_t modBase = 0;
                    if (!ui.client.getModuleBase(moduleName, modBase)) {
                        ui.addErrorLog(I18N::tr("Failed to get module base: ").c_str() + moduleName);
                        return;
                    }
                    uint64_t moduleOffset = std::stoull(offStr, nullptr, 16);
                    addr = modBase + moduleOffset;
                } else {
                    addr = CETCP::parseAddress(ui.addressInput);
                }
                std::vector<uint8_t> buffer;

                if (ui.writeType == 0) {
                    // Hex bytes
                    buffer = CETCP::hexToBytes(ui.valueInput);
                } else {
                    // 数值类型
                    std::string valueStr = ui.valueInput;
                    valueStr.erase(std::remove_if(valueStr.begin(), valueStr.end(), 
                        [](char c){ return std::isspace((unsigned char)c); }), valueStr.end());

                    switch (ui.writeType) {
                        case 1: { // u8
                            uint8_t val = (uint8_t)std::stoul(valueStr, nullptr, ui.writeBase);
                            buffer.push_back(val);
                            break;
                        }
                        case 2: { // u16
                            uint16_t val = (uint16_t)std::stoul(valueStr, nullptr, ui.writeBase);
                            buffer.resize(2);
                            memcpy(buffer.data(), &val, 2);
                            break;
                        }
                        case 3: { // u32
                            uint32_t val = (uint32_t)std::stoul(valueStr, nullptr, ui.writeBase);
                            buffer.resize(4);
                            memcpy(buffer.data(), &val, 4);
                            break;
                        }
                        case 4: { // u64
                            uint64_t val = std::stoull(valueStr, nullptr, ui.writeBase);
                            buffer.resize(8);
                            memcpy(buffer.data(), &val, 8);
                            break;
                        }
                        case 5: { // i8
                            int8_t val = (int8_t)std::stol(valueStr, nullptr, ui.writeBase);
                            buffer.push_back(*(uint8_t*)&val);
                            break;
                        }
                        case 6: { // i16
                            int16_t val = (int16_t)std::stol(valueStr, nullptr, ui.writeBase);
                            buffer.resize(2);
                            memcpy(buffer.data(), &val, 2);
                            break;
                        }
                        case 7: { // i32
                            int32_t val = (int32_t)std::stol(valueStr, nullptr, ui.writeBase);
                            buffer.resize(4);
                            memcpy(buffer.data(), &val, 4);
                            break;
                        }
                        case 8: { // i64
                            int64_t val = std::stoll(valueStr, nullptr, ui.writeBase);
                            buffer.resize(8);
                            memcpy(buffer.data(), &val, 8);
                            break;
                        }
                        case 9: { // f32
                            float val = std::stof(valueStr);
                            buffer.resize(4);
                            memcpy(buffer.data(), &val, 4);
                            break;
                        }
                        case 10: { // f64
                            double val = std::stod(valueStr);
                            buffer.resize(8);
                            memcpy(buffer.data(), &val, 8);
                            break;
                        }
                    }
                }

                if (ui.client.writeMemory(addr, buffer)) {
                    ui.addSuccessLog(I18N::tr("Wrote ").c_str() + std::to_string(buffer.size()) + I18N::tr(" bytes to 0x").c_str() + 
                                   std::to_string(addr));
                } else {
                    ui.addErrorLog(std::string(I18N::tr("Write failed: ").c_str()) + ui.client.getLastError());
                }
            } catch (const std::exception& e) {
                ui.addErrorLog(std::string(I18N::tr("Parse error: ").c_str()) + e.what());
            }
        }
    }

    ImGui::Text("%s", I18N::tr("Write Type:").c_str());
    
    // 使用静态字符串数组避免临时对象问题
    static std::string writeTypeNames[11];
    static bool writeTypeNamesInitialized = false;
    if (!writeTypeNamesInitialized) {
        writeTypeNames[0] = I18N::tr("Hex bytes");
        writeTypeNames[1] = I18N::tr("UInt8");
        writeTypeNames[2] = I18N::tr("UInt16");
        writeTypeNames[3] = I18N::tr("UInt32");
        writeTypeNames[4] = I18N::tr("UInt64");
        writeTypeNames[5] = I18N::tr("Int8");
        writeTypeNames[6] = I18N::tr("Int16");
        writeTypeNames[7] = I18N::tr("Int32");
        writeTypeNames[8] = I18N::tr("Int64");
        writeTypeNames[9] = I18N::tr("Float");
        writeTypeNames[10] = I18N::tr("Double");
        writeTypeNamesInitialized = true;
    }
    
    const char* writeTypes[] = {
        writeTypeNames[0].c_str(), writeTypeNames[1].c_str(), writeTypeNames[2].c_str(),
        writeTypeNames[3].c_str(), writeTypeNames[4].c_str(), writeTypeNames[5].c_str(),
        writeTypeNames[6].c_str(), writeTypeNames[7].c_str(), writeTypeNames[8].c_str(),
        writeTypeNames[9].c_str(), writeTypeNames[10].c_str()
    };
    ImGui::Combo("##writetype", &ui.writeType, writeTypes, IM_ARRAYSIZE(writeTypes));

    if (ui.writeType > 0 && ui.writeType <= 8) {
        ImGui::SameLine();
        ImGui::RadioButton(I18N::tr("Dec").c_str(), &ui.writeBase, 10);
        ImGui::SameLine();
        ImGui::RadioButton(I18N::tr("Hex").c_str(), &ui.writeBase, 16);
    }
}

// 渲染设置标签页
void RenderSettingsTab(AppUI& ui) {
    ImGui::Text("%s", I18N::tr("Language:").c_str());
    const char* languages[] = { "English", "中文" };
    if (ImGui::Combo("##language", &ui.languageSelection, languages, IM_ARRAYSIZE(languages))) {
        I18N::setLang(ui.languageSelection == 0 ? I18N::Lang::EN : I18N::Lang::ZH);
        g_initialLanguage = ui.languageSelection;
        SaveSettings();
    }

    ImGui::Text("%s", I18N::tr("Theme:").c_str());
    
    // 使用静态字符串数组避免临时对象问题
    static std::string themeNames[5];
    static bool themeNamesInitialized = false;
    if (!themeNamesInitialized) {
        themeNames[0] = I18N::tr("Dark");
        themeNames[1] = I18N::tr("Light");
        themeNames[2] = I18N::tr("Cold Purple");
        themeNames[3] = I18N::tr("Corporate Gray");
        themeNames[4] = I18N::tr("Warning Red");
        themeNamesInitialized = true;
    }
    
    const char* themes[] = {
        themeNames[0].c_str(),
        themeNames[1].c_str(),
        themeNames[2].c_str(),
        themeNames[3].c_str(),
        themeNames[4].c_str()
    };
    if (ImGui::Combo("##theme", &ui.themeSelection, themes, IM_ARRAYSIZE(themes))) {
        g_imguiTheme = ui.themeSelection;
        ApplyImGuiTheme(g_imguiTheme);
        SaveSettings();
    }

    ImGui::Separator();
    ImGui::Text("%s", I18N::tr("Hotkey Settings:").c_str());
    
    const char* hotkeys[] = {
        "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12"
    };
    int hotkeyIndex = g_toggleKeyVk - VK_F1;
    if (hotkeyIndex < 0 || hotkeyIndex >= 12) hotkeyIndex = 6; // 默认F7
    
    if (ImGui::Combo(I18N::tr("Toggle Overlay").c_str(), &hotkeyIndex, hotkeys, IM_ARRAYSIZE(hotkeys))) {
        g_toggleKeyVk = VK_F1 + hotkeyIndex;
        if (g_mainHwnd) {
            UnregisterHotKey(g_mainHwnd, 1);
            RegisterHotKey(g_mainHwnd, 1, 0, g_toggleKeyVk);
        }
        SaveSettings();
    }

    // 禁止截图（默认开启）
    bool excludeCapture = g_excludeFromCapture;
    if (ImGui::Checkbox(I18N::tr("Exclude from screen capture").c_str(), &excludeCapture)) {
        g_excludeFromCapture = excludeCapture;
        ApplyScreenshotExclusion(g_excludeFromCapture);
        SaveSettings();
    }

    ImGui::Separator();
    if (ImGui::Button(I18N::tr("About").c_str())) {
        ui.showAbout = true;
    }
    ImGui::SameLine();
    if (ImGui::Button(I18N::tr("Exit").c_str())) {
        PostQuitMessage(0);
    }
}

// 渲染指针链标签页
void RenderPointerChainTab(AppUI& ui) {
    ImGui::Text("%s", I18N::tr("Base Address:").c_str());
    ImGui::InputText("##ptrbase", ui.ptrBaseInput, IM_ARRAYSIZE(ui.ptrBaseInput));
    ImGui::SameLine();
    ImGui::TextDisabled(I18N::tr("(e.g: libEngine.dll+013781B0 or 0x50000000)").c_str());

    ImGui::Text("%s", I18N::tr("Offset Count:").c_str());
    ImGui::SliderInt("##offsetcount", &ui.offsetCount, 1, 10);

    // 确保偏移输入与类型数组大小匹配
    while ((int)ui.offsetInputs.size() < ui.offsetCount) ui.offsetInputs.emplace_back();
    while ((int)ui.offsetBaseTypes.size() < ui.offsetCount) ui.offsetBaseTypes.push_back(16);
    while ((int)ui.offsetInputs.size() > ui.offsetCount) ui.offsetInputs.pop_back();
    while ((int)ui.offsetBaseTypes.size() > ui.offsetCount) ui.offsetBaseTypes.pop_back();

    ImGui::Text("%s", I18N::tr("Offset Chain:").c_str());
    for (int i = 0; i < ui.offsetCount; i++) {
        ImGui::PushID(i);
        std::string label = "Offset " + std::to_string(i);
        ImGui::Text("%s:", label.c_str());
        ImGui::SameLine();
        ImGui::SetNextItemWidth(200);
        ImGui::InputText("##offset", ui.offsetInputs[i].data(), ui.offsetInputs[i].size());
        ImGui::SameLine();
        ImGui::RadioButton("Dec", &ui.offsetBaseTypes[i], 10);
        ImGui::SameLine();
        ImGui::RadioButton("Hex", &ui.offsetBaseTypes[i], 16);
        ImGui::PopID();
    }

    ImGui::Text("%s", I18N::tr("Data Length:").c_str());
    ImGui::InputText("##ptrlength", ui.ptrLengthInput, IM_ARRAYSIZE(ui.ptrLengthInput));

    // 指针大小选择
    ImGui::Text("%s", I18N::tr("Pointer Size:").c_str());
    ImGui::SameLine();
    ImGui::RadioButton(I18N::tr("Auto").c_str(), &ui.pointerSizeMode, 0);
    ImGui::SameLine();
    ImGui::RadioButton(I18N::tr("4-byte").c_str(), &ui.pointerSizeMode, 4);
    ImGui::SameLine();
    ImGui::RadioButton(I18N::tr("8-byte").c_str(), &ui.pointerSizeMode, 8);
    ImGui::SameLine();
    ImGui::Checkbox(I18N::tr("No deref at last step").c_str(), &ui.noFinalDeref);

    if (ImGui::Button(I18N::tr("Read Pointer Chain").c_str(), ImVec2(150, 0))) {
        if (!ui.isConnected) {
            ui.addErrorLog("Not connected to server");
        } else {
            try {
                // === 第一步：解析基址 ===
                uint64_t baseAddr = 0;
                std::string baseStr = ui.ptrBaseInput;
                
                // 去除首尾空格
                size_t start = baseStr.find_first_not_of(" \t\r\n");
                size_t end = baseStr.find_last_not_of(" \t\r\n");
                if (start != std::string::npos && end != std::string::npos) {
                    baseStr = baseStr.substr(start, end - start + 1);
                }
                
                // 判断是否是纯地址（只包含十六进制字符）
                bool isHexAddr = true;
                for (char c : baseStr) {
                    if (!(std::isxdigit((unsigned char)c) || c == 'x' || c == 'X')) {
                        isHexAddr = false;
                        break;
                    }
                }
                
                if (isHexAddr) {
                    // 纯十六进制地址
                    baseAddr = CETCP::parseAddress(baseStr.c_str());
                    ui.addInfoLog(I18N::tr("Base address: 0x").c_str() + CETCP::bytesToHex({
                        (uint8_t)(baseAddr >> 56), (uint8_t)(baseAddr >> 48),
                        (uint8_t)(baseAddr >> 40), (uint8_t)(baseAddr >> 32),
                        (uint8_t)(baseAddr >> 24), (uint8_t)(baseAddr >> 16),
                        (uint8_t)(baseAddr >> 8), (uint8_t)baseAddr
                    }).substr(0, 16));
                } else {
                    // 模块名+偏移格式（模块名不允许引号，偏移仅十六进制）
                    std::string moduleName = baseStr;
                    uint64_t moduleOffset = 0;
                    size_t plusPos = baseStr.find('+');
                    
                    if (plusPos != std::string::npos) {
                        moduleName = baseStr.substr(0, plusPos);
                        std::string offStr = baseStr.substr(plusPos + 1);

                    // 修剪首尾空白（不移除中间空格，避免破坏真实模块名）
                    {
                        size_t a = moduleName.find_first_not_of(" \t\r\n");
                        size_t b = moduleName.find_last_not_of(" \t\r\n");
                        if (a != std::string::npos) moduleName = moduleName.substr(a, b - a + 1); else moduleName.clear();
                    }
                    offStr.erase(std::remove_if(offStr.begin(), offStr.end(), 
                        [](char c){ return std::isspace((unsigned char)c); }), offStr.end());
                    // 去除包裹的引号（兼容用户误输入）
                    moduleName.erase(std::remove(moduleName.begin(), moduleName.end(), '"'), moduleName.end());
                    moduleName.erase(std::remove(moduleName.begin(), moduleName.end(), '\''), moduleName.end());

                        // 仅支持十六进制偏移，允许可选0x前缀
                        if (offStr.size() >= 2 && offStr[0] == '0' && (offStr[1] == 'x' || offStr[1] == 'X')) {
                            offStr = offStr.substr(2);
                        }
                        if (offStr.empty() || offStr.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos) {
                            throw std::invalid_argument("Offset after '+' must be hexadecimal");
                        }
                        moduleOffset = std::stoull(offStr, nullptr, 16);
                    }
                    
                    uint64_t modBase = 0;
                    if (!ui.client.getModuleBase(moduleName, modBase)) {
                        ui.addErrorLog(I18N::tr("Failed to get module base: ").c_str() + moduleName);
                        return;
                    }
                    
                    baseAddr = modBase + moduleOffset;
                    std::stringstream ss;
                    ss << "Module " << moduleName << " base: 0x" << std::hex << std::uppercase << modBase
                       << " + offset: 0x" << moduleOffset << " => 0x" << baseAddr;
                    ui.addInfoLog(ss.str());
                }

                // === 第二步：解析读取长度 ===
                std::string lenStr = ui.ptrLengthInput;
                lenStr.erase(std::remove_if(lenStr.begin(), lenStr.end(), 
                    [](char c){ return std::isspace((unsigned char)c); }), lenStr.end());
                
                uint32_t length = 0;
                if (lenStr.size() >= 2 && lenStr[0] == '0' && (lenStr[1] == 'x' || lenStr[1] == 'X')) {
                    length = std::stoul(lenStr.substr(2), nullptr, 16);
                } else if (lenStr.find_first_not_of("0123456789") != std::string::npos) {
                    length = std::stoul(lenStr, nullptr, 16);
                } else {
                    length = std::stoul(lenStr, nullptr, 10);
                }

                // === 第三步：解析偏移链并显示详细信息 ===
                std::vector<int64_t> offsets;
                std::stringstream debugInfo;
                debugInfo << "Parsing offsets: ";
                
                for (int i = 0; i < ui.offsetCount; i++) {
                    std::string os = ui.offsetInputs[i].data();
                    
                    // 去除空格
                    os.erase(std::remove_if(os.begin(), os.end(), 
                        [](char c){ return std::isspace((unsigned char)c); }), os.end());
                    
                    if (os.empty()) {
                        throw std::invalid_argument("Offset " + std::to_string(i) + " is empty");
                    }
                    
                    int baseSel = ui.offsetBaseTypes[i];
                    int64_t offset = 0;
                    
                    if (baseSel == 16) {
                        // 十六进制模式
                        bool isNegative = (os[0] == '-');
                        std::string core = isNegative ? os.substr(1) : os;
                        
                        // 如果没有0x前缀，添加它
                        if (!(core.size() >= 2 && core[0] == '0' && (core[1] == 'x' || core[1] == 'X'))) {
                            core = "0x" + core;
                        }
                        
                        offset = std::stoll(core, nullptr, 16);
                        if (isNegative) offset = -offset;
                        
                        debugInfo << "[" << i << ":0x" << std::hex << offset << std::dec << "] ";
                    } else {
                        // 十进制模式
                        offset = std::stoll(os, nullptr, 10);
                        debugInfo << "[" << i << ":" << offset << "] ";
                    }
                    
                    offsets.push_back(offset);
                }
                
                ui.addInfoLog(debugInfo.str());

                // === 第四步：执行指针链读取 ===
                std::string dtype;
                int resolvedPtrSize = ui.pointerSizeMode;
                int detectedPtrSize = 0;
                if (resolvedPtrSize == 0) {
                    DWORD targetPid = FindProcessId(L"windows-test.exe");
                    if (targetPid != 0) {
                        detectedPtrSize = DetectPointerSizeForPid(targetPid);
                    }
                    if (detectedPtrSize == 4 || detectedPtrSize == 8) {
                        resolvedPtrSize = detectedPtrSize;
                        ui.addInfoLog(std::string(I18N::tr("Auto pointer-size via process arch: ").c_str()) + (resolvedPtrSize == 4 ? I18N::tr("ptr32 (WOW64)").c_str() : I18N::tr("ptr64").c_str()));
                    } else {
                        // 回退：基于基址宽度的启发式
                        resolvedPtrSize = (baseAddr > 0xFFFFFFFFULL) ? 8 : 4;
                        ui.addInfoLog(std::string(I18N::tr("Auto pointer-size via baseAddr heuristic: ").c_str()) + (resolvedPtrSize == 4 ? I18N::tr("ptr32").c_str() : I18N::tr("ptr64").c_str()));
                    }
                }

                if (resolvedPtrSize == 4) dtype += "ptr32"; else if (resolvedPtrSize == 8) dtype += "ptr64";
                if (!dtype.empty()) dtype += " ";
                dtype += "ce";
                if (ui.noFinalDeref) {
                    dtype += " noderef";
                }

                ui.addInfoLog(I18N::tr("Pointer-chain DTYPE: ").c_str() + (dtype.empty() ? "<auto>" : dtype));

                std::vector<uint8_t> buffer;
                if (ui.client.readPointerChain(baseAddr, offsets, length, buffer, dtype)) {
                    std::stringstream ss;
                    ss << "SUCCESS! Read " << buffer.size() << " bytes from pointer chain:";
                    ui.addSuccessLog(ss.str());
                    
                    // 显示指针链路径
                    std::stringstream pathSS;
                    pathSS << I18N::tr("Path: ").c_str() << "[0x" << std::hex << std::uppercase << baseAddr << "]";
                    for (size_t i = 0; i < offsets.size(); i++) {
                        pathSS << " + 0x" << std::hex << offsets[i];
                    }
                    ui.addInfoLog(pathSS.str());
                    
                    // 显示数据预览
                    std::stringstream dataSS;
                    dataSS << "Data: ";
                    for (size_t i = 0; i < buffer.size() && i < 32; ++i) {
                        dataSS << std::hex << std::setw(2) << std::setfill('0') << (int)buffer[i] << " ";
                    }
                    if (buffer.size() > 32) dataSS << "...";
                    ui.addInfoLog(dataSS.str());
                    
                    // 将数据写入valueInput供查看
                    std::string hexStr = CETCP::bytesToHex(buffer);
                    strncpy_s(ui.valueInput, hexStr.c_str(), sizeof(ui.valueInput) - 1);
                    ui.lastReadData = buffer;
                } else {
                    ui.addErrorLog(I18N::tr("Pointer chain read failed: ").c_str() + ui.client.getLastError());
                }
            } catch (const std::exception& e) {
                ui.addErrorLog(std::string(I18N::tr("ERROR parsing input: ").c_str()) + e.what());
            }
        }
    }

    // 添加指针链写入功能
    ImGui::Separator();
    ImGui::Text("%s", I18N::tr("Value:").c_str());
    if (ui.ptrWriteType == 0) {
        // Hex bytes - 使用多行输入框
        ImGui::InputTextMultiline("##ptrwritevalue", ui.ptrValueInput, IM_ARRAYSIZE(ui.ptrValueInput), ImVec2(-1, 100));
    } else {
        // 其他类型 - 使用单行输入框
        ImGui::InputText("##ptrwritevalue", ui.ptrValueInput, IM_ARRAYSIZE(ui.ptrValueInput));
    }

    ImGui::Text("%s", I18N::tr("Write Type:").c_str());
    
    // 使用静态字符串数组避免临时对象问题
    static std::string ptrWriteTypeNames[11];
    static bool ptrWriteTypeNamesInitialized = false;
    if (!ptrWriteTypeNamesInitialized) {
        ptrWriteTypeNames[0] = I18N::tr("Hex bytes");
        ptrWriteTypeNames[1] = I18N::tr("UInt8");
        ptrWriteTypeNames[2] = I18N::tr("UInt16");
        ptrWriteTypeNames[3] = I18N::tr("UInt32");
        ptrWriteTypeNames[4] = I18N::tr("UInt64");
        ptrWriteTypeNames[5] = I18N::tr("Int8");
        ptrWriteTypeNames[6] = I18N::tr("Int16");
        ptrWriteTypeNames[7] = I18N::tr("Int32");
        ptrWriteTypeNames[8] = I18N::tr("Int64");
        ptrWriteTypeNames[9] = I18N::tr("Float");
        ptrWriteTypeNames[10] = I18N::tr("Double");
        ptrWriteTypeNamesInitialized = true;
    }
    
    const char* ptrWriteTypes[] = {
        ptrWriteTypeNames[0].c_str(), ptrWriteTypeNames[1].c_str(), ptrWriteTypeNames[2].c_str(),
        ptrWriteTypeNames[3].c_str(), ptrWriteTypeNames[4].c_str(), ptrWriteTypeNames[5].c_str(),
        ptrWriteTypeNames[6].c_str(), ptrWriteTypeNames[7].c_str(), ptrWriteTypeNames[8].c_str(),
        ptrWriteTypeNames[9].c_str(), ptrWriteTypeNames[10].c_str()
    };
    ImGui::Combo("##ptrwritetype", &ui.ptrWriteType, ptrWriteTypes, IM_ARRAYSIZE(ptrWriteTypes));

    if (ui.ptrWriteType > 0 && ui.ptrWriteType <= 8) {
        ImGui::SameLine();
        ImGui::RadioButton(I18N::tr("Dec").c_str(), &ui.ptrWriteBase, 10);
        ImGui::SameLine();
        ImGui::RadioButton(I18N::tr("Hex").c_str(), &ui.ptrWriteBase, 16);
    }

    if (ImGui::Button(I18N::tr("Write Pointer Chain").c_str(), ImVec2(150, 0))) {
        if (!ui.isConnected) {
            ui.addErrorLog(I18N::tr("Not connected to server").c_str());
        } else {
            try {
                // 解析基址
                uint64_t baseAddr = 0;
                std::string baseStr = ui.ptrBaseInput;
                if (baseStr.find('+') != std::string::npos) {
                    size_t plusPos = baseStr.find('+');
                    std::string moduleName = baseStr.substr(0, plusPos);
                    std::string offsetStr = baseStr.substr(plusPos + 1);

                    // 修剪首尾空白（不移除中间空格，避免破坏真实模块名）；偏移移除所有空白
                    {
                        size_t a = moduleName.find_first_not_of(" \t\r\n");
                        size_t b = moduleName.find_last_not_of(" \t\r\n");
                        if (a != std::string::npos) moduleName = moduleName.substr(a, b - a + 1); else moduleName.clear();
                    }
                    offsetStr.erase(std::remove_if(offsetStr.begin(), offsetStr.end(), 
                        [](char c){ return std::isspace((unsigned char)c); }), offsetStr.end());

                    // 去除包裹的引号（兼容用户误输入）
                    moduleName.erase(std::remove(moduleName.begin(), moduleName.end(), '"'), moduleName.end());
                    moduleName.erase(std::remove(moduleName.begin(), moduleName.end(), '\''), moduleName.end());

                    // 仅支持十六进制偏移，允许可选0x前缀
                    if (offsetStr.size() >= 2 && offsetStr[0] == '0' && (offsetStr[1] == 'x' || offsetStr[1] == 'X')) {
                        offsetStr = offsetStr.substr(2);
                    }
                    if (offsetStr.empty() || offsetStr.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos) {
                        throw std::invalid_argument("Offset after '+' must be hexadecimal");
                    }

                    uint64_t moduleOffset = std::stoull(offsetStr, nullptr, 16);

                    uint64_t modBase = 0;
                    if (ui.client.getModuleBase(moduleName, modBase)) {
                        baseAddr = modBase + moduleOffset;
                        ui.addInfoLog(I18N::tr("Module ").c_str() + moduleName + I18N::tr(" base: 0x").c_str() + CETCP::bytesToHex({
                            (uint8_t)(modBase >> 56), (uint8_t)(modBase >> 48),
                            (uint8_t)(modBase >> 40), (uint8_t)(modBase >> 32),
                            (uint8_t)(modBase >> 24), (uint8_t)(modBase >> 16),
                            (uint8_t)(modBase >> 8), (uint8_t)modBase
                        }).substr(0, 16));
                    } else {
                        ui.addErrorLog(I18N::tr("Failed to get module base for: ").c_str() + moduleName);
                        return;
                    }
                } else {
                    baseAddr = CETCP::parseAddress(ui.ptrBaseInput);
                }

                // 解析偏移
                std::vector<int64_t> offsets;
                for (int i = 0; i < ui.offsetCount; i++) {
                    std::string offsetStr = ui.offsetInputs[i].data();
                    if (!offsetStr.empty()) {
                        int64_t offset = 0;
                        if (ui.offsetBaseTypes[i] == 16) {
                            offset = std::stoll(offsetStr, nullptr, 16);
                        } else {
                            offset = std::stoll(offsetStr, nullptr, 10);
                        }
                        offsets.push_back(offset);
                    }
                }

                // 准备写入数据
                std::vector<uint8_t> buffer;
                std::string valueStr = ui.ptrValueInput;

                if (ui.ptrWriteType == 0) {
                    // Hex bytes
                    buffer = CETCP::hexToBytes(valueStr);
                } else {
                    // 其他数据类型
                    switch (ui.ptrWriteType) {
                    case 1: { // UInt8
                        uint8_t val = (uint8_t)std::stoul(valueStr, nullptr, ui.ptrWriteBase);
                        buffer.push_back(val);
                        break;
                    }
                    case 2: { // UInt16
                        uint16_t val = (uint16_t)std::stoul(valueStr, nullptr, ui.ptrWriteBase);
                        buffer.resize(2);
                        memcpy(buffer.data(), &val, 2);
                        break;
                    }
                    case 3: { // UInt32
                        uint32_t val = (uint32_t)std::stoul(valueStr, nullptr, ui.ptrWriteBase);
                        buffer.resize(4);
                        memcpy(buffer.data(), &val, 4);
                        break;
                    }
                    case 4: { // UInt64
                        uint64_t val = std::stoull(valueStr, nullptr, ui.ptrWriteBase);
                        buffer.resize(8);
                        memcpy(buffer.data(), &val, 8);
                        break;
                    }
                    case 5: { // Int8
                        int8_t val = (int8_t)std::stol(valueStr, nullptr, ui.ptrWriteBase);
                        buffer.push_back(*(uint8_t*)&val);
                        break;
                    }
                    case 6: { // Int16
                        int16_t val = (int16_t)std::stol(valueStr, nullptr, ui.ptrWriteBase);
                        buffer.resize(2);
                        memcpy(buffer.data(), &val, 2);
                        break;
                    }
                    case 7: { // Int32
                        int32_t val = (int32_t)std::stol(valueStr, nullptr, ui.ptrWriteBase);
                        buffer.resize(4);
                        memcpy(buffer.data(), &val, 4);
                        break;
                    }
                    case 8: { // Int64
                        int64_t val = std::stoll(valueStr, nullptr, ui.ptrWriteBase);
                        buffer.resize(8);
                        memcpy(buffer.data(), &val, 8);
                        break;
                    }
                    case 9: { // Float
                        float val = std::stof(valueStr);
                        buffer.resize(4);
                        memcpy(buffer.data(), &val, 4);
                        break;
                    }
                    case 10: { // Double
                        double val = std::stod(valueStr);
                        buffer.resize(8);
                        memcpy(buffer.data(), &val, 8);
                        break;
                    }
                    }
                }

                // 构建数据类型字符串
                std::string dtype;
                int resolvedPtrSize = ui.pointerSizeMode;
                if (resolvedPtrSize == 0) {
                    DWORD targetPid = FindProcessId(L"windows-test.exe");
                    if (targetPid != 0) {
                        int detectedPtrSize = DetectPointerSizeForPid(targetPid);
                        if (detectedPtrSize == 4 || detectedPtrSize == 8) {
                            resolvedPtrSize = detectedPtrSize;
                        } else {
                            resolvedPtrSize = (baseAddr > 0xFFFFFFFFULL) ? 8 : 4;
                        }
                    } else {
                        resolvedPtrSize = (baseAddr > 0xFFFFFFFFULL) ? 8 : 4;
                    }
                }

                if (resolvedPtrSize == 4) dtype += "ptr32"; else if (resolvedPtrSize == 8) dtype += "ptr64";
                if (!dtype.empty()) dtype += " ";
                dtype += "ce";
                if (ui.noFinalDeref) {
                    dtype += " noderef";
                }

                // 执行写入
                if (ui.client.writePointerChain(baseAddr, offsets, buffer, dtype)) {
                    ui.addSuccessLog(I18N::tr("Successfully wrote ").c_str() + std::to_string(buffer.size()) + I18N::tr(" bytes to pointer chain").c_str());
                    
                    // 显示指针链路径
                    std::stringstream pathSS;
                    pathSS << I18N::tr("Path: ").c_str() << "[0x" << std::hex << std::uppercase << baseAddr << "]";
                    for (size_t i = 0; i < offsets.size(); i++) {
                        pathSS << " + 0x" << std::hex << offsets[i];
                    }
                    ui.addInfoLog(pathSS.str());
                } else {
                    ui.addErrorLog(std::string(I18N::tr("Write failed: ").c_str()) + ui.client.getLastError());
                }
            } catch (const std::exception& e) {
                ui.addErrorLog(std::string(I18N::tr("Parse error: ").c_str()) + e.what());
            }
        }
    }
    
    ImGui::Separator();
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "%s:", I18N::tr("Tips").c_str());
    ImGui::BulletText("%s", I18N::tr("The pointer order in Cheat Engine is from bottom to top, be careful to distinguish it.").c_str());
}

// 渲染共享数据显示区域
void RenderSharedDataDisplay(AppUI& ui) {
    ImGui::Separator();
    // 先显示数据解析区块，再显示十六进制数据区块
    if (!ui.lastReadData.empty()) {
        ImGui::Text("%s", I18N::tr("Data Parse:").c_str());
        ImGui::BeginChild("DataParse", ImVec2(0, 150), true);

        // 以不同格式显示数据
        if (ui.lastReadData.size() >= 1) {
            ImGui::Text("Int8:  %d", (int8_t)ui.lastReadData[0]);
            ImGui::Text("UInt8: %u", ui.lastReadData[0]);
        }
        if (ui.lastReadData.size() >= 4) {
            uint32_t val32 = *(uint32_t*)ui.lastReadData.data();
            ImGui::Text("Int32:  %d", (int32_t)val32);
            ImGui::Text("UInt32: %u", val32);
            ImGui::Text("Float:  %.6f", *(float*)&val32);
        }
        if (ui.lastReadData.size() >= 8) {
            uint64_t val64 = *(uint64_t*)ui.lastReadData.data();
            ImGui::Text("Int64:  %lld", (int64_t)val64);
            ImGui::Text("UInt64: %llu", val64);
            ImGui::Text("Double: %.6f", *(double*)&val64);
        }

        ImGui::EndChild();
    }

    ImGui::Text("%s", I18N::tr("Data (Hex):").c_str());
    ImGui::InputTextMultiline("##hexvalue", ui.valueInput, IM_ARRAYSIZE(ui.valueInput), ImVec2(-1, 100));
}

// 渲染日志标签页
void RenderLogTab(AppUI& ui) {
    if (ImGui::Button(I18N::tr("Clear Log").c_str())) {
        ui.logs.clear();
    }
    ImGui::SameLine();
    ImGui::Checkbox(I18N::tr("Auto Scroll").c_str(), &ui.autoScroll);
    ImGui::SameLine();
    ImGui::Text("%s %d", I18N::tr("Log Count:").c_str(), (int)ui.logs.size());

    ImGui::Separator();
    ImGui::BeginChild("LogArea", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);

    for (const auto& log : ui.logs) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "[%s]", log.timestamp.c_str());
        ImGui::SameLine();
        ImGui::TextColored(log.color, "%s", log.message.c_str());
    }

    if (ui.autoScroll && !ui.logs.empty()) {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();
}

// 渲染主UI
void RenderUI(AppUI& ui) {
    // 首帧应用主题（避免未在 WinMain 初始化时应用）
    static bool s_themeAppliedOnce = false;
    if (!s_themeAppliedOnce) { ApplyImGuiTheme(g_imguiTheme); s_themeAppliedOnce = true; }
    // 初次应用保存位置
    if (!g_initialPosApplied && g_savedPosX >= 0 && g_savedPosY >= 0) {
        ImGui::SetNextWindowPos(ImVec2((float)g_savedPosX, (float)g_savedPosY), ImGuiCond_Always);
        g_initialPosApplied = true;
    }
    ImGui::Begin(I18N::tr("CEQP Control Program").c_str(), nullptr, ImGuiWindowFlags_None);

    // 标题
    ImGui::Text("%s", I18N::tr("CEQP Control Program").c_str());
    // 多开提示：3秒提示信息
    if (g_notifyUntil.time_since_epoch().count() != 0) {
        auto now = std::chrono::steady_clock::now();
        if (now < g_notifyUntil) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "%s", I18N::tr("Program already running").c_str());
        }
    }
    ImGui::Separator();

    // 连接面板
    RenderConnectionPanel(ui);
    ImGui::Separator();

    // 标签页
    bool isLogTabActive = false;
    bool isSettingsTabActive = false;
    if (ImGui::BeginTabBar("MainTabs", ImGuiTabBarFlags_None)) {
        if (ImGui::BeginTabItem(I18N::tr("Memory").c_str())) {
            RenderMemoryTab(ui);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(I18N::tr("Pointer Chain").c_str())) {
            RenderPointerChainTab(ui);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(I18N::tr("Log").c_str())) {
            isLogTabActive = true;
            RenderLogTab(ui);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(I18N::tr("Settings").c_str())) {
            isSettingsTabActive = true;
            RenderSettingsTab(ui);
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    // 在非日志且非设置标签页显示共享数据区域
    if (!isLogTabActive && !isSettingsTabActive) {
        RenderSharedDataDisplay(ui);
    }

    // 记录并约束窗口位置，防止移出可视区域
    ImVec2 pos = ImGui::GetWindowPos();
    ImVec2 size = ImGui::GetWindowSize();
    g_lastUiPos = pos;
    int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    float minX = 0.0f;
    float minY = 0.0f;
    float maxX = (float)vw - size.x;
    float maxY = (float)vh - size.y;
    ImVec2 clamped = ImVec2((std::max)(minX, (std::min)(pos.x, maxX)), (std::max)(minY, (std::min)(pos.y, maxY)));
    if (clamped.x != pos.x || clamped.y != pos.y) {
        ImGui::SetWindowPos(clamped, ImGuiCond_Always);
        g_lastUiPos = clamped;
    }
    ImGui::End();

    // 关于窗口
    if (ui.showAbout) {
        ImGui::Begin(I18N::tr("About").c_str(), &ui.showAbout, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Text("%s", I18N::tr("CEQP Control Program").c_str());
        ImGui::Text("%s", I18N::tr("Version: 1.0").c_str());
        ImGui::Separator();
        ImGui::Text("%s", I18N::tr("Cheat Engine TCP_UDP Plugin Client based on CEQP Protocol").c_str());
        ImGui::Text("%s", I18N::tr("Supports memory read/write and pointer chain parsing").c_str());
        ImGui::Text("%s", I18N::tr("Author: Lun. | GitHub: Lun-OS | QQ: 1596534228 | Open Source License: MIT").c_str());
        ImGui::End();
    }
}
