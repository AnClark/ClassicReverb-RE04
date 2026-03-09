---
name: customImGuiWidget
description: Design and implement a reusable Dear ImGui custom widget in C++.
argument-hint: Widget name, visual appearance description, behavioral requirements (e.g., centered text, adjustable width, custom colors)
---
Design and implement a custom Dear ImGui widget in C++ with the following requirements:

- **Widget name**: $WIDGET_NAME
- **Visual appearance**: $APPEARANCE_DESCRIPTION (e.g., similar to an existing ImGui widget such as `ImGui::SeparatorText()`, `ImGui::Button()`, etc.)
- **Behavioral requirements**: $BEHAVIOR_REQUIREMENTS (e.g., centered label, adjustable width, custom color, tooltip support)

Implementation guidelines:
1. Place both the declaration and the inline implementation in a single `.hpp` header file under the appropriate `widgets/` subdirectory.
2. Wrap the widget function inside a custom namespace (e.g., `ImGuiExt`) to avoid name collisions with the core ImGui API.
3. Include `<imgui.h>` and `<imgui_internal.h>` as needed. Use internal ImGui APIs (e.g., `ItemSize`, `ItemAdd`, `RenderTextEllipsis`, `GetColorU32`) to match the look, feel, and behavior of the nearest built-in ImGui widget as closely as possible.
4. Reuse existing ImGui style variables (e.g., `style.SeparatorTextBorderSize`, `style.ItemSpacing`, `ImGuiCol_Separator`) wherever possible so the widget respects the active ImGui theme automatically.
5. Support an optional `width` parameter (default `0.0f` = full available content width) for flexible layout control.
6. Handle edge cases: empty label, zero width, `window->SkipItems`, and text ellipsis clipping.
7. Add a concise doc-comment block at the top of the file describing the widget's purpose, differences from any reference widget, and a usage example.

Provide the complete, compilable header file content.
