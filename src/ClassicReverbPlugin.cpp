#include "ClassicReverbPlugin.h"
#include "Defines.h"

// ─────────────────────────────────────────────────────────────────────────────
// Classic Reverb plugin class
// ─────────────────────────────────────────────────────────────────────────────

ClassicReverbPlugin::ClassicReverbPlugin()
    : DISTRHO::Plugin(kParamCount, 0, 0)
{
    // Default parameter values – physical units matching initParameter() ranges
    fParams[kParamRoomSize] = 80.0f;    // m²
    fParams[kParamDamping]  = 40.0f;    // %
    fParams[kParamPreDelay] = 0.0f;     // ms
    fParams[kParamHiDamp]   = 30.0f;    // %
    fParams[kParamLoCut]    = 80.0f;    // Hz
    fParams[kParamEarlyRef] = -6.0f;    // dB
    fParams[kParamMix]      = 35.0f;    // %
    fParams[kParamLevel]    = 0.0f;     // dB

    // Call sampleRateChanged() to allocate buffers based on the initial sample rate.
    // This ensures that the plugin is ready to process audio immediately after construction.
    sampleRateChanged(getSampleRate());
}

// ── Parameters ────────────────────────────────────────────────────────
void ClassicReverbPlugin::initParameter(uint32_t index, Parameter& param)
{
    param.hints = kParameterIsAutomatable;
    param.ranges = DISTRHO::ParameterRanges(kParamRanges[index]);

    switch (index)
    {
    case kParamRoomSize:
        param.name = "Room Size";
        param.symbol = "room_size";
        param.unit  = "m^2";
        param.hints |= kParameterIsLogarithmic;
        // Override: use actual physical unit
        break;
    case kParamDamping:
        param.name = "Damping";
        param.symbol = "damping";
        param.unit  = "%";
        break;
    case kParamPreDelay:
        param.name = "Pre-delay";
        param.symbol = "pre_delay";
        param.unit  = "ms";
        break;
    case kParamHiDamp:
        param.name = "Hi-Damp.";
        param.symbol = "hi_damp";
        param.unit  = "%";
        param.ranges.max = 100.0f;
        param.ranges.def = 30.0f;
        break;
    case kParamLoCut:
        param.name = "Lo-Cut";
        param.symbol = "lo_cut";
        param.unit  = "Hz";
        param.hints |= kParameterIsLogarithmic;
        break;
    case kParamEarlyRef:
        param.name = "Early Ref.";
        param.symbol = "early_ref";
        param.unit  = "dB";
        break;
    case kParamMix:
        param.name = "Mix";
        param.symbol = "mix";
        break;
    case kParamLevel:
        param.name = "Level";
        param.symbol = "level";
        param.unit  = "dB";
        break;
    }
}

float ClassicReverbPlugin::getParameterValue(uint32_t index) const
{
    return fParams[index];
}

void ClassicReverbPlugin::setParameterValue(uint32_t index, float value)
{
    fParams[index] = std::clamp(value, kParamRanges[index].min,
                                        kParamRanges[index].max);
    updateCoefficients();
}


// ── Audio processing ──────────────────────────────────────────────────
void ClassicReverbPlugin::activate()
{
    // NOTICE: No need to invoke sampleRateChanged() here to allocate buffers.
    //         Hosts are expected to call sampleRateChanged() before processing and on sample rate changes.
    //         If we invoked sampleRateChanged() here, we may hear a click on play start in hosts (for example, REAPER).
}

void ClassicReverbPlugin::sampleRateChanged(double newSampleRate)
{
    fSampleRate = (float)newSampleRate;
    allocateBuffers();
    updateCoefficients();
}



// ── Buffer allocation ─────────────────────────────────────────────────
void ClassicReverbPlugin::allocateBuffers()
{
    const float sr = fSampleRate;

    // Early reflection buffer: needs to hold up to kErDelay[6] at current SR
    int maxErLen = (int)(kErDelay[6] * sr) + 4;
    fErBufL.reset(maxErLen + 1);
    fErBufR.reset(maxErLen + 1);
    for (int t = 0; t < 7; ++t)
    {
        fErDelayLen[t] = std::max(1, (int)(kErDelay[t] * sr));
        // Safety: clamp to buffer size so read() never goes out of bounds
        fErDelayLen[t] = std::min(fErDelayLen[t], fErBufL.size - 1);
    }

    // Allpass buffers
    for (int a = 0; a < 3; ++a)
    {
        int len = std::max(1, (int)(kApDelay[a] * sr));
        fAP[a].reset(len);
    }

    // Comb filter buffers (maximum size per delay when room=1)
    // The actual delay used is: round(kCombDelay[c] * roomScale * sr)
    // roomScale = kRoomSizeBase * pow(2, param_norm * log2(kRoomSizePower * ...))
    // For maximum sizing we use room=1
    for (int c = 0; c < 16; ++c)
    {
        // At param_bc = 1.0 (max room):
        // roomCoeff = kDampingCoeffB + kDampingCoeffA * (1 - 1.0) = kDampingCoeffB = 0.4
        // That's the feedback coefficient, not the delay length.
        // The delay length is scaled by: roomScale = kRoomSizeBase * (0.968^(param_bc))
        // At max size param_bc is the "room size" normalised.
        // In original code: delay = round(kCombDelay[c] * roomScale * sampleRate)
        // where roomScale = pow(32, roomSizeNorm) and roomSizeNorm in [0,1]
        // Max = pow(32, 1.0) * max_base_delay * sr
        int maxLen = (int)(kCombDelay[c] * 32.0f * sr) + 8;
        if (maxLen > kMaxCombSamples) maxLen = kMaxCombSamples;
        fCombBuf[c].reset(maxLen);
        fCombDampState[c] = 0.0f;
    }

    // Pre-delay: always allocate the full buffer; delay length is stored
    // separately in fPreDelaySamples and passed per-call in run().
    fPreDelay.reset();
    fPreDelaySamples = 0;

    // Lo-Cut biquad state
    fLoCutXL[0] = fLoCutXL[1] = fLoCutYL[0] = fLoCutYL[1] = 0.0f;
    fLoCutXR[0] = fLoCutXR[1] = fLoCutYR[0] = fLoCutYR[1] = 0.0f;
}


// ─────────────────────────────────────────────────────────────────────────────
// Entry point
// ─────────────────────────────────────────────────────────────────────────────

START_NAMESPACE_DISTRHO

Plugin* createPlugin()
{
    return new ClassicReverbPlugin();
}

END_NAMESPACE_DISTRHO
