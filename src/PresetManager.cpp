#include "PresetManager.h"
#include "UI.h"
#include "Structures.h"

constexpr Preset kFactoryPresets[] = {
    // Name, Room Size, Damping, Pre-Delay, Hi Damp, Lo Cut, Early Ref, Mix, Level
    {"Grand Hall", 640.0f, 32.0f, 0.0f, 36.0f, 80.0f, 1.60f, 35.0f, 0.0f},
};
constexpr size_t kFactoryPresetsCount = IM_ARRAYSIZE(kFactoryPresets);

void PresetManager::loadDefaultPreset()
{
    DISTRHO_SAFE_ASSERT_RETURN(ui != nullptr, )

    for (uint32_t i = 0; i < kParamCount; ++i)
    {
        _triggerParamUpdate(i, kParamRanges[i].def);   // Assuming default value is 0.0f for all parameters
    }
}

void PresetManager::loadFactoryPreset(uint32_t index)
{
    DISTRHO_SAFE_ASSERT_RETURN(index < kFactoryPresetsCount, )

    _applyPreset(kFactoryPresets[index]);
}

String PresetManager::getFactoryPresetName(uint32_t index)
{
    DISTRHO_SAFE_ASSERT_RETURN(index < kFactoryPresetsCount, String())

    return String(kFactoryPresets[index].name);
}

void PresetManager::_applyPreset(const Preset& preset)
{
    DISTRHO_SAFE_ASSERT_RETURN(ui != nullptr, )

    _triggerParamUpdate(kParamRoomSize, preset.roomSize);
    _triggerParamUpdate(kParamDamping, preset.damping);
    _triggerParamUpdate(kParamPreDelay, preset.preDelay);
    _triggerParamUpdate(kParamHiDamp, preset.hiDamp);
    _triggerParamUpdate(kParamLoCut, preset.loCut);
    _triggerParamUpdate(kParamEarlyRef, preset.earlyRef);
    _triggerParamUpdate(kParamMix, preset.mix);
    _triggerParamUpdate(kParamLevel, preset.level);
}

void PresetManager::_triggerParamUpdate(uint32_t index, float value)
{
    DISTRHO_SAFE_ASSERT_RETURN(index < kParamCount, )

    ui->setParameterValue(index, value);
    ui->parameterChanged(index, value);
}
