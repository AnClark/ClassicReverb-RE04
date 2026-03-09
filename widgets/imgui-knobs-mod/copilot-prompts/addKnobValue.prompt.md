---
name: addImGuiWidgetOverlay
description: Add a new visual overlay layer to an existing custom ImGui widget.
argument-hint: Description of the desired overlay feature (e.g., "scale marks with text labels around a knob")
---

I have a custom ImGui widget implemented in C++ (header + source file pair). I want to add a new **visual overlay feature** on top of the existing widget without breaking backward compatibility.

## Steps to follow

1. **Review the existing implementation**
   - Read the widget's header and source files.
   - Identify how the widget currently draws its visuals (draw calls, variants, helper structs).
   - Check whether any existing drawing code overlaps with the requested feature; evaluate whether to reuse, extend, or add independently.

2. **Propose an implementation plan** before writing any code:
   - Describe the new data structures (if any) needed to configure the overlay.
   - Describe the new drawing method(s) to be added, including coordinate/size conventions.
   - Describe how the new parameters will be exposed in the public API (default values, backward compatibility).
   - Highlight any edge cases (e.g., logarithmic mapping, zero-crossing, color sentinel values).

3. **Wait for confirmation**, then implement:
   - Add new structs/enums to the header, placed logically near related types.
   - Add the drawing method inside the internal widget struct in the source file.
   - Update `BaseKnob` / the main render function to invoke the overlay after all variants.
   - Update all public API function signatures with the new optional parameters (defaulting to `nullptr`/`0`/`false` for full backward compatibility).
   - Apply all independent edits simultaneously for efficiency.

4. **Verify** by triggering a build and confirming zero errors and zero new warnings.

## Constraints

- All new parameters must have **default values** so existing call sites compile unchanged.
- New drawing code must be a **separate layer** appended after the existing variant switch — do not modify variant internals.
- Sizes/radii must use the widget's established **relative-to-radius** convention.
- Sentinel values (e.g., `(0,0,0,0)` for "use theme color") should be documented in the struct.

## Feature to add

$ARGUMENTS
