#ifndef CLASSIC_REVERB_PRESET_H
#define CLASSIC_REVERB_PRESET_H

#include <string>
#include <vector>
#include <cstdint>
#include "Structures.h"

// Forward decls.
class ClassicReverbUI;

// ── DPF State keys ──────────────────────────────────────────────────────────
#define STATE_PRESET_NAME     "preset_name"
#define STATE_PRESET_MODIFIED "preset_modified"
#define STATE_PRESET_TYPE     "preset_type"

// ── Preset type ──────────────────────────────────────────────────────────────
enum class PresetType { Factory, User, Imported };

// ── Preset data structure ────────────────────────────────────────────────────
struct Preset
{
    std::string name;
    float roomSize  = kParamRanges[kParamRoomSize].def;
    float damping   = kParamRanges[kParamDamping].def;
    float preDelay  = kParamRanges[kParamPreDelay].def;
    float hiDamp    = kParamRanges[kParamHiDamp].def;
    float loCut     = kParamRanges[kParamLoCut].def;
    float earlyRef  = kParamRanges[kParamEarlyRef].def;
    float mix       = kParamRanges[kParamMix].def;
    float level     = kParamRanges[kParamLevel].def;

    inline std::string getUniqueButtonID() const {
        // Generate a unique ID for ImGui buttons based on the preset name.
        // This is necessary because ImGui buttons with the same label would
        // otherwise share state (e.g., hover, active), which we don't want.
        //
        // We don't allow duplicate names in the user presets, but users may
        // do nasty things with the config file, and imported preset may have
        // the same name as an existing preset, so we append a unique suffix
        // based on the pointer value.
        return name + "##" + std::to_string(reinterpret_cast<std::uintptr_t>(this));
    }
};

// ── PresetManager ────────────────────────────────────────────────────────────
// Handles all preset business logic; does NOT render any UI.
class PresetManager
{
public:
    explicit PresetManager(ClassicReverbUI* ui);
    ~PresetManager() = default;

    // ── Factory presets (read-only) ────────────────────────────────────────
    int           factoryPresetCount()     const;
    const Preset& factoryPreset(int index) const;

    // ── User presets ───────────────────────────────────────────────────────
    int                        userPresetCount()     const;
    const Preset&              userPreset(int index) const;
    const std::vector<Preset>& userPresets()         const { return fUserPresets; }

    bool loadUserPresetsFromDisk();
    bool saveUserPresetsToDisk();

    // ── Preset selection (applies parameters to the plugin) ────────────────
    void selectFactoryPreset(int index);
    void selectUserPreset(int index);
    void selectImportedPreset(); // make the in-memory imported preset "current"

    // Reset all parameters to their compiled-in default values
    void loadDefaultPreset();

    // ── User preset CRUD operations ────────────────────────────────────────
    // Check whether a name already exists in the user list
    bool nameExists(const std::string& name) const;
    // Save current UI parameters as a new user preset; returns false if name already exists
    bool saveAsNew(const std::string& name);
    // Overwrite the current user preset with current UI parameters
    bool overwriteCurrent();
    // Delete the current user preset
    bool deleteCurrent();
    // Rename the current user preset
    bool renameCurrent(const std::string& newName);

    // ── Import / Export ────────────────────────────────────────────────────
    // Import: load a single-preset JSON file into fImportedPreset; does NOT write to disk
    bool importFromFile(const std::string& filePath);
    // Export: write the current preset to a JSON file
    bool exportCurrentToFile(const std::string& filePath);
    // Commit: save the imported preset as a regular user preset (writes to disk)
    bool commitImported(const std::string& name);

    // Returns true if a preset has been imported into memory for this instance
    bool hasImported() const;
    // Returns a pointer to the in-memory imported preset, or nullptr if none
    const Preset* importedPreset() const;

    // ── State accessors ────────────────────────────────────────────────────
    const Preset* currentPreset() const; // nullptr if nothing is selected
    PresetType    currentType()   const { return fCurrentType; }
    int           currentIndex()  const { return fCurrentIndex; }
    bool          isModified()    const { return fModified; }

    void markModified();  // called when the user tweaks a knob
    void clearModified();

    // Push preset metadata to DPF plugin state so the host can save / restore it.
    // Full overload: atomically sets type/index/modified and emits state.
    void syncPluginState(PresetType type, int index, bool modified);
    // Convenience overload: keeps current type/index, only updates modified flag.
    void syncPluginState(bool modified);

    // Capture current UI parameter values into a Preset struct (name is left empty)
    Preset snapshotFromUI() const;

    // Restore type/name from previously saved DPF state strings
    // (called from ClassicReverbUI::stateChanged)
    void restoreFromState(const std::string& typeStr,
                          const std::string& nameStr,
                          bool modified);

private:
    ClassicReverbUI*    fUI           = nullptr;
    std::vector<Preset> fUserPresets;
    Preset              fImportedPreset;
    PresetType          fCurrentType  = PresetType::Factory;
    int                 fCurrentIndex = -1; // -1 = Default (no specific preset selected)
    bool                fModified     = false;

    void _applyPreset(const Preset& preset);
    void _triggerParamUpdate(uint32_t index, float value);

    std::string _getUserDataDir()         const;
    std::string _getUserPresetsFilePath() const;
    bool        _ensureDataDirExists()    const;
};

#endif // CLASSIC_REVERB_PRESET_H
