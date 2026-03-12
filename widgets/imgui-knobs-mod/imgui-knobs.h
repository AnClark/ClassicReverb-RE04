#pragma once

#include <cstdlib>
#include <imgui.h>

typedef int ImGuiKnobFlags;

enum ImGuiKnobFlags_ {
    ImGuiKnobFlags_NoTitle = 1 << 0,
    ImGuiKnobFlags_EnableInput = 1 << 1,
    ImGuiKnobFlags_ValueTooltip = 1 << 2,
    ImGuiKnobFlags_DragHorizontal = 1 << 3,
    ImGuiKnobFlags_DragVertical = 1 << 4,
    ImGuiKnobFlags_Logarithmic = 1 << 5,
    ImGuiKnobFlags_AlwaysClamp = 1 << 6,
    ImGuiKnobFlags_TitleBottom = 1 << 7, // Draw title below the knob instead of above
    ImGuiKnobFlags_Pivot       = 1 << 8  // Bilinear mapping: center of travel = pivot_value
};

typedef int ImGuiKnobVariant;

enum ImGuiKnobVariant_ {
    ImGuiKnobVariant_Tick = 1 << 0,
    ImGuiKnobVariant_Dot = 1 << 1,
    ImGuiKnobVariant_Wiper = 1 << 2,
    ImGuiKnobVariant_WiperOnly = 1 << 3,
    ImGuiKnobVariant_WiperDot = 1 << 4,
    ImGuiKnobVariant_Stepped = 1 << 5,
    ImGuiKnobVariant_Space = 1 << 6,
};

namespace ImGuiKnobs_Mod {

    // ---------------------------------------------------------------------------
    // Scale marks: external tick lines + optional text labels drawn outside the
    // knob body, like markings on an audio hardware panel or scientific instrument.
    // ---------------------------------------------------------------------------

    // One mark on the knob scale.
    struct KnobScaleMark {
        float       value;   // parameter value at this mark (within v_min..v_max)
        const char* label;   // text to draw next to the tick; nullptr = tick only
    };

    // Visual style for the scale marks layer (all sizes are relative to knob radius).
    struct KnobScaleMarkStyle {
        float outer_radius = 1.20f;  // tick outer edge distance from knob center
        float tick_length  = 0.10f;  // radial length of each tick line
        float tick_width   = 0.025f; // stroke width of each tick line
        float label_gap    = 0.5f;   // extra gap between tick outer end and label
                                     // center, expressed in units of font size
        float font_size    = 0.0f;   // 0 = 75 % of current ImGui font size
        // (0,0,0,0) sentinel => use ImGuiCol_Text
        ImVec4 tick_color  = ImVec4(0, 0, 0, 0);
        ImVec4 text_color  = ImVec4(0, 0, 0, 0);

        ImFont* custom_font = nullptr; // Optional custom font for labels. If null, uses current ImGui font.
    };

    struct color_set {
        ImColor base;
        ImColor hovered;
        ImColor active;

        color_set(ImColor base, ImColor hovered, ImColor active)
            : base(base), hovered(hovered), active(active) {}

        color_set(ImColor color) {
            base = color;
            hovered = color;
            active = color;
        }
    };

    bool Knob(
            const char *label,
            float *p_value,
            float v_min,
            float v_max,
            float speed = 0,
            const char *format = "%.3f",
            ImGuiKnobVariant variant = ImGuiKnobVariant_Tick,
            float size = 0,
            ImGuiKnobFlags flags = 0,
            int steps = 10,
            float angle_min = -1,
            float angle_max = -1,
            const KnobScaleMark *marks = nullptr,
            int mark_count = 0,
            const KnobScaleMarkStyle *mark_style = nullptr,
            float pivot_value = 0.0f);  // center of travel; only used with ImGuiKnobFlags_Pivot
    bool KnobInt(
            const char *label,
            int *p_value,
            int v_min,
            int v_max,
            float speed = 0,
            const char *format = "%i",
            ImGuiKnobVariant variant = ImGuiKnobVariant_Tick,
            float size = 0,
            ImGuiKnobFlags flags = 0,
            int steps = 10,
            float angle_min = -1,
            float angle_max = -1,
            const KnobScaleMark *marks = nullptr,
            int mark_count = 0,
            const KnobScaleMarkStyle *mark_style = nullptr,
            float pivot_value = 0.0f);

}// namespace ImGuiKnobs
