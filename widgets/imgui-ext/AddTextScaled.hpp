#pragma once

#include <imgui.h>
#include <imgui_internal.h>

namespace ImGuiExt {
    inline void AddTextScaled(ImDrawList* draw_list, ImFont* font, float font_size,
                    ImVec2 pos, ImU32 color, const char* text,
                    float scale_x = 1.0f, float scale_y = 1.0f)
    {
        const bool needs_scale = (scale_x != 1.0f || scale_y != 1.0f);

        // AddText clips characters against the current clip rect before generating
        // vertices. If scale_x < 1, the unscaled text is wider than the final result
        // and trailing glyphs get discarded before we can compress them.
        // Temporarily push a rect wide/tall enough for the full unscaled text.
        if (needs_scale)
        {
            ImVec2 text_size = (font ? font : ImGui::GetFont())
                                   ->CalcTextSizeA(font_size, FLT_MAX, 0.0f, text);
            const ImVec4& cr = draw_list->_ClipRectStack.back();
            draw_list->PushClipRect(
                ImVec2(cr.x, cr.y),
                ImVec2(ImMax(cr.z, pos.x + text_size.x + 1.0f),
                       ImMax(cr.w, pos.y + text_size.y + 1.0f)));
        }

        int vtx_start = draw_list->VtxBuffer.Size;
        draw_list->AddText(font, font_size, pos, color, text);

        if (!needs_scale)
            return;

        draw_list->PopClipRect(); // Restore original clip rect

        for (int i = vtx_start; i < draw_list->VtxBuffer.Size; i++)
        {
            ImDrawVert& v = draw_list->VtxBuffer[i];
            v.pos.x = pos.x + (v.pos.x - pos.x) * scale_x;
            v.pos.y = pos.y + (v.pos.y - pos.y) * scale_y;
        }
    }

    inline void AddTextScaled(const char* text, float scale_x = 1.0f, float scale_y = 1.0f)
    {
        ImDrawList *draw_list = ImGui::GetWindowDrawList();
        ImFont *font = ImGui::GetFont();
        const float font_size = ImGui::GetFontSize();
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        auto color = ImGui::GetColorU32((ImGui::GetStyle().Colors[ImGuiCol_Text]));

        AddTextScaled(draw_list, font, font_size, pos, color, text, scale_x, scale_y);
    }
}
