// HardwareButton.hpp
// Realistic 3D push-button widget for audio plugin UIs.
//
// Design: mirrors the lighting model used in imgui-knobs-mod's draw_shadow
// and draw_circle_3d routines — a soft multi-layer drop shadow and a
// per-segment perimeter rim lit from the top-left — adapted for rounded rects.
//
// Usage:
//   if (ImGuiExt::HardwareButton("Load Preset...##id", ImVec2(w, h), base_color)) { ... }
//
#pragma once

#include <imgui.h>
#include <imgui_internal.h>
#include <cmath>

// Define HARDWARE_BUTTON_TEXT_ELLIPSIS to enable text truncation with "..."
// when the label is wider than the button face.
// Comment out to restore the original (unconstrained, potentially overflowing) behavior.
#define HARDWARE_BUTTON_TEXT_ELLIPSIS

namespace ImGuiExt {

// Draw a hardware-style 3D push button.
// Returns true on click (mouse-button released inside the widget).
//
// Parameters:
//   label     – ImGui label string; text after ## is used as ID only (not drawn).
//   size      – widget size in pixels; height should be GetFrameHeight() or fixed.
//   base_col  – button face color at rest (RGBA floats 0-1).
//   font      – optional explicit font; nullptr = use current ImGui font.
//   font_size – optional explicit font size; 0 = use current ImGui font size.
static inline bool HardwareButton(
        const char *label,
        ImVec2      size,
        ImVec4      base_col,
        ImFont     *font      = nullptr,
        float       font_size = 0.0f)
{
    static constexpr float kPi        = 3.14159265358979323846f;
    static constexpr float kInvSqrt2  = 0.70710678f;
    static constexpr float kRounding  = 5.0f;     // corner radius
    static constexpr int   kArcSegs   = 4;        // segments per quarter-circle corner

    ImDrawList *dl  = ImGui::GetWindowDrawList();

    // ── Capture layout position BEFORE the InvisibleButton shifts the cursor ──
    ImVec2 pos = ImGui::GetCursorScreenPos();

    // ── Interaction ──────────────────────────────────────────────────────────
    bool clicked = ImGui::InvisibleButton(label, size);
    bool hovered = ImGui::IsItemHovered();
    bool active  = ImGui::IsItemActive();   // held down

    // Pressed: shift the rendered body +1,+1 to simulate mechanical depression.
    float shift  = active ? 1.0f : 0.0f;
    ImVec2 bmin  = { pos.x + shift,        pos.y + shift        };
    ImVec2 bmax  = { pos.x + size.x + shift, pos.y + size.y + shift };
    float  r     = kRounding;

    // ── 1. Soft drop shadow — hidden while pressed (same as knob draw_shadow) ─
    // Intentionally lighter than the knob shadow: smaller spread, lower alpha,
    // shorter offset — so a compact button doesn't look visually heavy on the UI.
    if (!active) {
        constexpr int   kLayers    = 4;
        constexpr float kSpread    = 2.0f;
        constexpr int   kCoreAlpha = 32;
        constexpr float kOffset    = 1.5f;
        ImVec2 sc_min = { pos.x + kOffset, pos.y + kOffset };
        ImVec2 sc_max = { pos.x + size.x + kOffset, pos.y + size.y + kOffset };
        for (int i = kLayers; i >= 0; i--) {
            float f  = (float)i / kLayers;
            float ex = kSpread * f;
            int   a  = (int)(kCoreAlpha * (1.0f - f * f));   // quadratic falloff
            if (a <= 0) continue;
            dl->AddRectFilled(
                { sc_min.x - ex, sc_min.y - ex },
                { sc_max.x + ex, sc_max.y + ex },
                IM_COL32(0, 0, 0, a), r + ex);
        }
    }

    // ── 2. Body fill ─────────────────────────────────────────────────────────
    float brightness = active ? 0.78f : (hovered ? 1.10f : 1.0f);
    ImU32 body_col   = IM_COL32(
        (int)ImClamp(base_col.x * 255.0f * brightness, 0.0f, 255.0f),
        (int)ImClamp(base_col.y * 255.0f * brightness, 0.0f, 255.0f),
        (int)ImClamp(base_col.z * 255.0f * brightness, 0.0f, 255.0f),
        (int)(base_col.w * 255.0f));
    dl->AddRectFilled(bmin, bmax, body_col, r);

    // ── 3. Gradient rim — same lighting model as imgui-knobs draw_circle_3d ──
    //
    // Light source at top-left.  In y-down screen coords, the direction TOWARD
    // the light is L = (−1, −1)/√2.
    //
    // For each perimeter segment the per-segment illumination is:
    //   illum = dot(outward_normal, L) = −(nx + ny) · kInvSqrt2
    //
    // illum is remapped from [−1, +1] to [0, 1], then used to choose a grey
    // alpha for the rim stroke — bright on top-left, dark on bottom-right.
    // When active the mapping is flipped, reversing the bevel so it looks
    // sunken rather than raised.
    {
        float rim_t = ImMax(1.2f, size.y * 0.055f);   // rim thickness (px)

        // Illumination of outward normal (nx, ny) → [0, 1]
        auto illum_of = [](float nx, float ny) -> float {
            return (-(nx + ny) * kInvSqrt2 + 1.0f) * 0.5f;
        };

        // Grey rim stroke color for normalised illumination t∈[0,1]
        auto rim_color = [&](float t) -> ImU32 {
            if (active) t = 1.0f - t;     // reverse bevel on press
            int gray  = (int)(255.0f * t);
            int alpha = (int)(45.0f + 70.0f * t);  // 45 (shadow side) → 115 (highlight side)
            return IM_COL32(gray, gray, gray, alpha);
        };

        // Subdivide one quarter-circle corner into kArcSegs short line segments.
        // Each segment's outward normal is the radial direction at its midpoint.
        auto draw_arc_rim = [&](ImVec2 c, float a_start, float a_end) {
            for (int j = 0; j < kArcSegs; j++) {
                float a0    = a_start + (a_end - a_start) *  j          / kArcSegs;
                float a1    = a_start + (a_end - a_start) * (j + 1)     / kArcSegs;
                float a_mid = (a0 + a1) * 0.5f;
                float t     = illum_of(cosf(a_mid), sinf(a_mid));
                ImVec2 p0   = { c.x + r * cosf(a0), c.y + r * sinf(a0) };
                ImVec2 p1   = { c.x + r * cosf(a1), c.y + r * sinf(a1) };
                dl->AddLine(p0, p1, rim_color(t), rim_t);
            }
        };

        // Corner arcs (ImGui y-down conventions).
        // TL: π → 3π/2  (normals point upper-left → all bright)
        draw_arc_rim({ bmin.x + r, bmin.y + r }, kPi,         kPi * 1.5f);
        // TR: −π/2 → 0  (normals: up=bright → right=dark, transition)
        draw_arc_rim({ bmax.x - r, bmin.y + r }, -kPi * 0.5f, 0.0f      );
        // BR: 0 → π/2   (normals point lower-right → all dark)
        draw_arc_rim({ bmax.x - r, bmax.y - r }, 0.0f,         kPi * 0.5f);
        // BL: π/2 → π   (normals: down=dark → left=bright, transition)
        draw_arc_rim({ bmin.x + r, bmax.y - r }, kPi * 0.5f,  kPi       );

        // Straight edges — outward normal is constant per edge.
        dl->AddLine({ bmin.x + r, bmin.y        }, { bmax.x - r, bmin.y        }, rim_color(illum_of( 0, -1)), rim_t); // top
        dl->AddLine({ bmax.x,     bmin.y + r    }, { bmax.x,     bmax.y - r    }, rim_color(illum_of( 1,  0)), rim_t); // right
        dl->AddLine({ bmin.x + r, bmax.y        }, { bmax.x - r, bmax.y        }, rim_color(illum_of( 0,  1)), rim_t); // bottom
        dl->AddLine({ bmin.x,     bmin.y + r    }, { bmin.x,     bmax.y - r    }, rim_color(illum_of(-1,  0)), rim_t); // left
    }

    // ── 4. Specular inner highlight — thin bright strip along the top face ───
    // Simulates a convex button surface catching top-left light.
    // Hidden when pressed (face is now in shadow).
    if (!active) {
        dl->AddRectFilled(
            { bmin.x + r,     bmin.y + 1.5f },
            { bmax.x - r,     bmin.y + 3.5f },
            IM_COL32(255, 255, 255, 22));
    }

    // ── 5. Text (centered, with subtle drop shadow) ───────────────────────────
    {
        ImFont *draw_font  = font      ? font      : ImGui::GetFont();
        float   fs         = font_size > 0.0f ? font_size : ImGui::GetFontSize();

        // Trim the ##id suffix: only compute layout for the visible portion.
        const char *text_end = ImGui::FindRenderedTextEnd(label);
        ImVec2 text_sz = draw_font->CalcTextSizeA(fs, FLT_MAX, 0.0f, label, text_end);

#if defined(HARDWARE_BUTTON_TEXT_ELLIPSIS)
        // ── Ellipsis truncation ───────────────────────────────────────────────
        // Leave a small margin inside the rounded corners on each side.
        constexpr float kTextPadX = 4.0f;
        float avail_w = size.x - kTextPadX * 2.0f;

        const char *draw_end    = text_end;  // end of the visible label portion
        float       clipped_w   = text_sz.x; // pixel width of the visible label portion
        float       ellipsis_w  = 0.0f;
        bool        has_ellipsis = false;

        if (text_sz.x > avail_w) {
            has_ellipsis = true;
            ellipsis_w   = draw_font->CalcTextSizeA(fs, FLT_MAX, 0.0f, "...", nullptr).x;
            // CalcTextSizeA with max_width stops at the last glyph that still fits;
            // draw_end is set to the byte just past that glyph.
            float fit_w  = ImMax(0.0f, avail_w - ellipsis_w);
            clipped_w    = draw_font->CalcTextSizeA(fs, fit_w, 0.0f, label, text_end, &draw_end).x;
        }

        // Center the (possibly truncated + "...") text on the button face.
        float total_w = clipped_w + ellipsis_w;
        float tx = bmin.x + (size.x - total_w) * 0.5f;
        float ty = bmin.y + (size.y - text_sz.y) * 0.5f;

        // Shadow pass
        dl->AddText(draw_font, fs, { tx + 1.0f, ty + 1.0f },
                    IM_COL32(0, 0, 0, 70), label, draw_end);
        if (has_ellipsis)
            dl->AddText(draw_font, fs, { tx + clipped_w + 1.0f, ty + 1.0f },
                        IM_COL32(0, 0, 0, 70), "...");
        // Main text pass
        dl->AddText(draw_font, fs, { tx, ty },
                    IM_COL32(210, 225, 220, 255), label, draw_end);
        if (has_ellipsis)
            dl->AddText(draw_font, fs, { tx + clipped_w, ty },
                        IM_COL32(210, 225, 220, 255), "...");

#else   // original behavior — text may overflow beyond the button edges
        // Center on button face; include shift so text moves with body on press.
        float tx = bmin.x + (size.x - text_sz.x) * 0.5f;
        float ty = bmin.y + (size.y - text_sz.y) * 0.5f;

        // 1px text shadow for depth
        dl->AddText(draw_font, fs, { tx + 1.0f, ty + 1.0f },
                    IM_COL32(0, 0, 0, 70), label, text_end);
        // Main text — light to contrast the dark body
        dl->AddText(draw_font, fs, { tx, ty },
                    IM_COL32(210, 225, 220, 255), label, text_end);
#endif
    }

    // ── 6. Mouse cursor ───────────────────────────────────────────────────────
    if (hovered && !active)
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

    return clicked;
}
}
