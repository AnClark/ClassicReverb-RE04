#include "imgui-knobs.h"

#include <cmath>
#include <cstdlib>
#include <imgui.h>
#include <imgui_internal.h>

#define IMGUIKNOBS_PI 3.14159265358979323846f

static inline float ImLog(int x) { return ImLog(static_cast<float>(x)); }

namespace ImGuiKnobs_Mod {
    namespace detail {
        void draw_arc(ImVec2 center, float radius, float start_angle, float end_angle, float thickness, ImColor color) {
            auto *draw_list = ImGui::GetWindowDrawList();

            draw_list->PathArcTo(center, radius, start_angle, end_angle);
            draw_list->PathStroke(color, 0, thickness);
        }

        template<typename DataType>
        struct knob {
            float radius;
            bool value_changed;
            ImVec2 center;
            bool is_active;
            bool is_hovered;
            float angle_min;
            float angle_max;
            float t;
            float angle;
            float angle_cos;
            float angle_sin;
            float pivot_value; // bilinear pivot in value space (used when ImGuiKnobFlags_Pivot)

            knob(const char *_label,
                 ImGuiDataType data_type,
                 DataType *p_value,
                 DataType v_min,
                 DataType v_max,
                 float speed,
                 float _radius,
                 const char *format,
                 ImGuiKnobFlags flags,
                 float _angle_min,
                 float _angle_max,
                 float _pivot_value = 0.0f) {
                radius      = _radius;
                pivot_value = _pivot_value;
                if (flags & ImGuiKnobFlags_Pivot) {
                    // Bilinear mapping:
                    //   [v_min,  pivot] → t ∈ [0.0, 0.5]  (spread over bottom half of travel)
                    //   [pivot,  v_max] → t ∈ [0.5, 1.0]  (spread over top half of travel)
                    // This makes the knob centre (t=0.5) correspond exactly to pivot_value.
                    float v  = (float)ImMax(ImMin(*p_value, v_max), v_min);
                    float lo = (float)v_min, hi = (float)v_max, pv = _pivot_value;
                    if (v <= pv)
                        t = (lo != pv) ? 0.5f * (v - lo) / (pv - lo) : 0.0f;
                    else
                        t = (pv != hi) ? 0.5f + 0.5f * (v - pv) / (hi - pv) : 1.0f;
                } else if (flags & ImGuiKnobFlags_Logarithmic) {
                    float v = ImMax(ImMin(*p_value, v_max), v_min);
                    t = (ImLog(ImAbs(v)) - ImLog(ImAbs(v_min))) / (ImLog(ImAbs(v_max)) - ImLog(ImAbs(v_min)));
                } else {
                    t = ((float) *p_value - v_min) / (v_max - v_min);
                }
                auto screen_pos = ImGui::GetCursorScreenPos();

                // Handle dragging
                ImGui::InvisibleButton(_label, {radius * 2.0f, radius * 2.0f});

                // Handle drag: if DragVertical or DragHorizontal flags are set, only the given direction is
                // used, otherwise use the drag direction with the highest delta
                ImGuiIO &io = ImGui::GetIO();
                bool drag_vertical =
                        !(flags & ImGuiKnobFlags_DragHorizontal) &&
                        (flags & ImGuiKnobFlags_DragVertical || ImAbs(io.MouseDelta[ImGuiAxis_Y]) > ImAbs(io.MouseDelta[ImGuiAxis_X]));

                auto gid = ImGui::GetID(_label);
                ImGuiSliderFlags drag_behaviour_flags = 0;
                if (drag_vertical) {
                    drag_behaviour_flags |= ImGuiSliderFlags_Vertical;
                }
                if (flags & ImGuiKnobFlags_AlwaysClamp) {
                    drag_behaviour_flags |= ImGuiSliderFlags_AlwaysClamp;
                }
                if (flags & ImGuiKnobFlags_Logarithmic) {
                    drag_behaviour_flags |= ImGuiSliderFlags_Logarithmic;
                }
                value_changed = ImGui::DragBehavior(
                        gid,
                        data_type,
                        p_value,
                        speed,
                        &v_min,
                        &v_max,
                        format,
                        drag_behaviour_flags);

                angle_min = _angle_min < 0 ? IMGUIKNOBS_PI * 0.75f : _angle_min;
                angle_max = _angle_max < 0 ? IMGUIKNOBS_PI * 2.25f : _angle_max;

                center = {screen_pos[0] + radius, screen_pos[1] + radius};
                is_active = ImGui::IsItemActive();
                is_hovered = ImGui::IsItemHovered();
                angle = angle_min + (angle_max - angle_min) * t;
                angle_cos = cosf(angle);
                angle_sin = sinf(angle);

#if IMGUI_KNOBS_SET_CURSOR
                // AnClark's MOD: Set mouse cursor based on interaction state.
                //                Useful for audio plugins where better user experience can be achieved by providing visual feedback
                //                on when the knob is interactive.
                if (is_hovered && !is_active) {
                    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                }
                if (is_active) {
                    ImGui::SetMouseCursor((flags & ImGuiKnobFlags_DragHorizontal) ? ImGuiMouseCursor_ResizeEW : ImGuiMouseCursor_ResizeNS);
                }
#endif
            }

            void draw_dot(float size, float radius, float angle, color_set color, bool filled, int segments) {
                auto dot_size = size * this->radius;
                auto dot_radius = radius * this->radius;

                ImGui::GetWindowDrawList()->AddCircleFilled(
                        {center[0] + cosf(angle) * dot_radius,
                         center[1] + sinf(angle) * dot_radius},
                        dot_size,
                        is_active ? color.active : (is_hovered ? color.hovered : color.base),
                        segments);
            }

            void draw_tick(float start, float end, float width, float angle, color_set color) {
                auto tick_start = start * radius;
                auto tick_end = end * radius;
                auto angle_cos = cosf(angle);
                auto angle_sin = sinf(angle);

                ImGui::GetWindowDrawList()->AddLine(
                        {center[0] + angle_cos * tick_end, center[1] + angle_sin * tick_end},
                        {center[0] + angle_cos * tick_start,
                         center[1] + angle_sin * tick_start},
                        is_active ? color.active : (is_hovered ? color.hovered : color.base),
                        width * radius);
            }

            void draw_circle(float size, color_set color, bool filled, int segments) {
                auto circle_radius = size * radius;

                ImGui::GetWindowDrawList()->AddCircleFilled(
                        center,
                        circle_radius,
                        is_active ? color.active : (is_hovered ? color.hovered : color.base));
            }

            // Drop shadow. When soft=true, layered concentric circles simulate a blurred penumbra;
            // when soft=false, a single flat semi-transparent circle is drawn instead.
            void draw_shadow(float size, int segments = 32, bool soft = true) {
                auto *draw_list = ImGui::GetWindowDrawList();
                float base_radius = size * radius;
                float offset = radius * 0.07f;
                ImVec2 shadow_center = {center[0] + offset, center[1] + offset};

                if (soft) {
                    // Layers from outermost (large, very transparent) to innermost (dense core).
                    // Each layer expands the radius by a fraction of `radius` and reduces alpha,
                    // producing a smooth penumbra without a hard edge.
                    const int   layers     = 6;
                    const float spread     = radius * 0.18f; // total blur spread in pixels
                    const int   core_alpha = 60;             // alpha at the hard core
                    for (int i = layers; i >= 0; i--) {
                        float f = (float) i / layers;               // 1 = outermost, 0 = core
                        float r = base_radius + spread * f;
                        int   a = (int)(core_alpha * (1.0f - f * f)); // quadratic falloff
                        if (a <= 0) continue;
                        draw_list->AddCircleFilled(shadow_center, r, IM_COL32(0, 0, 0, a), segments);
                    }
                } else {
                    draw_list->AddCircleFilled(shadow_center, base_radius, IM_COL32(0, 0, 0, 70), segments);
                }
            }

            // 3D body: filled circle + smooth gradient rim simulating top-left light source.
            void draw_circle_3d(float size, color_set color, int segments = 32) {
                auto *draw_list = ImGui::GetWindowDrawList();
                auto body_radius = size * radius;
                ImColor col = is_active ? color.active : (is_hovered ? color.hovered : color.base);

                // Main body fill
                draw_list->AddCircleFilled(center, body_radius, col, segments);

                // Gradient rim: subdivide the rim into small arcs and interpolate color per segment.
                // Light source is at top-left. In ImGui y-down coords, "top-left" outward direction
                // is angle 5π/4 (cos<0, sin<0). The per-segment illumination is computed as the
                // dot product of the outward rim normal with the direction TOWARD the light:
                //   illum = -(cos(a) + sin(a)) / sqrt(2),  range [-1, +1]
                // This makes top-left rim bright (illum≈+1) and bottom-right dark (illum≈-1).
                float rim_t   = body_radius * 0.10f;
                float rim_r   = body_radius - rim_t * 0.5f;
                const float inv_sqrt2 = 0.70710678f;
                const int   rim_segs  = 36; // enough for smooth visual gradient
                for (int i = 0; i < rim_segs; i++) {
                    float a0    = (IMGUIKNOBS_PI * 2.0f *  i      ) / rim_segs;
                    float a1    = (IMGUIKNOBS_PI * 2.0f * (i + 1) ) / rim_segs;
                    float a_mid = (a0 + a1) * 0.5f;
                    // illum in [-1, +1]; remap to [0, 1]
                    float illum = -(cosf(a_mid) + sinf(a_mid)) * inv_sqrt2;
                    float t     = (illum + 1.0f) * 0.5f;
                    // gray: 0 (bottom-right shadow) → 255 (top-left highlight)
                    int gray  = (int)(255.0f * t);
                    int alpha = (int)(80.0f  + 40.0f * t); // 80 (dark side) → 120 (light side)
                    draw_list->PathArcTo(center, rim_r, a0, a1, 1);
                    draw_list->PathStroke(IM_COL32(gray, gray, gray, alpha), 0, rim_t);
                }
            }

            void draw_arc(float radius, float size, float start_angle, float end_angle, color_set color) {
                auto track_radius = radius * this->radius;
                auto track_size = size * this->radius * 0.5f + 0.0001f;

                detail::draw_arc(center, track_radius, start_angle, end_angle, track_size, is_active ? color.active : (is_hovered ? color.hovered : color.base));
            }

            // Draw external scale marks (tick lines + optional text labels) around
            // the knob.  Marks are positioned by mapping each mark's `value` through
            // the same angle mapping used for the knob indicator, so they line up
            // precisely with the knob's travel range.
            void draw_scale_marks(
                    const KnobScaleMark *marks,
                    int count,
                    float v_min_f,
                    float v_max_f,
                    ImGuiKnobFlags flags,
                    const KnobScaleMarkStyle *style) {
                if (!marks || count <= 0) { return; }

                static const KnobScaleMarkStyle default_style;
                const KnobScaleMarkStyle &s = style ? *style : default_style;

                auto *draw_list = ImGui::GetWindowDrawList();
                ImFont *font    = (s.custom_font != nullptr)
                                ? s.custom_font
                                : ImGui::GetFont();

                float fs = s.font_size > 0.0f
                         ? s.font_size
                         : ImGui::GetFontSize() * 0.75f;

                // Resolve sentinel color (0,0,0,0) => ImGuiCol_Text
                auto resolve_color = [](const ImVec4 &c) -> ImU32 {
                    if (c.x == 0.0f && c.y == 0.0f && c.z == 0.0f && c.w == 0.0f) {
                        return ImGui::ColorConvertFloat4ToU32(
                                ImGui::GetStyle().Colors[ImGuiCol_Text]);
                    }
                    return ImGui::ColorConvertFloat4ToU32(c);
                };
                ImU32 tick_col = resolve_color(s.tick_color);
                ImU32 text_col = resolve_color(s.text_color);

                float r_outer = s.outer_radius * this->radius;
                float r_inner = r_outer - s.tick_length * this->radius;
                float line_w  = ImMax(1.0f, s.tick_width * this->radius);

                for (int i = 0; i < count; i++) {
                    const KnobScaleMark &m = marks[i];

                    // Compute normalised position t in [0,1]
                    float t_mark;
                    if (flags & ImGuiKnobFlags_Pivot) {
                        float v  = ImClamp(m.value, v_min_f, v_max_f);
                        float pv = pivot_value;
                        if (v <= pv)
                            t_mark = (v_min_f != pv) ? 0.5f * (v - v_min_f) / (pv - v_min_f) : 0.0f;
                        else
                            t_mark = (pv != v_max_f) ? 0.5f + 0.5f * (v - pv) / (v_max_f - pv) : 1.0f;
                    } else if (flags & ImGuiKnobFlags_Logarithmic) {
                        float v   = ImMax(ImMin(m.value, v_max_f), v_min_f);
                        float lv  = ImLog(v);
                        float lmn = ImLog(v_min_f);
                        float lmx = ImLog(v_max_f);
                        t_mark = (lmx != lmn) ? (lv - lmn) / (lmx - lmn) : 0.0f;
                    } else {
                        t_mark = (v_max_f != v_min_f)
                               ? (m.value - v_min_f) / (v_max_f - v_min_f)
                               : 0.0f;
                    }
                    t_mark = ImClamp(t_mark, 0.0f, 1.0f);

                    float a  = angle_min + (angle_max - angle_min) * t_mark;
                    float ca = cosf(a);
                    float sa = sinf(a);

                    // Tick line
                    ImVec2 p0 = {center.x + ca * r_inner, center.y + sa * r_inner};
                    ImVec2 p1 = {center.x + ca * r_outer, center.y + sa * r_outer};
                    draw_list->AddLine(p0, p1, tick_col, line_w);

                    // Text label (centered on the radial direction)
                    if (m.label) {
                        float label_r   = r_outer + fs * (0.5f + s.label_gap * 0.5f);
                        ImVec2 text_sz  = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, m.label);
                        ImVec2 text_pos = {
                            center.x + ca * label_r - text_sz.x * 0.5f,
                            center.y + sa * label_r - text_sz.y * 0.5f
                        };

                        // HACK: When the label is at the middle of the knob, apply a small nudge to let it display properly on the center 
                        if (t_mark == 0.5f) {
                            text_pos.x += 1.0f;
                        }

                        draw_list->AddText(font, fs, text_pos, text_col, m.label);
                    }
                }
            }
        };

        template<typename DataType>
        knob<DataType> knob_with_drag(
                const char *label,
                ImGuiDataType data_type,
                DataType *p_value,
                DataType v_min,
                DataType v_max,
                float _speed,
                const char *format,
                float size,
                ImGuiKnobFlags flags,
                float angle_min,
                float angle_max,
                const KnobScaleMarkStyle *marks_pad_style = nullptr,
                float pivot_value = 0.0f) {
            if (flags & ImGuiKnobFlags_Logarithmic && v_min <= 0.0 && v_max >= 0.0) {
                // we must handle the cornercase if a client specifies a logarithmic range that contains zero
                // for this we clamp lower limit to avoid hitting zero like it is done in ImGui::SliderBehaviorT
                const bool is_floating_point = (data_type == ImGuiDataType_Float) || (data_type == ImGuiDataType_Double);
                const int decimal_precision = is_floating_point ? ImParseFormatPrecision(format, 3) : 1;
                v_min = ImPow(0.1f, (float) decimal_precision);
                v_max = ImMax(v_min, v_max); // this ensures that in the cornercase v_max is still at least ge v_min
                *p_value = ImMax(ImMin(*p_value, v_max), v_min); // this ensures that in the cornercase p_value is within the range
            }

            // For Pivot mapping the two halves have different value ranges but
            // identical angular travel (each = half the total arc).  To keep the
            // visual rotation speed uniform, speed must be proportional to the
            // local dv/dt = 2*(range of current half) rather than the full v range.
            float speed;
            if (_speed != 0) {
                speed = _speed;
            } else if (flags & ImGuiKnobFlags_Pivot) {
                float cv = (float)*p_value;
                speed = cv <= pivot_value
                      ? 2.0f * (pivot_value - (float)v_min) / 250.f
                      : 2.0f * ((float)v_max - pivot_value) / 250.f;
            } else {
                speed = (v_max - v_min) / 250.f;
            }
            ImGui::PushID(label);

#if IMGUI_VERSION_NUM < 19197
            auto font_scale = ImGui::GetIO().FontGlobalScale;
#else
            auto font_scale = ImGui::GetStyle().FontScaleMain;
#endif
            auto width = size == 0 ? ImGui::GetTextLineHeight() * 4.0f : size * font_scale;
            ImGui::PushItemWidth(width);

            // Pre-compute vertical padding needed to keep scale marks from
            // overlapping the title label.  Marks are drawn outside the knob body
            // via raw draw-list calls, so ImGui's layout system is unaware of them.
            //
            // Standard angle range [0.75π, 2.25π] in screen (y-down) coordinates:
            //   • Topmost mark  at angle 1.5π → sin = −1 → extends UPWARD   by label_extent
            //   • Bottom marks  at angle_min/max → sin ≈ 0.707 → extends DOWNWARD by 0.707·label_extent
            float pad_top = 0.0f, pad_bot = 0.0f;
            if (marks_pad_style && !(flags & ImGuiKnobFlags_NoTitle)) {
                float r   = width * 0.5f;
                float fs  = marks_pad_style->font_size > 0.0f
                          ? marks_pad_style->font_size
                          : ImGui::GetFontSize() * 0.75f;
                float label_extent = marks_pad_style->outer_radius * r
                                   + fs * (0.5f + marks_pad_style->label_gap * 0.5f);
                pad_top = ImMax(0.0f,           label_extent - r);  // top mark reaches straight up
                pad_bot = ImMax(0.0f, 0.707f * label_extent - r);  // bottom marks at ±45° from bottom
            }

            ImGui::BeginGroup();

            // There's an issue with `SameLine` and Groups, see
            // https://github.com/ocornut/imgui/issues/4190. This is probably not the best
            // solution, but seems to work for now
            ImGui::GetCurrentWindow()->DC.CurrLineTextBaseOffset = 0;

            // Draw title (top, default)
            if (!(flags & ImGuiKnobFlags_NoTitle) && !(flags & ImGuiKnobFlags_TitleBottom)) {
                // NOTE: Set wrap width from given width ("width" param) to 0.0f, so long titles will wrap within the knob's width
                //       instead of overflowing horizontally.
                //       This fixed an issue: putting a space in the title (e.g. "Size (m²)") makes the title unwrap and overflow horizontally,
                //       even if the knob is not so narrow.
                auto title_size = ImGui::CalcTextSize(label, NULL, true, 0.0f);

                // Center title
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                                     (width - title_size[0]) * 0.5f);

                ImGui::TextUnformatted(label, ImGui::FindRenderedTextEnd(label));

                // Gap between title and knob body so top scale marks don't overlap the label
                if (pad_top > 0.0f)
                    ImGui::Dummy(ImVec2(width, pad_top));
            }

            // Draw knob
            knob<DataType> k(label, data_type, p_value, v_min, v_max, speed, width * 0.5f, format, flags, angle_min, angle_max, pivot_value);

            // Draw title (bottom)
            if (!(flags & ImGuiKnobFlags_NoTitle) && (flags & ImGuiKnobFlags_TitleBottom)) {
                // Gap between knob body and title so bottom scale marks don't overlap the label
                if (pad_bot > 0.0f)
                    ImGui::Dummy(ImVec2(width, pad_bot));

                auto title_size = ImGui::CalcTextSize(label, NULL, true, 0.0f);

                // Center title
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                                     (width - title_size[0]) * 0.5f);

                ImGui::TextUnformatted(label, ImGui::FindRenderedTextEnd(label));
            }

            // Draw tooltip
            if (flags & ImGuiKnobFlags_ValueTooltip &&
                (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) ||
                 ImGui::IsItemActive())) {
                ImGui::BeginTooltip();
                ImGui::Text(format, *p_value);
                ImGui::EndTooltip();
            }

            // Draw input
            if (flags & ImGuiKnobFlags_EnableInput) {
                ImGuiSliderFlags drag_scalar_flags = 0;
                if (flags & ImGuiKnobFlags_AlwaysClamp) {
                    drag_scalar_flags |= ImGuiSliderFlags_AlwaysClamp;
                }
                if (flags & ImGuiKnobFlags_Logarithmic) {
                    drag_scalar_flags |= ImGuiSliderFlags_Logarithmic;
                }
                auto changed = ImGui::DragScalar("###knob_drag", data_type, p_value, speed, &v_min, &v_max, format, drag_scalar_flags);
                if (changed) {
                    k.value_changed = true;
                }
            }

            ImGui::EndGroup();
            ImGui::PopItemWidth();
            ImGui::PopID();

            return k;
        }

        color_set GetPrimaryColorSet() {
            auto *colors = ImGui::GetStyle().Colors;

            return {colors[ImGuiCol_ButtonActive], colors[ImGuiCol_ButtonHovered], colors[ImGuiCol_ButtonHovered]};
        }

        color_set GetSecondaryColorSet() {
            auto *colors = ImGui::GetStyle().Colors;
            auto active = ImVec4(colors[ImGuiCol_ButtonActive].x * 0.5f,
                                 colors[ImGuiCol_ButtonActive].y * 0.5f,
                                 colors[ImGuiCol_ButtonActive].z * 0.5f,
                                 colors[ImGuiCol_ButtonActive].w);

            auto hovered = ImVec4(colors[ImGuiCol_ButtonHovered].x * 0.5f,
                                  colors[ImGuiCol_ButtonHovered].y * 0.5f,
                                  colors[ImGuiCol_ButtonHovered].z * 0.5f,
                                  colors[ImGuiCol_ButtonHovered].w);

            return {active, hovered, hovered};
        }

        color_set GetTrackColorSet() {
            auto *colors = ImGui::GetStyle().Colors;

            return {colors[ImGuiCol_Button], colors[ImGuiCol_Button], colors[ImGuiCol_Button]};
        }
    }// namespace detail

    template<typename DataType>
    bool BaseKnob(
            const char *label,
            ImGuiDataType data_type,
            DataType *p_value,
            DataType v_min,
            DataType v_max,
            float speed,
            const char *format,
            ImGuiKnobVariant variant,
            float size,
            ImGuiKnobFlags flags,
            int steps,
            float angle_min,
            float angle_max,
            const KnobScaleMark *marks,
            int mark_count,
            const KnobScaleMarkStyle *mark_style,
            float pivot_value = 0.0f) {
        // Use a default style instance as the padding reference when the caller
        // passes marks but no explicit style, so padding is still computed.
        static const KnobScaleMarkStyle s_default_mark_style;
        const KnobScaleMarkStyle *pad_style =
                (marks && mark_count > 0)
                ? (mark_style ? mark_style : &s_default_mark_style)
                : nullptr;

        auto knob = detail::knob_with_drag(
                label,
                data_type,
                p_value,
                v_min,
                v_max,
                speed,
                format,
                size,
                flags,
                angle_min,
                angle_max,
                pad_style,
                pivot_value);

        // Scale marks — drawn FIRST so the knob shadow/body renders on top of them
        knob.draw_scale_marks(marks, mark_count, (float) v_min, (float) v_max, flags, mark_style);

        switch (variant) {
            case ImGuiKnobVariant_Tick: {
                // Real knob: shadow → 3D body → groove tick → specular spot
                knob.draw_shadow(0.85f);
                knob.draw_circle_3d(0.85f, detail::GetSecondaryColorSet());
                knob.draw_tick(0.5f, 0.82f, 0.08f, knob.angle, detail::GetPrimaryColorSet());
                break;
            }
            case ImGuiKnobVariant_Dot: {
                // Real knob: shadow → 3D body → dot indicator → specular spot
                knob.draw_shadow(0.85f);
                knob.draw_circle_3d(0.85f, detail::GetSecondaryColorSet());
                knob.draw_dot(0.12f, 0.60f, knob.angle, detail::GetPrimaryColorSet(), true, 12);
                break;
            }

            case ImGuiKnobVariant_Wiper: {
                // Real knob: arc track → shadow → 3D body → wiper arc → specular spot
                knob.draw_arc(0.8f, 0.41f, knob.angle_min, knob.angle_max, detail::GetTrackColorSet());
                knob.draw_shadow(0.72f);
                knob.draw_circle_3d(0.7f, detail::GetSecondaryColorSet());
                if (knob.t > 0.01f) {
                    knob.draw_arc(0.8f, 0.43f, knob.angle_min, knob.angle, detail::GetPrimaryColorSet());
                }
                break;
            }
            case ImGuiKnobVariant_WiperOnly: {
                knob.draw_arc(0.8f, 0.41f, knob.angle_min, knob.angle_max, detail::GetTrackColorSet());
                if (knob.t > 0.01f) {
                    knob.draw_arc(0.8f, 0.43f, knob.angle_min, knob.angle, detail::GetPrimaryColorSet());
                }
                break;
            }
            case ImGuiKnobVariant_WiperDot: {
                // Real knob: arc track → shadow → 3D body → outer dot → specular spot
                knob.draw_arc(0.85f, 0.41f, knob.angle_min, knob.angle_max, detail::GetTrackColorSet());
                knob.draw_shadow(0.62f);
                knob.draw_circle_3d(0.6f, detail::GetSecondaryColorSet());
                knob.draw_dot(0.10f, 0.85f, knob.angle, detail::GetPrimaryColorSet(), true, 12);
                break;
            }
            case ImGuiKnobVariant_Stepped: {
                // Real knob: step ticks → shadow → 3D body → dot indicator → specular spot
                for (auto n = 0.f; n < steps; n++) {
                    auto a = n / (steps - 1);
                    auto angle = knob.angle_min + (knob.angle_max - knob.angle_min) * a;
                    knob.draw_tick(0.72f, 0.90f, 0.04f, angle, detail::GetPrimaryColorSet());
                }
                knob.draw_shadow(0.62f);
                knob.draw_circle_3d(0.6f, detail::GetSecondaryColorSet());
                knob.draw_dot(0.12f, 0.40f, knob.angle, detail::GetPrimaryColorSet(), true, 12);
                break;
            }
            case ImGuiKnobVariant_Space: {
                // Space variant: shadow → small dynamic core → concentric arcs
                float core_r = 0.3f - knob.t * 0.1f;
                knob.draw_shadow(core_r + 0.04f, 16);
                knob.draw_circle_3d(core_r, detail::GetSecondaryColorSet(), 16);
                if (knob.t > 0.01f) {
                    knob.draw_arc(0.4f, 0.15f, knob.angle_min - 1.0f, knob.angle - 1.0f, detail::GetPrimaryColorSet());
                    knob.draw_arc(0.6f, 0.15f, knob.angle_min + 1.0f, knob.angle + 1.0f, detail::GetPrimaryColorSet());
                    knob.draw_arc(0.8f, 0.15f, knob.angle_min + 3.0f, knob.angle + 3.0f, detail::GetPrimaryColorSet());
                }
                break;
            }
        }

        return knob.value_changed;
    }

    bool Knob(
            const char *label,
            float *p_value,
            float v_min,
            float v_max,
            float speed,
            const char *format,
            ImGuiKnobVariant variant,
            float size,
            ImGuiKnobFlags flags,
            int steps,
            float angle_min,
            float angle_max,
            const KnobScaleMark *marks,
            int mark_count,
            const KnobScaleMarkStyle *mark_style,
            float pivot_value) {
        return BaseKnob(
                label,
                ImGuiDataType_Float,
                p_value,
                v_min,
                v_max,
                speed,
                format,
                variant,
                size,
                flags,
                steps,
                angle_min,
                angle_max,
                marks,
                mark_count,
                mark_style,
                pivot_value);
    }

    bool KnobInt(
            const char *label,
            int *p_value,
            int v_min,
            int v_max,
            float speed,
            const char *format,
            ImGuiKnobVariant variant,
            float size,
            ImGuiKnobFlags flags,
            int steps,
            float angle_min,
            float angle_max,
            const KnobScaleMark *marks,
            int mark_count,
            const KnobScaleMarkStyle *mark_style,
            float pivot_value) {
        return BaseKnob(
                label,
                ImGuiDataType_S32,
                p_value,
                v_min,
                v_max,
                speed,
                format,
                variant,
                size,
                flags,
                steps,
                angle_min,
                angle_max,
                marks,
                mark_count,
                mark_style,
                pivot_value);
    }
}// namespace ImGuiKnobs
