#include "UI.h"

#include "CenteredSeparatorText.hpp"
#include "imgui-knobs.h"
#include "AddTextScaled.hpp"

#include "../fonts/LiberationSans-Regular.hpp"
#include "../fonts/CormorantFont.hpp"
#include "src/Resources.hpp"    // For Dejavu Sans font (bundled with DGL)

ImGuiKnobs_Mod::KnobScaleMarkStyle kScaleMarkStyle = {
    .outer_radius = 1.20f,
    .tick_length  = 0.50f,    
    .font_size    = 12.5f,
};

void ClassicReverbUI::_loadFonts()
{
    // Font sizes:
    // - Section title text:                   14px
    // - Chassis Regular text:                 12.5px (e.g. Knob labels)
    // - Larger text size:                     20px (e.g. Knob scale marks, down-sampled to 12.5px for better rendering quality)
    // - ImGui UI text size:                   14.5px (e.g. tooltip text, menu text)

    ImGuiIO& io(ImGui::GetIO());

    ImFontConfig fc;
    fc.FontDataOwnedByAtlas = false;
    fc.OversampleH = 1;
    fc.OversampleV = 1;
    fc.PixelSnapH = true;

    io.Fonts->Clear();

    // ↓ Font #0: Chassis regular text (e.g. Knob labels)
    //            Only load basic Latin glyphs for the chassis font to reduce atlas size, since most chassis text is simple alphanumeric characters.
    static constexpr ImWchar kChassisRanges[] = { ' ', '~', 178, 178 + 1, 0 }; // Basic Latin range. 178 = '²'
    io.Fonts->AddFontFromMemoryCompressedTTF((void*)LiberationSansTTF_Compressed_compressed_data, LiberationSansTTF_Compressed_compressed_size, 12.5f * getScaleFactor(), &fc, kChassisRanges);

    // ↓ Font #1: Larger text size for section titles (e.g. "REVERBERATION")
    //            Only load uppercase glyphs for the title font to reduce atlas size, since section titles are always uppercase.
    static constexpr ImWchar kTitleRanges[] = { 'A', 'Z', 0 };
    io.Fonts->AddFontFromMemoryCompressedTTF((void*)LiberationSansTTF_Compressed_compressed_data, LiberationSansTTF_Compressed_compressed_size, 14.0f * getScaleFactor(), &fc, kTitleRanges);

    // ↓ Font #2: Even larger text size for scale marks, which will be down-sampled to 12.5px by the Knob widget for better visual quality.
    //            For convenience, this font is also used to drawing the "Kjaerhus Audio" logo.
    //            Only load alphanumeric glyphs for the scale-mark font to reduce atlas size.
    static constexpr ImWchar kScaleMarkRanges[] = { 'A', 'Z', 'a', 'z', '0', '9', ' ', ' ' + 1, '+', ':', 8734, 8734 + 1, 198, 198 + 1, 0 };    // 8734 = infinity symbol, 198 = 'Æ' in Liberation Sans
    io.Fonts->AddFontFromMemoryCompressedTTF((void*)LiberationSansTTF_Compressed_compressed_data, LiberationSansTTF_Compressed_compressed_size, 20.0f * getScaleFactor(), &fc, kScaleMarkRanges);

    // ↓ Font #3: Semi-BoldItalic Cormorant font for drawing "Classic Reverb" logo text
    //            Only load essential charset.
    static constexpr ImWchar kPluginNameRanges[] = { 'A', 'Z', 'a', 'z', '0', '9', ' ', ' ' + 1, 0 };
    io.Fonts->AddFontFromMemoryCompressedTTF((void*)CormorantSemiBoldItalicTTF_compressed_data, CormorantSemiBoldItalicTTF_compressed_size, 20.0f * getScaleFactor(), &fc, kPluginNameRanges);

    // ↓ Font #4: Dejavu Sans for ImGui menu and tooltip text (not used in the chassis board, so we can load a full charset)
    io.Fonts->AddFontFromMemoryTTF((void*)dpf_resources::dejavusans_ttf, dpf_resources::dejavusans_ttf_size, 14.5f * getScaleFactor(), &fc);

    io.Fonts->Build();
    io.FontDefault = io.Fonts->Fonts[4];

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

void ClassicReverbUI::_drawKjaerhusLogo(const ImVec2& size)
{
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    [[maybe_unused]] const ImVec2 rect_max = ImVec2(pos.x + size.x, pos.y + size.y);

    //
    // Add a placeholder
    //
    ImGui::Dummy(size); // Reserve space for the logo

    //
    // Draw the "triangle" layered at the bottom of the "AUDIO" text
    // (Actually it is not a real triangle, as its two lanes are Bezier curves.)
    //
    {
        const float triangle_left_line_length = 40.0f;  // The triangle line on the left of charater "A"
        const float triangle_height = 35.0f;

        const ImVec2 triangle_p1 = ImVec2(pos.x + 72.0f, pos.y - 1.0f);
        const ImVec2 triangle_p2 = ImVec2(triangle_p1.x, triangle_p1.y + triangle_left_line_length);
        const ImVec2 triangle_p3 = ImVec2(triangle_p1.x + triangle_height, triangle_p1.y + (triangle_left_line_length * 0.5f));

        // Amount the two slanted edges curve inward (toward the triangle interior).
        // Increase this value for a more pronounced concave effect.
        const float curve_inset = 10.0f;

        // Control point for edge p1 → p3:
        //   midpoint of p1-p3, shifted downward (toward p2) by curve_inset.
        const ImVec2 ctrl_top = ImVec2(
            (triangle_p1.x + triangle_p3.x) * 0.5f,
            (triangle_p1.y + triangle_p3.y) * 0.5f + curve_inset
        );
        // Control point for edge p3 → p2:
        //   midpoint of p3-p2, shifted upward (toward p1) by curve_inset.
        //   Symmetric with ctrl_top because p3 sits at the exact vertical midpoint of p1-p2.
        const ImVec2 ctrl_bot = ImVec2(
            (triangle_p3.x + triangle_p2.x) * 0.5f,
            (triangle_p3.y + triangle_p2.y) * 0.5f - curve_inset
        );

        draw_list->PathClear();
        draw_list->PathLineTo(triangle_p1);
        draw_list->PathBezierQuadraticCurveTo(ctrl_top, triangle_p3);   // p1 → p3 (curved)
        draw_list->PathBezierQuadraticCurveTo(ctrl_bot, triangle_p2);   // p3 → p2 (curved)
        // p2 → p1 is closed as a straight line.
        // PathFillConcave is required because the inward curves make the shape concave.
        draw_list->PathFillConcave(IM_COL32(255, 255, 255, 60));
    }

    //
    // Draw logo text
    //
    ImGuiExt::AddTextScaled(draw_list, ImGui::GetIO().Fonts->Fonts[2], 20.0f,
                            ImVec2(pos.x + 10.0f, pos.y + 8.0f), IM_COL32(255, 255, 255, 255),
                            "KJÆRHUS AUDIO", 0.65f, 1.0f);
    
    //
    // Draw inform text ("Open Source Recreation / Recrated by AnClark") with a semi-transparent rounded-rect background.
    //
    {
        const char* info_text = "Recreated by AnClark";
        constexpr float kOSR_FontSz   = 16.0f;
        constexpr float kOSR_ScaleX   = 0.8f;
        constexpr float kOSR_ScaleY   = 0.8f;
        constexpr float kOSR_PadX     = 8.0f;              // ← adjustable horizontal padding
        constexpr float kOSR_PadY     = 1.0f;              // ← adjustable vertical padding
        constexpr float kOSR_Rounding = 3.0f;              // ← adjustable corner rounding
        constexpr ImU32 kOSR_BgColor  = IM_COL32(100, 100, 100, 60); // ← adjustable bg colour / alpha

        ImFont*        osr_font  = ImGui::GetIO().Fonts->Fonts[2];
        const ImVec2   text_pos  = ImVec2(pos.x + 10.0f, pos.y + 8.0f + 22.0f);
        const ImVec2   raw_sz    = osr_font->CalcTextSizeA(kOSR_FontSz, FLT_MAX, 0.0f, info_text);
        const ImVec2   text_sz   = ImVec2(raw_sz.x * kOSR_ScaleX, raw_sz.y * kOSR_ScaleY);

        draw_list->AddRectFilled(
            ImVec2(text_pos.x - kOSR_PadX, text_pos.y - kOSR_PadY),
            ImVec2(text_pos.x + text_sz.x + kOSR_PadX, text_pos.y + text_sz.y + kOSR_PadY),
            kOSR_BgColor, kOSR_Rounding);

        ImGuiExt::AddTextScaled(draw_list, osr_font, kOSR_FontSz,
                                text_pos, IM_COL32(255, 255, 255, 255),
                                info_text, kOSR_ScaleX, kOSR_ScaleY);
    }
}

void ClassicReverbUI::_drawPluginName()
{
    // TODO: Add an invisible button for "About" dialog.

    ImGui::BeginGroup();

    //
    // Plugin name mark: "Classic Reverb"
    //
    ImGui::AlignTextToFramePadding();   // make the text align with the baseline of the chassis
    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[3]);
    ImGui::Dummy(ImVec2(0, 2)); // Left padding
    ImGui::SameLine();
    ImGui::Text("Classic Reverb");
    ImGui::PopFont();

    ImGui::SameLine();

    //
    // "RE-04" model mark: capsule split into two halves.
    // Left  half – transparent background, white "RE".
    // Right half – solid white background,  black "04".
    //
    {
        ImDrawList*     dl      = ImGui::GetWindowDrawList();
        ImFont*         font    = ImGui::GetIO().Fonts->Fonts[2];
        constexpr float kFontSz = 12.5f;
        constexpr float kPadX   = 5.0f - 2.0f;
        constexpr float kPadY   = 2.0f;
        constexpr float kRound  = 4.0f;

        const ImVec2 re_sz  = font->CalcTextSizeA(kFontSz, FLT_MAX, 0.0f, "RE");
        const ImVec2 o4_sz  = font->CalcTextSizeA(kFontSz, FLT_MAX, 0.0f, "04");
        const float  height = re_sz.y + kPadY * 2.0f;
        const float  lw     = re_sz.x + kPadX * 2.0f;   // left half width
        const float  rw     = o4_sz.x + kPadX * 2.0f;   // right half width

        const ImVec2 cursor_pos  = ImGui::GetCursorScreenPos();
        const ImVec2 p0          = ImVec2(cursor_pos.x, cursor_pos.y + 4.0f); // Vertical adjustment to align with the chassis
        const ImVec2 mid = ImVec2(p0.x + lw,      p0.y);
        const ImVec2 p1  = ImVec2(p0.x + lw + rw, p0.y + height);

        // Right half – solid white fill
        dl->AddRectFilled(mid, p1, IM_COL32(255, 255, 255, 200), kRound, ImDrawFlags_RoundCornersRight);

        // Outer border for the whole capsule – white
        dl->AddRect(p0, p1, IM_COL32(255, 255, 255, 255), kRound);

        // "RE" – white text on transparent left half
        dl->AddText(font, kFontSz, ImVec2(p0.x + kPadX, p0.y + kPadY), IM_COL32(255, 255, 255, 255), "RE");

        // "04" – black text on white right half
        dl->AddText(font, kFontSz, ImVec2(mid.x + kPadX, p0.y + kPadY), IM_COL32(0, 0, 0, 255), "04");

        // Reserve layout space so ImGui accounts for the drawn area
        //ImGui::Dummy(ImVec2(lw + rw, height));
        ImGui::InvisibleButton("##Extra_Info", ImVec2(lw + rw, height));
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_NoSharedDelay))
        {
            ImGui::SetTooltip("\"RE\" stands for Reverse Engineering. Different models of Classic Reverb RE have different timbre.\n"
                "Classic Reverb RE is an open source recreation of the original Kjaerhus Classic Reverb.");
        }
    }

    ImGui::EndGroup();
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

    // Now default font is Droid Sans, so we need to push the chassis font for the knob label and scale marks.
    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);

    if (ImGuiKnobs_Mod::Knob(label, &fParams[paramId], v_min, v_max, 0.0f, "%.1f", ImGuiKnobVariant_Tick, KNOB_SIZE, flags,
        DEFAULT_STEP, angle_min, angle_max,
        marks, mark_count, &kScaleMarkStyle, pivot_value))
    {
        setParameterValue(paramId, fParams[paramId]);
    }

    // NOTE: Putting ImGui::IsItemActivated() in ImGuiKnobs_Mod::Knob() will cause IsItemActivated() unavailable.
    if (ImGui::IsItemActivated())
    {
        editParameter(paramId, true);
        // TODO: Double-click to reset to default value
    }

    if (ImGui::IsItemDeactivated())
        editParameter(paramId, false);

    ImGui::PopFont();        
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
