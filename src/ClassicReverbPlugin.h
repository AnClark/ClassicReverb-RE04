#ifndef CLASSIC_REVERB_PLUGIN_H_INCLUDED
#define CLASSIC_REVERB_PLUGIN_H_INCLUDED

#include "DistrhoPlugin.hpp"
#include "Structures.h"

class ClassicReverbPlugin : public DISTRHO::Plugin
{
public:
    ClassicReverbPlugin();

protected:
    // ── Plugin metadata ────────────────────────────────────────────────────
    const char* getLabel()   const override { return DISTRHO_PLUGIN_NAME; }
    const char* getMaker()   const override { return DISTRHO_PLUGIN_BRAND; }
    const char* getLicense() const override { return "GPLv3+"; }
    uint32_t    getVersion() const override { return d_version(1, 0, 0); }

    // ── Parameters ────────────────────────────────────────────────────────
    void initParameter(uint32_t index, Parameter& param) override;
    float getParameterValue(uint32_t index) const override;
    void setParameterValue(uint32_t index, float value) override;

    // ── Audio processing ──────────────────────────────────────────────────
    void activate() override;
    void sampleRateChanged(double newSampleRate) override;
    void run(const float** inputs, float** outputs, uint32_t frames) override;

private:
    // ── Internal helpers ──────────────────────────────────────────────────

    // Schroeder allpass section
    inline float schroederAllpass(float in, SmallCircBuf& buf, float g)
    {
        float delayed = buf.buf[buf.pos];
        float w       = in + g * delayed;
        buf.buf[buf.pos] = w;
        if (++buf.pos >= buf.size) buf.pos = 0;
        return delayed - g * w;
    }


    // Normalise room-size m² → 0–1 (VST normalised form used by original plugin)
    static float roomSizeToNorm(float sqm)
    {
        // The original plugin stored a normalised value that scaled the comb
        // delay lengths.  A room area of 0.625 m² → norm=0, 640 m² → norm=1.
        // Using simple log2 mapping: norm = (log2(sqm) - log2(0.625)) / (log2(640)-log2(0.625))
        // log2(0.625)≈-0.678, log2(640)≈9.322 → range ~10.0
        float logMin = std::log2f(0.625f);
        float logMax = std::log2f(640.0f);
        float norm   = (std::log2f(sqm) - logMin) / (logMax - logMin);
        return std::clamp(norm, 0.0f, 1.0f);
    }

    // ── Buffer allocation ─────────────────────────────────────────────────
    void allocateBuffers();

    // ── Coefficient update ────────────────────────────────────────────────
    void updateCoefficients();

    // ── State ──────────────────────────────────────────────────────────────
    float fParams[kParamCount];
    float fSampleRate = 44100.0f;

    // Pre-delay
    PreDelayBuf fPreDelay;
    int         fPreDelaySamples = 0;  // current read-offset in samples

    // Early reflections (stereo buffers, matching the original DLL)
    ErBuf fErBufL;
    ErBuf fErBufR;
    int   fErDelayLen[7];

    // Allpass diffusion
    SmallCircBuf fAP[3];

    // Comb filters
    CircBuf fCombBuf[16];
    int     fCombLen[16];
    float   fCombDampState[16];

    // Computed coefficients
    float fRoomCoeff    = 0.6f;
    float fRoomCoeffSq  = 0.8f;
    float fHiDampA      = 1.0f;  // LP pole gain
    float fHiDampB      = 0.0f;  // complement
    float fLevelGain    = 1.0f;
    float fMix          = 0.35f;
    float fEarlyRefGain = 0.5f;

    // Lo-Cut biquad
    float fLoCutB0 = 1.0f, fLoCutB1 = 0.0f, fLoCutB2 = 0.0f;
    float fLoCutA1 = 0.0f, fLoCutA2 = 0.0f;
    float fLoCutXL[2] = {}, fLoCutYL[2] = {};
    float fLoCutXR[2] = {}, fLoCutYR[2] = {};

    // Preset bank
    String fPresetName[33];
    float  fPresetParam[33][kParamCount];

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClassicReverbPlugin)
};

#endif // CLASSIC_REVERB_PLUGIN_H_INCLUDED
