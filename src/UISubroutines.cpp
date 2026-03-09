#include "UI.h"

#include "CenteredSeparatorText.hpp"
#include "imgui-knobs.h"
#include "../fonts/LiberationSans-Regular.hpp"

ImGuiKnobs_Mod::KnobScaleMarkStyle kScaleMarkStyle = {
    .outer_radius = 1.20f,
    .tick_length  = 0.50f,    
    .font_size    = 12.5f,
};

void ClassicReverbUI::_loadFonts()
{
    // Font sizes:
    // - Section title text:                   14px
    // - Regular text:                         12.5px (e.g. Knob labels)
    // - Larger text size (for down-sampling): 16px (e.g. Knob scale marks, down-sampled to 12.5px for better rendering quality)

    ImGuiIO& io(ImGui::GetIO());

    ImFontConfig fc;
    fc.FontDataOwnedByAtlas = false;
    fc.OversampleH = 1;
    fc.OversampleV = 1;
    fc.PixelSnapH = true;

    io.Fonts->Clear();

    // ↓ Font #0: Regular text (e.g. Knob labels)
    io.Fonts->AddFontFromMemoryCompressedTTF((void*)LiberationSansTTF_Compressed_compressed_data, LiberationSansTTF_Compressed_compressed_size, 12.5f * getScaleFactor(), &fc);

    // ↓ Font #1: Larger text size for section titles (e.g. "REVERBERATION")
    //            Only load uppercase glyphs for the title font to reduce atlas size, since section titles are always uppercase.
    static constexpr ImWchar kTitleRanges[] = { 'A', 'Z', 0 };
    io.Fonts->AddFontFromMemoryCompressedTTF((void*)LiberationSansTTF_Compressed_compressed_data, LiberationSansTTF_Compressed_compressed_size, 14.0f * getScaleFactor(), &fc, kTitleRanges);

    // ↓ Font #2: Even larger text size for scale marks, which will be down-sampled to 12.5px by the Knob widget for better visual quality.
    //            Only load alphanumeric glyphs for the scale-mark font to reduce atlas size.
    static constexpr ImWchar kScaleMarkRanges[] = { 'A', 'Z', 'a', 'z', '0', '9', '+', ':', 8734, 8734 + 1, 0 };    // 8734 = infinity symbol
    io.Fonts->AddFontFromMemoryCompressedTTF((void*)LiberationSansTTF_Compressed_compressed_data, LiberationSansTTF_Compressed_compressed_size, 16.0f * getScaleFactor(), &fc, kScaleMarkRanges);

    io.Fonts->Build();
    io.FontDefault = io.Fonts->Fonts[0];

    // Specify a larger font for the scale marks to improve rendering quality.
    // The Knob widget will down-sample it to the specified font size (12.5px) to achieve better visual quality.
    kScaleMarkStyle.custom_font = io.Fonts->Fonts[2];
}

void ClassicReverbUI::_drawChassisBackground(float margin, float rounding)
{
    const ImVec2 winPos  = ImGui::GetWindowPos();
    const ImVec2 winSize = ImGui::GetWindowSize();

    // Step 2 – Background panel metrics
    const ImVec2 panelMin = ImVec2(winPos.x + margin,             winPos.y + margin);
    const ImVec2 panelMax = ImVec2(winPos.x + winSize.x - margin, winPos.y + winSize.y - margin);

    // ── Draw shadow & panel directly onto the window draw list ───────────
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Soft drop-shadow: light from upper-left → shadow falls to bottom-right.
    // Layers are drawn farthest-first so nearer (darker) ones paint on top.
    static constexpr int   kShadowLayers = 6;
    static constexpr float kShadowMax    = 9.0f;
    for (int i = kShadowLayers; i >= 1; --i)
    {
        const float frac   = static_cast<float>(i) / kShadowLayers;
        const float offset = kShadowMax * frac; // * scale * frac;
        const int   alpha  = static_cast<int>(70.0f * (kShadowLayers - i + 1) / kShadowLayers);
        dl->AddRectFilled(
            ImVec2(panelMin.x + offset, panelMin.y + offset),
            ImVec2(panelMax.x + offset, panelMax.y + offset),
            IM_COL32(0, 0, 0, alpha), rounding);
    }

    // Main panel – base colour #b77f58
    dl->AddRectFilled(panelMin, panelMax, IM_COL32(0xb7, 0x7f, 0x58, 0xff), rounding);

    // Subtle top-left highlight edge to reinforce the upper-left light
    dl->AddRect(panelMin, panelMax, IM_COL32(0xff, 0xe0, 0xb8, 60), rounding, 0, 1.5f); // 1.5f * scale);
}

void ClassicReverbUI::_addKnob(Parameters paramId, const char* label, float v_min, float v_max, const ImGuiKnobs_Mod::KnobScaleMark *marks, uint32_t mark_count, bool isLogarithmic, bool use_pivot, float pivot_value)
{
    // This is a helper function to add a knob with given parameters.
    // It can be called from onImGuiDisplay() to reduce code duplication.

    constexpr float KNOB_SIZE = 50.0f;
    constexpr int DEFAULT_STEP = 10;

    constexpr auto IMGUIKNOBS_PI = 3.14159265358979323846f;
    constexpr float angle_min = IMGUIKNOBS_PI * (130.0f / 180.0f);   // Down-left 40° (starting point)
    constexpr float angle_max = IMGUIKNOBS_PI * (410.0f / 180.0f);   // Down-right 40° (+360°)

    ImGuiKnobFlags flags = ImGuiKnobFlags_TitleBottom;
    if (isLogarithmic) flags |= ImGuiKnobFlags_Logarithmic;
    if (use_pivot)     flags |= ImGuiKnobFlags_Pivot;

    // Knob color
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(0x2f + 70, 0x4d + 70, 0x44 + 70, 0xff));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(0x2f + 90, 0x4d + 90, 0x44 + 90, 0xff));

    if (ImGuiKnobs_Mod::Knob(label, &fParams[paramId], v_min, v_max, 0.0f, "%.1f", ImGuiKnobVariant_Tick, KNOB_SIZE, flags,
        DEFAULT_STEP, angle_min, angle_max,
        marks, mark_count, &kScaleMarkStyle, pivot_value))
    {
        if (ImGui::IsItemActivated())
        {
            editParameter(paramId, true);
            // TODO: Double-click to reset to default value
        }
        setParameterValue(paramId, fParams[paramId]);
    }

    if (ImGui::IsItemDeactivated())
        editParameter(paramId, false);

    ImGui::PopStyleColor(2);
}

bool ClassicReverbUI::_BeginSection(const char* title, float width)
{
    ImGui::BeginGroup();

    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);    // 14px font size. See _loadFonts()
    ImGui::PushStyleColor(ImGuiCol_Separator, IM_COL32(255, 255, 255, 255));
    ImGuiExt::CenteredSeparatorText(title, width);        
    ImGui::PopStyleColor();
    ImGui::PopFont();

    // Add a vertical gap between the section title and the knobs.
    // This prevents the topmost scale marks from overlapping the title text.
    ImGui::Dummy(ImVec2(0, 8));

    return true;    // Always return true so that we can wrap UI elements in `if` statement.
}

void ClassicReverbUI::_EndSection()
{
  
    ImGui::EndGroup();
}
