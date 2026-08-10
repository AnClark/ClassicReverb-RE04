#ifndef CLASSIC_REVERB_UI_H
#define CLASSIC_REVERB_UI_H

#include <string>
#include <queue>
#include <mutex>

#include "DistrhoUI.hpp"
#include "FileBrowserDialog.hpp"  // DPF cross-platform file browser API
#include "Structures.h"
#include "PresetManager.h"

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
    // State Callbacks (DPF WANT_STATE)

    void stateChanged(const char* key, const char* value) override;

    // -------------------------------------------------------------------
    // ImGui Callbacks

    void onImGuiDisplay() override;


private:
    // -------------------------------------------------------------------
    // Local variables

    float fParams[kParamCount];

    bool fAboutWindowOpened; // Flag to track if the "About" window is open
    int  fLastMouseCursor;   // To track the last mouse cursor state for optimization

    // -------------------------------------------------------------------
    // Preset Manager UI state

    // Dialog mode enum (for modal popups inside the preset manager overlay)
    enum class PmDialogMode { None, SaveNew, Rename, ConfirmDelete, ConfirmUpdate };

    bool              fPresetManagerOpened  = false;
    PmDialogMode      fPmDialogMode         = PmDialogMode::None;
    char              fPmNameBuffer[128]    = {};   // text input for Save As / Rename dialogs

    // Buffered values for atomic state restoration from stateChanged() callbacks
    std::string fRestoredPresetType = "Factory";
    std::string fRestoredPresetName;
    bool        fRestoredModified   = false;

    // -------------------------------------------------------------------
    // Internal procedures

    void _loadFonts();  // Load ImGui fonts (invoked in constructor)
    void _drawChassisBackground(float margin, float rounding); // Draw the background of the plugin chassis (called from onImGuiDisplay)
    void _drawKjaerhusLogo(const ImVec2& size);
    void _drawPluginName();

    void _addKnob(Parameters paramId, const char* label, float v_min, float v_max, const ImGuiKnobs_Mod::KnobScaleMark *marks, uint32_t mark_count, bool isLogarithmic = false, bool use_pivot = false, float pivot_value = 0.0f); // Helper function to add a knob with given parameters (called from onImGuiDisplay)
    inline void _addKnob(Parameters paramId, const char* label, const ImGuiKnobs_Mod::KnobScaleMark *marks, uint32_t mark_count, bool isLogarithmic = false, bool use_pivot = false, float pivot_value = 0.0f)
    {
        // This variant of _addKnob uses the predefined parameter ranges from kParamRanges,
        // so you only need to specify the paramId and it will automatically use the correct min/max values.
        _addKnob(paramId, label, kParamRanges[paramId].min, kParamRanges[paramId].max, marks, mark_count, isLogarithmic, use_pivot, pivot_value);
    }

    bool _BeginSection(const char* title, float width); // Helper function to begin a new section with a centered title.
    void _EndSection(); // Helper function to end a section started with _BeginSection.
    void _UpdateMouseCursor(); // Update the OS mouse cursor based on the current ImGui mouse cursor state (called from onImGuiDisplay)

    // Preset Manager related procedures
    void _drawPresetManager(); // Draw the preset manager overlay window (implemented in PresetManagerUI.cpp)
    void _applyRestoredPresetState(); // Restore preset context from buffered stateChanged() values

    // -------------------------------------------------------------------
    // Instances

    ScopedPointer<PresetManager> fPresetManager;
    friend class PresetManager;

    // -------------------------------------------------------------------
    // File browser stuff (DPF cross-platform native file dialog)

    // Definitions & states
    enum class FileBrowserAction { None, Import, Export };
    DGL_NAMESPACE::FileBrowserHandle fFileBrowserHandle = nullptr;  // nullptr = no dialog open
    FileBrowserAction                fFileBrowserAction = FileBrowserAction::None;

    // Poll native file dialog each frame; process result when dialog closes
    void _handleFileBrowserIdle();  // Should be called from onImGuiDisplay() to handle file browser state and results

    // -------------------------------------------------------------------
    // Message box stuff

    // Definitions & states
    std::queue<std::string> fMessageBoxQueue; // Queue of messages to be shown in message boxes
    bool                    fRequestMessagePopup = false; // Trigger flag to indicate that a message box popup should be displayed
    std::mutex              fMessageQueueMutex; // Mutex to protect access to the message box queue

    // Poll message queue and show message box when needed.
    void _handleMessageBoxIdle();   // Should be called from onImGuiDisplay().
    inline void _showMessageBox(const std::string& message)
    {
        // Apply a mutex to avoid possible conflict
        std::lock_guard<std::mutex> lock(fMessageQueueMutex);

        fMessageBoxQueue.push(message);
        fRequestMessagePopup = true;
    }

    // -------------------------------------------------------------------
    // Dear ImGui helper stuff

    float fScaleFactor { 1.0f };    // Local cache of current scale factor
                                    // (should be updated in constructor)

    // A macro-like function to convert absolute values to DPI-scaled value.
    // NOTICE: Dear ImGui itself does not support implicitly apply DPI scaling
    //         on absolute size / pos values. We have to convert by ourselves.
    inline float SCALE(float value)
    {
        return value * fScaleFactor;
    }

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClassicReverbUI)
};

// -----------------------------------------------------------------------



#endif // CLASSIC_REVERB_UI_H
