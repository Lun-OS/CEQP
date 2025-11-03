#include "themes.h"
#include "ImGui/imgui.h"

// 应用主题
void ApplyImGuiTheme(int theme)
{
    switch (theme)
    {
    case 0: ImGui::StyleColorsDark(); break;
    case 1: ImGui::StyleColorsLight(); break;
    case 2: ImGui::StyleColorsClassic(); break;
    case 3: SetStyleCorporateGrey(); break;
    case 4: SetStyleCherry(); break;
    default: ImGui::StyleColorsDark(); break;
    }
    // 统一修正：避免纯黑与颜色键造成透明洞
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;
    for (int i = 0; i < ImGuiCol_COUNT; ++i) {
        // 如果是纯黑，则调整为深灰，避免被颜色键(RGB(0,0,0))剔除
        if (colors[i].x == 0.0f && colors[i].y == 0.0f && colors[i].z == 0.0f) {
            colors[i].x = 0.06f; colors[i].y = 0.06f; colors[i].z = 0.06f;
            if (colors[i].w < 0.20f) colors[i].w = 0.20f;
        }
    }
}

// 参考样式：Corporate Grey（简洁灰蓝）
void SetStyleCorporateGrey()
{
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 4.0f;
    style.FrameRounding = 3.0f;
    style.GrabRounding = 3.0f;
    style.PopupRounding = 3.0f;
    style.TabRounding = 3.0f;
    style.ScrollbarRounding = 3.0f;
    style.WindowTitleAlign = ImVec2(0.5f, 0.5f);

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = ImVec4(0.92f, 0.92f, 0.92f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.55f, 0.55f, 0.55f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.13f, 0.14f, 0.15f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.13f, 0.14f, 0.15f, 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.10f, 0.10f, 0.11f, 0.94f);
    colors[ImGuiCol_Border] = ImVec4(0.25f, 0.25f, 0.27f, 1.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.27f, 0.29f, 0.30f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.20f, 0.40f, 0.50f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.11f, 0.12f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.10f, 0.11f, 0.12f, 0.60f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.16f, 0.17f, 0.18f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.31f, 0.34f, 0.36f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.41f, 0.44f, 0.46f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.51f, 0.55f, 0.58f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.40f, 0.65f, 0.90f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.30f, 0.55f, 0.80f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.20f, 0.40f, 0.50f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.30f, 0.55f, 0.80f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.15f, 0.35f, 0.55f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.20f, 0.40f, 0.50f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.30f, 0.55f, 0.80f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.15f, 0.35f, 0.55f, 1.00f);
    colors[ImGuiCol_Separator] = ImVec4(0.25f, 0.25f, 0.27f, 1.00f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.41f, 0.44f, 0.46f, 1.00f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.51f, 0.55f, 0.58f, 1.00f);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.31f, 0.34f, 0.36f, 1.00f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.41f, 0.44f, 0.46f, 1.00f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.51f, 0.55f, 0.58f, 1.00f);
    colors[ImGuiCol_Tab] = ImVec4(0.20f, 0.40f, 0.50f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.30f, 0.55f, 0.80f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.25f, 0.50f, 0.70f, 1.00f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.20f, 0.23f, 0.25f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.25f, 0.50f, 0.70f, 1.00f);

    // 额外补齐，避免使用纯黑导致颜色键透明
    colors[ImGuiCol_TextSelectedBg]      = ImVec4(0.30f, 0.55f, 0.80f, 0.35f);
    colors[ImGuiCol_DragDropTarget]      = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
    colors[ImGuiCol_NavHighlight]        = ImVec4(0.30f, 0.55f, 0.80f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight]= ImVec4(0.80f, 0.80f, 0.80f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg]   = ImVec4(0.10f, 0.10f, 0.10f, 0.60f);
    colors[ImGuiCol_ModalWindowDimBg]    = ImVec4(0.10f, 0.10f, 0.10f, 0.35f);
    colors[ImGuiCol_TableHeaderBg]       = ImVec4(0.19f, 0.21f, 0.23f, 1.00f);
    colors[ImGuiCol_TableBorderStrong]   = ImVec4(0.32f, 0.35f, 0.38f, 1.00f);
    colors[ImGuiCol_TableBorderLight]    = ImVec4(0.24f, 0.26f, 0.28f, 1.00f);
    colors[ImGuiCol_TableRowBg]          = ImVec4(0.15f, 0.16f, 0.17f, 1.00f);
    colors[ImGuiCol_TableRowBgAlt]       = ImVec4(0.17f, 0.18f, 0.19f, 1.00f);
}

// 参考样式：Cherry（红黑主题）
void SetStyleCherry()
{
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 5.3f;
    style.FrameRounding = 2.3f;
    style.GrabRounding = 2.3f;
    style.ScrollbarRounding = 2.3f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.09f, 0.09f, 0.09f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.09f, 0.09f, 0.09f, 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
    colors[ImGuiCol_Border] = ImVec4(0.40f, 0.40f, 0.44f, 1.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.70f, 0.21f, 0.21f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.90f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.05f, 0.05f, 0.05f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    // 避免纯黑与颜色键冲突，使用深灰
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.08f, 0.08f, 0.08f, 0.60f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.31f, 0.11f, 0.11f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.44f, 0.16f, 0.16f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.62f, 0.21f, 0.21f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.90f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.90f, 0.40f, 0.40f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.70f, 0.21f, 0.21f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.90f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.62f, 0.21f, 0.21f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.70f, 0.21f, 0.21f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.90f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.62f, 0.21f, 0.21f, 1.00f);
    colors[ImGuiCol_Separator] = ImVec4(0.40f, 0.40f, 0.44f, 1.00f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.75f, 0.22f, 0.22f, 1.00f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.90f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.31f, 0.11f, 0.11f, 1.00f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.44f, 0.16f, 0.16f, 1.00f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.62f, 0.21f, 0.21f, 1.00f);
    colors[ImGuiCol_Tab] = ImVec4(0.70f, 0.21f, 0.21f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.90f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.62f, 0.21f, 0.21f, 1.00f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);

    // 额外补齐，避免使用纯黑导致颜色键透明
    colors[ImGuiCol_TextSelectedBg]      = ImVec4(0.90f, 0.30f, 0.30f, 0.35f);
    colors[ImGuiCol_DragDropTarget]      = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
    colors[ImGuiCol_NavHighlight]        = ImVec4(0.90f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight]= ImVec4(0.85f, 0.85f, 0.85f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg]   = ImVec4(0.12f, 0.12f, 0.12f, 0.70f);
    colors[ImGuiCol_ModalWindowDimBg]    = ImVec4(0.10f, 0.10f, 0.10f, 0.35f);
    colors[ImGuiCol_TableHeaderBg]       = ImVec4(0.19f, 0.21f, 0.23f, 1.00f);
    colors[ImGuiCol_TableBorderStrong]   = ImVec4(0.32f, 0.35f, 0.38f, 1.00f);
    colors[ImGuiCol_TableBorderLight]    = ImVec4(0.24f, 0.26f, 0.28f, 1.00f);
    colors[ImGuiCol_TableRowBg]          = ImVec4(0.15f, 0.16f, 0.17f, 1.00f);
    colors[ImGuiCol_TableRowBgAlt]       = ImVec4(0.17f, 0.18f, 0.19f, 1.00f);
}