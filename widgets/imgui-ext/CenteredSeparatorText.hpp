#pragma once

#include <imgui.h>
#include <imgui_internal.h>

// ----------------------------------------------------------------------------
// ImGuiExt::CenteredSeparatorText
//
// A variant of ImGui::SeparatorText() with the following differences:
//   - The label is horizontally centered within the widget area.
//   - The total widget width is adjustable via the `width` parameter.
//     Pass 0.0f (default) to span the full available content width, which
//     matches the default behaviour of ImGui::SeparatorText().
//
// The separator lines use exactly the same thickness and colour as
// ImGui::SeparatorText() (i.e. style.SeparatorTextBorderSize and
// ImGuiCol_Separator).
//
// Usage example:
//   ImGuiExt::CenteredSeparatorText("Section Title");        // full width
//   ImGuiExt::CenteredSeparatorText("Narrow Title", 160.f);  // fixed width
// ----------------------------------------------------------------------------

namespace ImGuiExt {

inline void CenteredSeparatorText(const char* label, float width = 0.0f)
{
    ImGuiContext&  g      = *GImGui;
    ImGuiWindow*   window = g.CurrentWindow;
    if (window->SkipItems)
        return;

    ImGuiStyle& style = g.Style;

    // Locate the visible portion of the label (strip trailing "##id" etc.)
    const char* label_end = ImGui::FindRenderedTextEnd(label);
    const ImVec2 label_size = ImGui::CalcTextSize(label, label_end, false);

    // ── Geometry ─────────────────────────────────────────────────────────────
    const ImVec2 pos     = window->DC.CursorPos;
    const ImVec2 padding = style.SeparatorTextPadding;

    const float separator_thickness = style.SeparatorTextBorderSize;

    // Total widget width: caller-supplied value or full available width.
    const float avail_w = ImGui::GetContentRegionAvail().x;
    const float total_w = (width > 0.0f) ? width : avail_w;

    // Bounding box height mirrors SeparatorTextEx.
    const float bb_h = ImMax(label_size.y + padding.y * 2.0f, separator_thickness);
    const ImRect bb(pos, ImVec2(pos.x + total_w, pos.y + bb_h));

    // Vertical text baseline – matches SeparatorTextEx logic.
    const float text_baseline_y =
        ImTrunc((bb.GetHeight() - label_size.y) * style.SeparatorTextAlign.y + 0.99999f);

    ImGui::ItemSize(ImVec2(total_w, bb_h), text_baseline_y);
    if (!ImGui::ItemAdd(bb, 0))
        return;

    // ── Drawing ──────────────────────────────────────────────────────────────
    const float seps_y = ImTrunc((bb.Min.y + bb.Max.y) * 0.5f + 0.99999f);
    const ImU32 sep_col = ImGui::GetColorU32(ImGuiCol_Separator);

    if (label_size.x > 0.0f)
    {
        // Center the text within total_w.
        const float label_x  = pos.x + ImTrunc((total_w - label_size.x) * 0.5f);
        const ImVec2 label_pos(label_x, pos.y + text_baseline_y);

        // Left separator segment
        const float sep1_x1 = pos.x;
        const float sep1_x2 = label_pos.x - style.ItemSpacing.x;

        // Right separator segment
        const float sep2_x1 = label_pos.x + label_size.x + style.ItemSpacing.x;
        const float sep2_x2 = pos.x + total_w;

        if (separator_thickness > 0.0f)
        {
            if (sep1_x2 > sep1_x1)
                window->DrawList->AddLine(
                    ImVec2(sep1_x1, seps_y), ImVec2(sep1_x2, seps_y),
                    sep_col, separator_thickness);

            if (sep2_x2 > sep2_x1)
                window->DrawList->AddLine(
                    ImVec2(sep2_x1, seps_y), ImVec2(sep2_x2, seps_y),
                    sep_col, separator_thickness);
        }

        // Render text (with ellipsis clipping to respect the widget boundary).
        ImGui::RenderTextEllipsis(
            window->DrawList,
            label_pos,
            ImVec2(bb.Max.x, bb.Max.y + style.ItemSpacing.y),
            bb.Max.x, bb.Max.x,
            label, label_end, &label_size);
    }
    else
    {
        // No label – draw a plain full-width separator line, identical to the
        // empty-label case in SeparatorTextEx.
        if (separator_thickness > 0.0f)
            window->DrawList->AddLine(
                ImVec2(pos.x, seps_y), ImVec2(pos.x + total_w, seps_y),
                sep_col, separator_thickness);
    }
}

} // namespace ImGuiExt
