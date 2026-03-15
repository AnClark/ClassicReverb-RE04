#ifndef CLASSIC_REVERB_PRESET_H
#define CLASSIC_REVERB_PRESET_H

#include <cstdint>
#include "extra/String.hpp"

// Forward decls.
class ClassicReverbUI;

struct Preset
{
    char name[24];

    float roomSize;
    float damping;
    float preDelay;
    float hiDamp;
    float loCut;
    float earlyRef;
    float mix;
    float level;
};

class PresetManager
{
    ClassicReverbUI *ui;

public:
    PresetManager(ClassicReverbUI *_ui)
        : ui(_ui) {}

    void loadDefaultPreset();
    void loadFactoryPreset(uint32_t index);
    String getFactoryPresetName(uint32_t index);

private:
    void _applyPreset(const Preset& preset);
    void _triggerParamUpdate(uint32_t index, float value);
    // TODO: void _updatePluginState(const char*) { ui->fPresetName = ...; ui->setState(...); }
};

#endif // CLASSIC_REVERB_PRESET_H
