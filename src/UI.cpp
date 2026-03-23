#include "UI.h"

#include "CenteredSeparatorText.hpp"
#include "imgui-knobs.h"
#include "HardwareButton.hpp"

#include "config.h"

// -----------------------------------------------------------------------
// Configurations

static const ImGuiKnobs_Mod::KnobScaleMark kSizeMarks[] = {
    {   0.625f, "0.625" },
    {   1.25f,  "1.25"  },
    {   2.5f,   "2.5"   },
    {   5.0f,   "5"     },
    {  10.0f,   "10"    },
    {  20.0f,   "20"    },
    {  40.0f,   "40"    },
    {  80.0f,   "80"    },
    { 160.0f,   "160"   },
    { 320.0f,   "320"   },
    { 640.0f,   "640"   },
};

static const ImGuiKnobs_Mod::KnobScaleMark kDampingMarks[] = {
    {   0.0f, "MIN" },
    {   12.5f, nullptr },
    {   25.0f, nullptr },
    {   37.5f, nullptr },
    {   50.0f, nullptr },
    {   62.5f, nullptr },
    {   75.0f, nullptr },
    {   87.5f, nullptr },
    {  100.0f, "MAX" },
};

static const ImGuiKnobs_Mod::KnobScaleMark kPreDelayMarks[] = {
    { -150.0f, "-150" },
    { -120.0f, "-120" },
    {  -90.0f, "-90"  },
    {  -60.0f, "-60"  },
    {  -30.0f, "-30"  },
    {    0.0f, "0"    },
    {   30.0f, "30"   },
    {   60.0f, "60"   },
    {   90.0f, "90"   },
    {  120.0f, "120" },
    {  150.0f, "150" },
};

static const ImGuiKnobs_Mod::KnobScaleMark kLoCutMarks[] = {
    { 20.0f, "20" },
    { 30.0f, "30" },
    {  44.0f, "44"  },
    {  65.0f, "65"  },
    {  96.0f, "96"  },
    {  141.0f, "141" },
    {  209.0f, "209"   },
    {  309.0f, "309"   },
    {  457.0f, "457"   },
    {  676.0f, "676"   },
    { 1000.0f, "1k"  },
};

static const ImGuiKnobs_Mod::KnobScaleMark kEarlyRefMarks[] = {
    { -40.0f, "-\u221e" },   // -40 dB → displayed as -∞
    { -30.0f, nullptr  },
    { -20.0f, nullptr  },
    { -10.0f, nullptr  },
    {   0.0f,  "0"     },   // pivot: knob centre
    {   1.5f,  nullptr },
    {   3.0f,  nullptr },
    {   4.5f,  nullptr },
    {   6.0f,  "+6"    },
};

static const ImGuiKnobs_Mod::KnobScaleMark kMixMarks[] = {
    {   0.0f, "DIR." },
    {   12.5f, nullptr },
    {   25.0f, nullptr },
    {   37.5f, nullptr },
    {   50.0f, "1:1" },
    {   62.5f, nullptr },
    {   75.0f, nullptr },
    {   87.5f, nullptr },
    {  100.0f, "EFF." },
};

static const ImGuiKnobs_Mod::KnobScaleMark kLevelMarks[] = {
    {  -10.0f, "-10" },
    {  -7.5f, nullptr },
    {   -5.0f, nullptr },
    {   -2.5f, nullptr },
    {    0.0f, "0"   },
    { 2.5f, nullptr },
    {   5.0f, nullptr },
    {   7.5f, nullptr },
    {  10.0f, "10"  },
};

// -----------------------------------------------------------------------
// Constructor and UI callbacks

ClassicReverbUI::ClassicReverbUI()
    : DISTRHO::UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT, true)
{
    // Initialize parameters to default values (optional)
    std::memset(fParams, 0, sizeof(fParams));

    // Initialize preset manager and load persisted user presets from disk
    fPresetManager = new PresetManager(this);
    const bool presetsLoaded = fPresetManager->loadUserPresetsFromDisk();
    if (!presetsLoaded) {
        // NOTE: _showMessageBox() can be used here because it pushes the message into a queue and doesn't require an active ImGui context at this point.
        //       The message will be displayed as a popup when the UI is rendered.
        //       @see _showMessageBox() and _handleMessageBoxIdle() in UI.h/UI.cpp
        _showMessageBox("WARNING: could not load user presets from disk. Presets will not be saved.");
    }

    // Load fonts for ImGui
    _loadFonts();

    // Set the flag to track if the "About" window is open
    fAboutWindowOpened = false;
}

void ClassicReverbUI::parameterChanged(uint32_t index, float value)
{
    DISTRHO_SAFE_ASSERT_RETURN(index < kParamCount, )

    fParams[index] = value;
}

void ClassicReverbUI::onImGuiDisplay()
{
    //const float scale   = getScaleFactor();
    const float margin  = 4.0f; //* scale;

    // ── Main viewport (fullscreen, no decoration) ────────────────────────────
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);

    static constexpr auto window_flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove       |
        ImGuiWindowFlags_NoSavedSettings;

    // White background for the host window
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

    if (ImGui::Begin("Main Window", nullptr, window_flags))
    {
        const float  rounding = 10.0f; //* scale;
        const ImVec2 winSize = ImGui::GetWindowSize();

        // ── Draw the plugin chassis background directly onto the main viewport ──
        _drawChassisBackground(margin, rounding);

        // ── Child window (transparent overlay for placing controls) ──────────
        ImGui::SetCursorPos(ImVec2(margin, margin));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, rounding);

        const ImVec2 childSize = ImVec2(winSize.x - 2.0f * margin, winSize.y - 2.0f * margin);
        if (ImGui::BeginChild("BackgroundPanel", childSize, false,
                              ImGuiWindowFlags_NoScrollbar |
                              ImGuiWindowFlags_NoScrollWithMouse))
        {
            // ── UI controls will go here ─────────────────────────────────────
            
            // Left margin
            ImGui::Dummy(ImVec2(2, 0));
            ImGui::SameLine();

            if (_BeginSection("REVERBERATION", (90.0f - 4.0f) * 3))        
            {
                // Add an extra left margin to the first knob so its leftmost scale mark doesn't get cut off.
                ImGui::Dummy(ImVec2(12, 0));
                ImGui::SameLine();

                _addKnob(kParamRoomSize, "SIZE (m²)", kSizeMarks, IM_ARRAYSIZE(kSizeMarks), true);

                ImGui::SameLine(0, 35);

                _addKnob(kParamDamping, "DAMPING", kDampingMarks, IM_ARRAYSIZE(kDampingMarks));

                ImGui::SameLine(0, 35);

                _addKnob(kParamPreDelay, "PREDELAY (ms)", kPreDelayMarks, IM_ARRAYSIZE(kPreDelayMarks), false);

                _EndSection();              
            }

            ImGui::SameLine(0, 10);

            if (_BeginSection("FILTERS", (90.0f - 6.0f) * 2))
            {
                // Add an extra left margin
                ImGui::Dummy(ImVec2(2, 0));
                ImGui::SameLine();

                _addKnob(kParamHiDamp, "HI DAMP.", kDampingMarks, IM_ARRAYSIZE(kDampingMarks));

                ImGui::SameLine(0, 35);

                _addKnob(kParamLoCut, "LO CUT (Hz)", kLoCutMarks, IM_ARRAYSIZE(kLoCutMarks), true);

                _EndSection();
            }

            ImGui::SameLine(0, 10);

            if (_BeginSection("OUTPUT", (80.0f - 2.0f) * 3))        
            {
                // Add an extra left margin
                ImGui::Dummy(ImVec2(1, 0));
                ImGui::SameLine();
  
                _addKnob(kParamEarlyRef, "EARLY REF. (dB)", kEarlyRefMarks, IM_ARRAYSIZE(kEarlyRefMarks),
                         false,   // isLogarithmic
                         true,    // use_pivot: knob centre = 0 dB
                         0.0f);   // pivot_value

                ImGui::SameLine(0, 20 - 5);

                _addKnob(kParamMix, "MIX", kMixMarks, IM_ARRAYSIZE(kMixMarks));

                ImGui::SameLine(0, 30);

                _addKnob(kParamLevel, "LEVEL", kLevelMarks, IM_ARRAYSIZE(kLevelMarks));

                _EndSection();
            }

            ImGui::SameLine(0, 10.0f);

            // Right panel (Logo, config buttons, etc.)
            {
                ImGui::BeginGroup();

                // Add an extra top margin
                ImGui::Dummy(ImVec2(0, 2));

                _drawKjaerhusLogo(ImVec2(100, 50));

#if 1   // Extra controls.
        // TODO: Make Preset Manager an optional feature
                // Preset button — label shows the current preset name
                {
                    ImGui::BeginGroup();
                    ImGui::AlignTextToFramePadding();

                    ImGui::Dummy(ImVec2(2, 0));
                    ImGui::SameLine(0.0f, 0.0f);

                    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);   // Use the smaller font for the preset button

                    {
                        const Preset* curPreset = fPresetManager->currentPreset();
                        std::string   btnLabel;
                        if (curPreset) {
                            btnLabel = curPreset->name;
                            if (fPresetManager->isModified()) btnLabel += " *";
                        } else {
                            btnLabel = "Select Preset...";
                        }
                        btnLabel += "##Preset";
                        if (ImGuiExt::HardwareButton(btnLabel.c_str(),
                                           ImVec2(100 - 3, ImGui::GetFrameHeight()),
                                           ImVec4(0x2f / 255.0f, 0x4d / 255.0f, 0x44 / 255.0f, 1.0f)))
                        {
                            fPresetManagerOpened = !fPresetManagerOpened;
                        }
                    }
                    ImGui::SameLine(0.0f, 5.0f);
                    ImGui::Text("PRESET");

                    ImGui::PopFont();

                    ImGui::EndGroup();
                }

                ImGui::Dummy(ImVec2(0, 0.5f));

#else   // No extra controls, just a placeholder
                ImGui::Dummy(ImVec2(0, 23));     // TODO: This is a placeholder. I will add extra controls here in future.
#endif

                _drawPluginName();

                ImGui::EndGroup();
            }

        }
        ImGui::EndChild();

        ImGui::PopStyleVar();   // ChildRounding
        ImGui::PopStyleColor(); // ChildBg

        ImGui::End();
    }
    ImGui::PopStyleColor(); // WindowBg

    // ── "About" window (fullscreen) ───────────────────────────────────────────────
    static constexpr auto about_window_flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoMove       |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_AlwaysAutoResize;

    if (fAboutWindowOpened)
    {
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);

        if (ImGui::Begin("About Window", &fAboutWindowOpened, about_window_flags))
        {
            {
                ImGui::Columns(2, "AboutColumns", false);
                ImGui::SetColumnWidth(0, 400.0f - 5.0f);
                ImGui::SetColumnWidth(1, 420.0f - 15.0f);

                {
                    const String versionStr = String("Classic Reverb RE-04") + "  |  Version " +
                                        String(VERSION_MAJOR) + "." +
                                        String(VERSION_MINOR) + "." +
                                        String(VERSION_PATCH);

                    ImGui::SeparatorText(versionStr);
                    ImGui::Text("Reverse engineering of Kjaerhus Audio Classic Reverb (2003).");
                    ImGui::Text("Original algorithm by Kjaerhus Audio.");
                    ImGui::Text("Copyright (c) 2026 AnClark Liu <clarklaw4701@qq.com>");
                    
                    ImGui::SeparatorText("License: GNU General Public License v3.0 or later");
                    ImGui::Dummy(ImVec2(0, 2));
                    ImGui::TextWrapped("Classic Reverb RE-04 is free software: "
                                            "you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation,"
                                            "either version 3 of the License, or (at your option) any later version.");
                }

                ImGui::NextColumn();

                {
                    ImGui::SeparatorText("Disclaimer");
                    ImGui::TextWrapped("This is an unofficial, reverse-engineered clone of the discontinued Kjaerhus Classic Reverb, aiming at bringing"
                                            "this vintage and fantastic plugin to life again.");
                    ImGui::TextWrapped("This project is NOT related to official Kjaerhus Audio, Acoustica LLC. and their affiliates.");
                    ImGui::Dummy(ImVec2(0, 2));
                    ImGui::TextWrapped("The Kjaerhus logo is used under fair use for identification purposes only, "
                                            "and is not intended to infringe any trademarks.");
                    ImGui::Dummy(ImVec2(0, 2));
                    ImGui::TextWrapped("VST is a trademark of Steinberg GmbH.");
                }

                ImGui::Columns(1);
            }

            {
                ImGui::BeginGroup();
                
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
                ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(0x2f, 0x4d, 0x44, 0xff));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(0x2f + 20, 0x4d + 20, 0x44 + 20, 0xff));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(0x2f + 40, 0x4d + 40, 0x44 + 40, 0xff));

                // Fixed position OK button at bottom-right (screen coordinates)
                static constexpr ImVec2 button_size = ImVec2(60 - 5, 25);
                ImVec2 buttonPos = ImVec2(viewport->Pos.x + viewport->Size.x - button_size.x - 22.0f,
                                        viewport->Pos.y + viewport->Size.y - button_size.y - 10.0f);
                ImGui::SetCursorScreenPos(buttonPos);
                if (ImGui::Button("OK", button_size))
                {
                    fAboutWindowOpened = false;
                }

                ImGui::PopStyleColor(3);
                ImGui::PopStyleVar(); // FrameRounding

                ImGui::EndGroup();
            }

            ImGui::End();
        }
    }

    // Update the OS mouse cursor based on the current ImGui mouse cursor state
    _UpdateMouseCursor();

    // Poll the native file browser dialog (Import/Export). Must be called every frame.
    _handleFileBrowserIdle();

    // Draw the preset manager overlay (renders nothing when fPresetManagerOpened == false)
    _drawPresetManager();

    // Handle message box display
    _handleMessageBoxIdle();
}

// -----------------------------------------------------------------------
// State callbacks

void ClassicReverbUI::stateChanged(const char* key, const char* value)
{
    // Buffer each restored value; rebuild state after all three arrive.
    if (std::strcmp(key, STATE_PRESET_TYPE) == 0)
        fRestoredPresetType = value;
    else if (std::strcmp(key, STATE_PRESET_NAME) == 0)
        fRestoredPresetName = value;
    else if (std::strcmp(key, STATE_PRESET_MODIFIED) == 0)
        fRestoredModified = (std::strcmp(value, "true") == 0);

    _applyRestoredPresetState();
}

// -----------------------------------------------------------------------

START_NAMESPACE_DISTRHO

UI* createUI()
{
    return new ClassicReverbUI();
}

END_NAMESPACE_DISTRHO

