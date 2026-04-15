#include "purrpch.h"
#include "ImGuiTheme.h"

#include "imgui.h"
#include "imgui_internal.h"

namespace Purr {

    static void ApplyImGuiThemeDarkPink()
    {
        ImGui::StyleColorsDark();
        ImVec4* colors = ImGui::GetStyle().Colors;

        const ImVec4 accent = ImVec4(0.78f, 0.40f, 0.62f, 1.00f);

        colors[ImGuiCol_Text]                   = ImVec4(0.96f, 0.91f, 0.93f, 1.00f);
        colors[ImGuiCol_TextDisabled]           = ImVec4(0.52f, 0.44f, 0.50f, 1.00f);
        colors[ImGuiCol_WindowBg]               = ImVec4(0.09f, 0.07f, 0.11f, 0.94f);
        colors[ImGuiCol_ChildBg]                = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_PopupBg]                = ImVec4(0.11f, 0.08f, 0.13f, 0.94f);
        colors[ImGuiCol_Border]                 = ImVec4(0.48f, 0.36f, 0.50f, 0.50f);
        colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_FrameBg]                = ImVec4(0.18f, 0.12f, 0.20f, 0.54f);
        colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.78f, 0.40f, 0.62f, 0.40f);
        colors[ImGuiCol_FrameBgActive]          = ImVec4(0.78f, 0.40f, 0.62f, 0.67f);
        colors[ImGuiCol_TitleBg]                = ImVec4(0.06f, 0.05f, 0.08f, 1.00f);
        colors[ImGuiCol_TitleBgActive]          = ImVec4(0.42f, 0.22f, 0.38f, 1.00f);
        colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.06f, 0.05f, 0.08f, 0.51f);
        colors[ImGuiCol_MenuBarBg]              = ImVec4(0.12f, 0.09f, 0.14f, 1.00f);
        colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.06f, 0.05f, 0.08f, 0.53f);
        colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.45f, 0.32f, 0.45f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.55f, 0.40f, 0.55f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.65f, 0.48f, 0.60f, 1.00f);
        colors[ImGuiCol_CheckMark]              = accent;
        colors[ImGuiCol_SliderGrab]             = ImVec4(0.72f, 0.38f, 0.58f, 1.00f);
        colors[ImGuiCol_SliderGrabActive]       = accent;
        colors[ImGuiCol_Button]                 = ImVec4(0.78f, 0.40f, 0.62f, 0.45f);
        colors[ImGuiCol_ButtonHovered]          = ImVec4(0.85f, 0.48f, 0.68f, 1.00f);
        colors[ImGuiCol_ButtonActive]           = ImVec4(0.65f, 0.30f, 0.52f, 1.00f);
        colors[ImGuiCol_Header]                 = ImVec4(0.78f, 0.40f, 0.62f, 0.35f);
        colors[ImGuiCol_HeaderHovered]          = ImVec4(0.78f, 0.40f, 0.62f, 0.80f);
        colors[ImGuiCol_HeaderActive]           = accent;
        colors[ImGuiCol_Separator]              = colors[ImGuiCol_Border];
        colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.65f, 0.38f, 0.58f, 0.78f);
        colors[ImGuiCol_SeparatorActive]        = ImVec4(0.78f, 0.45f, 0.65f, 1.00f);
        colors[ImGuiCol_ResizeGrip]             = ImVec4(0.78f, 0.40f, 0.62f, 0.25f);
        colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.78f, 0.40f, 0.62f, 0.67f);
        colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.78f, 0.40f, 0.62f, 0.95f);
        colors[ImGuiCol_InputTextCursor]        = colors[ImGuiCol_Text];
        colors[ImGuiCol_PlotLines]              = ImVec4(0.75f, 0.55f, 0.70f, 1.00f);
        colors[ImGuiCol_PlotLinesHovered]       = ImVec4(0.95f, 0.55f, 0.65f, 1.00f);
        colors[ImGuiCol_PlotHistogram]          = ImVec4(0.78f, 0.45f, 0.60f, 1.00f);
        colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(0.92f, 0.55f, 0.72f, 1.00f);
        colors[ImGuiCol_TableHeaderBg]          = ImVec4(0.20f, 0.14f, 0.22f, 1.00f);
        colors[ImGuiCol_TableBorderStrong]      = ImVec4(0.38f, 0.28f, 0.40f, 1.00f);
        colors[ImGuiCol_TableBorderLight]       = ImVec4(0.28f, 0.20f, 0.30f, 1.00f);
        colors[ImGuiCol_TableRowBg]             = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_TableRowBgAlt]          = ImVec4(1.00f, 1.00f, 1.00f, 0.05f);
        colors[ImGuiCol_TextLink]               = colors[ImGuiCol_HeaderActive];
        colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.78f, 0.40f, 0.62f, 0.38f);
        colors[ImGuiCol_TreeLines]              = colors[ImGuiCol_Border];
        colors[ImGuiCol_DragDropTarget]         = ImVec4(0.95f, 0.65f, 0.85f, 0.90f);
        colors[ImGuiCol_DragDropTargetBg]       = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_NavCursor]              = accent;
        colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
        colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.35f, 0.25f, 0.35f, 0.22f);
        colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.08f, 0.05f, 0.10f, 0.55f);

        colors[ImGuiCol_TabHovered]             = colors[ImGuiCol_HeaderHovered];
        colors[ImGuiCol_Tab]                    = ImLerp(colors[ImGuiCol_Header],       colors[ImGuiCol_TitleBgActive], 0.80f);
        colors[ImGuiCol_TabSelected]            = ImLerp(colors[ImGuiCol_HeaderActive], colors[ImGuiCol_TitleBgActive], 0.60f);
        colors[ImGuiCol_TabSelectedOverline]    = colors[ImGuiCol_HeaderActive];
        colors[ImGuiCol_TabDimmed]              = ImLerp(colors[ImGuiCol_Tab],          colors[ImGuiCol_TitleBg], 0.80f);
        colors[ImGuiCol_TabDimmedSelected]      = ImLerp(colors[ImGuiCol_TabSelected],  colors[ImGuiCol_TitleBg], 0.40f);
        colors[ImGuiCol_TabDimmedSelectedOverline] = ImVec4(0.50f, 0.50f, 0.50f, 0.00f);
        {
            const ImVec4& h = colors[ImGuiCol_HeaderActive];
            colors[ImGuiCol_DockingPreview] = ImVec4(h.x, h.y, h.z, h.w * 0.7f);
        }
        colors[ImGuiCol_DockingEmptyBg]         = ImVec4(0.16f, 0.12f, 0.18f, 1.00f);
    }

    void ApplyImGuiTheme(ImGuiThemeKind theme)
    {
        switch (theme) {
        case ImGuiThemeKind::DarkPink:
            ApplyImGuiThemeDarkPink();
            break;
        case ImGuiThemeKind::DarkClassic:
        default:
            ImGui::StyleColorsDark();
            break;
        }
    }

}
