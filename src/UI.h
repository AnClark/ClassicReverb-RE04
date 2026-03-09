#ifndef CLASSIC_REVERB_UI_H
#define CLASSIC_REVERB_UI_H

#include "DistrhoUI.hpp"
#include "Structures.h"

// Forward decls.
namespace ImGuiKnobs_Mod {
    struct KnobScaleMark;
}

// -----------------------------------------------------------------------

class ClassicReverbUI : public DISTRHO::UI
{
public:
    ClassicReverbUI();

protected:
    // -------------------------------------------------------------------
    // DSP Callbacks

    void parameterChanged(uint32_t index, float value) override;

    // -------------------------------------------------------------------
    // ImGui Callbacks

    void onImGuiDisplay() override;


private:
    // -------------------------------------------------------------------
    // Local variables

    float fParams[kParamCount];

    // -------------------------------------------------------------------
    // Internal procedures

    void _loadFonts();  // Load ImGui fonts (invoked in constructor)
    void _drawChassisBackground(float margin, float rounding); // Draw the background of the plugin chassis (called from onImGuiDisplay)
    void _addKnob(Parameters paramId, const char* label, float v_min, float v_max, const ImGuiKnobs_Mod::KnobScaleMark *marks, uint32_t mark_count, bool isLogarithmic = false, bool use_pivot = false, float pivot_value = 0.0f); // Helper function to add a knob with given parameters (called from onImGuiDisplay)

    bool _BeginSection(const char* title, float width); // Helper function to begin a new section with a centered title.
    void _EndSection(); // Helper function to end a section started with _BeginSection.
    
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClassicReverbUI)
};

// -----------------------------------------------------------------------



#endif // CLASSIC_REVERB_UI_H
