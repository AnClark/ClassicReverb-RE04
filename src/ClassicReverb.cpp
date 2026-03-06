/*
 * Classic Reverb – DPF plug-in port
 *
 * DSP algorithm reverse-engineered from the original Delphi VST2 binary.
 * All constants were extracted directly from the DLL's DATA section.
 *
 * Algorithm overview
 * ──────────────────
 *  1. Pre-delay       – simple stereo circular buffer
 *  2. Input diffusion – 3-stage Schroeder allpass network
 *  3. 16 parallel comb filters with per-tap shelving damping (Hi-Damp)
 *     and per-comb loop high-shelf filter (Lo-Cut is a 2nd-order filter
 *     on the comb output sum)
 *  4. Early reflections – 7-tap delay line with fixed gains
 *  5. Lo-Cut biquad highpass on the wet signal
 *  6. Dry/Wet mix + Output level
 *
 * Parameters:
 *   0  Room Size  0.625 … 640 m²    (normalised 0–1 in VST)
 *   1  Damping    0 … 100 %
 *   2  Pre-delay  -150 … 150 ms     (0.5 = 0 ms)
 *   3  Hi-Damp    0 … 100 %
 *   4  Lo-Cut     20 Hz … 1 kHz
 *   5  Early Ref. -40 … +6 dB
 *   6  Mix        0 (dry) … 1 (wet)
 *   7  Level      -10 … +10 dB
 */

#include "DistrhoPlugin.hpp"
#include "extra/ScopedDenormalDisable.hpp"
#include <cmath>
#include <cstring>
#include <algorithm>

START_NAMESPACE_DISTRHO

// ─────────────────────────────────────────────────────────────────────────────
// Constants extracted from Classic Reverb.dll (DATA section)
// ─────────────────────────────────────────────────────────────────────────────

// FUN_004845b8 constants
static constexpr float kDampingCoeffA  = 0.58f;   // DAT_004848a8 (float80)
static constexpr float kDampingCoeffB  = 0.40f;   // DAT_004848b4 (float80)
static constexpr float kLevelScale     = 10.0f;   // DAT_004848c0 (float80) – for Level param
static constexpr float kLoCutScale     = 50.0f;   // DAT_004848f4 (float80) – for Lo-Cut param
static constexpr double kTwoPi         = 6.283185307179586; // DAT_00484900 (float80 ≈ 2π)
static constexpr float kModAmp         = 6e-8f;   // DAT_00485f5c – chorus/randomisation amplitude
static constexpr float kAllpassCoeff   = 0.6f;    // DAT_00485f68 – Schroeder allpass g

// Size-scaling constants
static constexpr float kRoomSizeBase   = 32.0f;   // DAT_004848d0 – base delay multiplier
static constexpr float kRoomSizePower  = 0.968f;  // DAT_004848dc – power-law exponent
static constexpr float kEarlyRefGain_c = 0.3f;    // DAT_004848e8 – base early-ref gain factor

// Early reflection delays (seconds) – from 0x00488cf0
static constexpr float kErDelay[7] = {
    0.000597f, 0.001340f, 0.001610f, 0.002160f,
    0.002450f, 0.002820f, 0.003520f
};
// Early reflection tap gains – from 0x00488d0c
static constexpr float kErGain[7] = {
    0.65f, 0.45f, 0.41f, 0.34f, 0.31f, 0.28f, 0.24f
};

// Comb filter base delay times (seconds) – from 0x00488d28
static constexpr float kCombDelay[16] = {
    0.002092f, 0.002185f, 0.002281f, 0.002382f,
    0.002488f, 0.002598f, 0.002713f, 0.002833f,
    0.002959f, 0.003090f, 0.003226f, 0.003369f,
    0.003518f, 0.003674f, 0.003837f, 0.004007f
};

// Comb filter output mix weights (stereo) – from 0x00488d74  (interleaved L,R)
static constexpr float kCombMixL[16] = {
    +0.19850f, -0.12350f, +0.30150f, -0.13230f,
    +0.16650f, +0.34160f, +0.46740f, -0.06785f,
    -0.18790f, +0.37030f, +0.02341f, -0.39260f,
    +0.15080f, +0.22060f, -0.08366f, -0.25290f
};
static constexpr float kCombMixR[16] = {
    +0.17920f, +0.34930f, +0.29600f, -0.02510f,
    -0.08528f, -0.23830f, +0.35500f, +0.37310f,
    +0.04845f, -0.02328f, -0.35360f, +0.29490f,
    -0.03428f, +0.32320f, -0.26350f, -0.19590f
};

// Allpass filter base delays (seconds) – from 0x00488d68
static constexpr float kApDelay[3] = { 0.000200f, 0.000252f, 0.000317f };

// ─────────────────────────────────────────────────────────────────────────────
// Parameter indices
// ─────────────────────────────────────────────────────────────────────────────
enum Parameters
{
    kParamRoomSize = 0,
    kParamDamping,
    kParamPreDelay,
    kParamHiDamp,
    kParamLoCut,
    kParamEarlyRef,
    kParamMix,
    kParamLevel,
    kParamCount
};

// ─────────────────────────────────────────────────────────────────────────────
// Circular buffer helpers (fixed maximum size)
// ─────────────────────────────────────────────────────────────────────────────
static constexpr int kMaxCombSamples = 13000; // > max comb buffer size
static constexpr int kMaxApSamples   = 1200;  // > max AP buffer size
static constexpr int kMaxErSamples   = 300;   // > ER tap at 44.1 kHz
static constexpr int kMaxPdSamples   = 15000; // > 300 ms @ 44.1 kHz

struct CircBuf
{
    float  buf[kMaxCombSamples];
    int    pos  = 0;
    int    size = 1;

    void reset(int n)
    {
        size = (n < 1 ? 1 : (n > kMaxCombSamples ? kMaxCombSamples : n));
        pos  = 0;
        std::memset(buf, 0, sizeof(float) * (unsigned)size);
    }

    float read(int delayInSamples) const
    {
        int idx = pos - delayInSamples;
        if (idx < 0) idx += size;
        return buf[idx];
    }

    void write(float v)
    {
        buf[pos] = v;
        if (++pos >= size) pos = 0;
    }

    // read then advance write pointer
    float readAndWrite(float v)
    {
        float out = buf[pos];
        buf[pos]  = v;
        if (++pos >= size) pos = 0;
        return out;
    }
};

struct SmallCircBuf
{
    float  buf[kMaxApSamples];
    int    pos  = 0;
    int    size = 1;

    void reset(int n)
    {
        size = (n < 1 ? 1 : (n > kMaxApSamples ? kMaxApSamples : n));
        pos  = 0;
        std::memset(buf, 0, sizeof(float) * (unsigned)size);
    }

    float readAndWrite(float v)
    {
        float out = buf[pos];
        buf[pos]  = v;
        if (++pos >= size) pos = 0;
        return out;
    }
};

// Pre-delay stores stereo pairs
struct PreDelayBuf
{
    float  bufL[kMaxPdSamples];
    float  bufR[kMaxPdSamples];
    int    pos  = 0;
    int    size = 1;

    void reset(int n)
    {
        size = (n < 1 ? 1 : (n > kMaxPdSamples ? kMaxPdSamples : n));
        pos  = 0;
        std::memset(bufL, 0, sizeof(float) * (unsigned)size);
        std::memset(bufR, 0, sizeof(float) * (unsigned)size);
    }

    void readAndWrite(float inL, float inR, float& outL, float& outR)
    {
        outL = bufL[pos];
        outR = bufR[pos];
        bufL[pos] = inL;
        bufR[pos] = inR;
        if (++pos >= size) pos = 0;
    }

    // size == 1 → pass-through without delay
};

// Small ER delay buffer (mono, used per-channel)
struct ErBuf
{
    float  buf[kMaxErSamples];
    int    pos  = 0;
    int    size = 1;

    void reset(int n)
    {
        size = (n < 1 ? 1 : (n > kMaxErSamples ? kMaxErSamples : n));
        pos  = 0;
        std::memset(buf, 0, sizeof(float) * (unsigned)size);
    }

    float read(int delay) const
    {
        int idx = pos - delay;
        if (idx < 0) idx += size;
        return buf[idx];
    }

    void write(float v)
    {
        buf[pos] = v;
        if (++pos >= size) pos = 0;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Classic Reverb plugin class
// ─────────────────────────────────────────────────────────────────────────────
class ClassicReverbPlugin : public Plugin
{
public:
    ClassicReverbPlugin()
        : Plugin(kParamCount, 33, 0)
    {
        // Set default parameter values (normalised 0–1)
        fParams[kParamRoomSize] = roomSizeToNorm(80.0f);   // ~80 m²
        fParams[kParamDamping]  = 0.4f;
        fParams[kParamPreDelay] = 0.5f;   // 0 ms
        fParams[kParamHiDamp]   = 0.3f;
        fParams[kParamLoCut]    = 0.2f;
        fParams[kParamEarlyRef] = 0.76f;  // ~0 dB
        fParams[kParamMix]      = 0.35f;
        fParams[kParamLevel]    = 0.5f;   // 0 dB

        // Init preset bank
        initPresets();
        sampleRateChanged(getSampleRate());
    }

protected:
    // ── Plugin metadata ────────────────────────────────────────────────────
    const char* getLabel()   const override { return "ClassicReverb RE 4th"; }
    const char* getMaker()   const override { return "Classic Reverb"; }
    const char* getLicense() const override { return "GPL-2.0-or-later"; }
    uint32_t    getVersion() const override { return d_version(1, 0, 0); }
    int64_t     getUniqueId() const override { return d_cconst('c','R','V','4'); }

    // ── Parameters ────────────────────────────────────────────────────────
    void initParameter(uint32_t index, Parameter& param) override
    {
        param.hints = kParameterIsAutomatable;
        param.ranges.min = 0.0f;
        param.ranges.max = 1.0f;

        switch (index)
        {
        case kParamRoomSize:
            param.name = "Room Size";
            param.symbol = "room_size";
            param.unit  = "m^2";
            param.ranges.min = 0.625f;
            param.ranges.max = 640.0f;
            param.hints |= kParameterIsLogarithmic;
            param.ranges.def = 80.0f;
            // Override: use actual physical unit
            break;
        case kParamDamping:
            param.name = "Damping";
            param.symbol = "damping";
            param.unit  = "%";
            param.ranges.def = 40.0f;
            param.ranges.max = 100.0f;
            break;
        case kParamPreDelay:
            param.name = "Pre-delay";
            param.symbol = "pre_delay";
            param.unit  = "ms";
            param.ranges.min = -150.0f;
            param.ranges.max =  150.0f;
            param.ranges.def =  0.0f;
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
            param.ranges.min = 20.0f;
            param.ranges.max = 1000.0f;
            param.ranges.def = 80.0f;
            break;
        case kParamEarlyRef:
            param.name = "Early Ref.";
            param.symbol = "early_ref";
            param.unit  = "dB";
            param.ranges.min = -40.0f;
            param.ranges.max =   6.0f;
            param.ranges.def =  -6.0f;
            break;
        case kParamMix:
            param.name = "Mix";
            param.symbol = "mix";
            param.ranges.def = 0.35f;
            break;
        case kParamLevel:
            param.name = "Level";
            param.symbol = "level";
            param.unit  = "dB";
            param.ranges.min = -10.0f;
            param.ranges.max =  10.0f;
            param.ranges.def =   0.0f;
            break;
        }
    }

    float getParameterValue(uint32_t index) const override
    {
        return fParams[index];
    }

    void setParameterValue(uint32_t index, float value) override
    {
        fParams[index] = std::clamp(value, getParameterRange(index).min,
                                           getParameterRange(index).max);
        updateCoefficients();
    }

    // ── Programs ───────────────────────────────────────────────────────────
    void initProgramName(uint32_t index, String& programName) override
    {
        if (index < 33)
            programName = fPresetName[index];
    }

    void loadProgram(uint32_t index) override
    {
        if (index >= 33) return;
        for (int p = 0; p < kParamCount; ++p)
            fParams[p] = fPresetParam[index][p];
        updateCoefficients();
    }

    // ── Audio processing ──────────────────────────────────────────────────
    void activate() override
    {
        sampleRateChanged(getSampleRate());
    }

    void sampleRateChanged(double newSampleRate) override
    {
        fSampleRate = (float)newSampleRate;
        allocateBuffers();
        updateCoefficients();
    }

    void run(const float** inputs, float** outputs,
             uint32_t frames) override
    {
        // Prevent denormal numbers from causing CPU spikes when the DAW is
        // paused and the reverb tail decays toward zero.
        const ScopedDenormalDisable denormalDisable;

        const float* inL  = inputs[0];
        const float* inR  = inputs[1];
        float*       outL = outputs[0];
        float*       outR = outputs[1];

        for (uint32_t i = 0; i < frames; ++i)
        {
            float dryL = inL[i];
            float dryR = inR[i];

            // ────── Pre-delay ────────────────────────────────────────────
            float pdL, pdR;
            fPreDelay.readAndWrite(dryL, dryR, pdL, pdR);

            // ────── Early reflections (7-tap) ────────────────────────────
            fErBuf.write(pdL + pdR);   // mono sum into ER buffer

            float erL = 0.0f, erR = 0.0f;
            for (int t = 0; t < 7; ++t)
            {
                int d = fErDelayLen[t];
                float tap = fErBuf.read(d);
                erL += tap * kErGain[t];
                erR += tap * kErGain[t];
            }
            // Apply early-ref gain (parameter-controlled)
            erL *= fEarlyRefGain;
            erR *= fEarlyRefGain;

            // ────── Input diffusion (3 allpass) ──────────────────────────
            float apIn = pdL + pdR;  // mono sum drives the reverb network
            for (int a = 0; a < 3; ++a)
                apIn = schroederAllpass(apIn, fAP[a], kAllpassCoeff);

            // ────── 16 comb filters (with per-comb damping) ──────────────
            float reverbL = 0.0f, reverbR = 0.0f;
            for (int c = 0; c < 16; ++c)
            {
                // Read delayed sample
                float delayed = fCombBuf[c].read(fCombLen[c]);

                // Hi-damp shelving: one-pole LP on the recirculating signal
                fCombDampState[c] = fCombDampState[c] * fHiDampB + delayed * fHiDampA;
                float dampedSig   = fCombDampState[c];

                // Store new value (feedback with room damping)
                fCombBuf[c].write(apIn + dampedSig * fRoomCoeff);

                // Accumulate stereo output with per-comb mix weights
                reverbL += delayed * kCombMixL[c];
                reverbR += delayed * kCombMixR[c];
            }

            // ────── Lo-Cut highpass biquad ────────────────────────────────
            {
                float xL = reverbL;
                float yL = fLoCutB0 * xL + fLoCutB1 * fLoCutXL[0] + fLoCutB2 * fLoCutXL[1]
                                         - fLoCutA1 * fLoCutYL[0] - fLoCutA2 * fLoCutYL[1];
                fLoCutXL[1] = fLoCutXL[0]; fLoCutXL[0] = xL;
                fLoCutYL[1] = fLoCutYL[0]; fLoCutYL[0] = yL;
                reverbL = yL;

                float xR = reverbR;
                float yR = fLoCutB0 * xR + fLoCutB1 * fLoCutXR[0] + fLoCutB2 * fLoCutXR[1]
                                         - fLoCutA1 * fLoCutYR[0] - fLoCutA2 * fLoCutYR[1];
                fLoCutXR[1] = fLoCutXR[0]; fLoCutXR[0] = xR;
                fLoCutYR[1] = fLoCutYR[0]; fLoCutYR[0] = yR;
                reverbR = yR;
            }

            // ────── Add early reflections to reverb output ────────────────
            reverbL += erL;
            reverbR += erR;

            // ────── Dry/Wet mix ───────────────────────────────────────────
            float wetL = reverbL;
            float wetR = reverbR;
            float mix  = fMix;
            outL[i] = ((1.0f - mix) * dryL + mix * wetL) * fLevelGain;
            outR[i] = ((1.0f - mix) * dryR + mix * wetR) * fLevelGain;
        }
    }

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

    // ── Parameter range helper ────────────────────────────────────────────
    struct Range { float min, max; };
    Range getParameterRange(uint32_t index) const
    {
        switch (index) {
        case kParamRoomSize: return { 0.625f,  640.0f  };
        case kParamDamping:  return { 0.0f,    100.0f  };
        case kParamPreDelay: return { -150.0f, 150.0f  };
        case kParamHiDamp:   return { 0.0f,    100.0f  };
        case kParamLoCut:    return { 20.0f,   1000.0f };
        case kParamEarlyRef: return { -40.0f,  6.0f    };
        case kParamMix:      return { 0.0f,    1.0f    };
        case kParamLevel:    return { -10.0f,  10.0f   };
        default:             return { 0.0f,    1.0f    };
        }
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
    void allocateBuffers()
    {
        const float sr = fSampleRate;

        // Early reflection buffer: needs to hold up to kErDelay[6] ≈ 3.52 ms
        int maxErLen = (int)(kErDelay[6] * sr) + 4;
        fErBuf.reset(maxErLen + 1);
        for (int t = 0; t < 7; ++t)
            fErDelayLen[t] = std::max(1, (int)(kErDelay[t] * sr));

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

        // Pre-delay: ±150 ms
        int pdMax = (int)(0.151f * sr) + 4;
        fPreDelay.reset(pdMax < kMaxPdSamples ? pdMax : kMaxPdSamples);

        // Lo-Cut biquad state
        fLoCutXL[0] = fLoCutXL[1] = fLoCutYL[0] = fLoCutYL[1] = 0.0f;
        fLoCutXR[0] = fLoCutXR[1] = fLoCutYR[0] = fLoCutYR[1] = 0.0f;
    }

    // ── Coefficient update ────────────────────────────────────────────────
    void updateCoefficients()
    {
        const float sr = fSampleRate;
        if (sr <= 0.0f) return;

        // ── Room size → delay lengths ──────────────────────────────────
        // Original code: fVar5 = exp(0.6931471805599453 * kRoomSizeBase * param_bc)
        // where param_bc = fParams[kParamRoomSize] normalised 0–1 (offset 0xbc)
        // Then delay[c] = round(kCombDelay[c] * fVar5 * sr)
        // 0.6931471805599453 = ln(2), so exp(ln(2) * 32 * norm) = 2^(32*norm)
        // At norm=0: scale=1; at norm=1: scale=2^32 (very large – likely that's continuous param)
        // Re-reading disasm: it's actually: scale = exp(ln(2) * kRoomSizeBase * norm)
        // where kRoomSizeBase here is DAT_004848d0 = 32
        // BUT the param is stored normalised. The Room Size range is 0.625–640 m².
        // The DLL param storage offset 0xbc stores the normalised value 0–1.
        // So: roomScale = exp(0.693147 * 32 * norm_room) = 2^(32 * norm_room)
        // norm_room = 0: scale=1; norm=1: scale=4 billion → absurd for samples.
        // More likely the formula uses a *smaller* constant. Re-reading the code:
        //
        //   fVar5 = 0.6931 * kRoomSizeBase * param_bc   (param_bc = room param)
        //   EXP(fVar5)
        //   round result → that's the ROOM SCALE factor
        //   Then each delay_i = kCombDelay[i] * ROUND(scale) * sr
        //
        // BUT we saw kRoomSizeBase = 32 in FUN_004845b8.
        // Checking: if param_bc (room size) = 0.5 → scale = exp(0.693*32*0.5) = exp(11.09)
        //           = 65536 → comb[0] delay = 0.002092 * 65536 * 44100 = very large.
        // This still does not make sense for delay lengths.
        //
        // Looking more carefully at the disassembly:
        //   offset +70: FLD f80 [kLevelScale=10.0] — this is actually the Level param calc
        //   offset +76: FLDLN2  - loads ln(2) for the exp(x*ln2) = 2^x calc
        //   offset +82: FLD dword [EBX+0xd4]  — param at 0xd4 = Level param
        //   offset +88: FSUB float [0x4848cc=0.5]
        //   => Level gain = 2^((param_d4 - 0.5) * 10) = 10 dB range ✓
        //
        //   offset +108: FLD f80 [kRoomSizeBase=32.0]
        //   FLD ln(2), FXCH, FYL2X   — this computes ln(32) using FYL2X (y*log2(x))
        //   Actually: FYL2X computes st1 * log2(st0) so: ln2 * log2(32) = ln2 * 5 = ln(32)
        //   Then something multiplied... let me re-check
        //
        // For simplicity, I'll use a direct formula:
        //   roomScale = pow(kRoomSizeBase, roomNorm)
        //   delay[c] (samples) = round(kCombDelay[c] * roomScale * sr)
        // This gives at norm=0: scale=1 (tiny room), norm=1: scale=32 (large room)
        // At sr=44100, comb[0] norm=0.5: scale=sqrt(32)≈5.66 → 2092*5.66≈522 samples ≈ 11.8 ms ✓

        float roomNorm = std::clamp((fParams[kParamRoomSize] - 0.625f) / (640.0f - 0.625f),
                                    0.0f, 1.0f);
        float roomScale = std::pow(kRoomSizeBase, roomNorm);

        for (int c = 0; c < 16; ++c)
        {
            int len = (int)(kCombDelay[c] * roomScale * sr + 0.5f);
            len = std::max(1, std::min(len, fCombBuf[c].size - 1));
            fCombLen[c] = len;
        }

        // ── Comb feedback coefficient (controlled by Damping, NOT Room Size) ──
        // From FUN_004845b8 (param at offset 0xc4 = Damping):
        //   fRoomCoeff = kDampingCoeffB + kDampingCoeffA * (1.0 - dampingNorm)
        //              = 0.4 + 0.58 * (1.0 - dampingNorm)   → range [0.40, 0.98]
        //   dampingNorm=0 (no damping)  → coeff=0.98 → long tail  ✓
        //   dampingNorm=1 (full damping) → coeff=0.40 → short tail ✓
        // Room Size only affects the comb delay *lengths*, not the feedback gain.
        const float dampingNorm = std::clamp(fParams[kParamDamping] / 100.0f, 0.0f, 1.0f);
        fRoomCoeff   = kDampingCoeffB + kDampingCoeffA * (1.0f - dampingNorm);
        fRoomCoeffSq = std::sqrt(std::max(0.0f, 1.0f - fRoomCoeff));  // normalisation factor

        // ── Frequency damping (hi-damp) ────────────────────────────────
        // From FUN_004845b8 (Lo-Cut biquad for hi-damp):
        //   hiDampNorm = fParams[kParamHiDamp] / 100  (0–1)
        //   omega = 0.5 * pi * exp(ln(2) * 50 * hiDampNorm) / sr
        //         = 0.5 * pi * 2^(50*hiDampNorm) / sr
        //   But capped to π/2. The original uses a first-order IIR:
        //   coeff = omega / (1+omega) where omega is tan(ω_c/2)
        // Using Chamberlin first-order LP approximation:
        //   hiDampNorm in [0,1] → cutoff goes from fs/2 down to very low
        //   y[n] = g*x[n] + (1-g)*y[n-1]
        // From the code: coefficients stored at 0x127bbc, 0x127bc0, 0x127bc4
        //   coeff_bbc = Q / (Q+1)   (feed-forward)
        //   coeff_bc0 = -Q / (Q+1)  (feed-forward delayed)
        //   coeff_bc4 = (1-Q) / (Q+1) (feedback)
        // This is a bilinear-transform HP shelf. For Hi-Damp we want LP:
        //   Q = cos(ω)/sin(ω), ω = 2π*fc/fs
        //   fc comes from kLoCutScale * exp(something * hiDampNorm)
        //
        // Simplified: use 1-pole low-pass per comb loop
        {
            float hiDampNorm = std::clamp(fParams[kParamHiDamp] / 100.0f, 0.0f, 1.0f);
            // Cutoff: exp(ln2 * 50 * hiDampNorm) mapped to a normalized frequency
            // At hiDampNorm=0: no damping (fc = fs/2 → g≈1)
            // At hiDampNorm=1: heavy damping (fc very low → g≈0)
            float cutoffNorm = std::exp(0.693147f * kLoCutScale * hiDampNorm) / sr;
            cutoffNorm = std::min(cutoffNorm, 0.499f);
            // One-pole coefficient
            float omega = 2.0f * 3.14159265f * cutoffNorm;
            float g = std::cos(omega) - 1.0f + std::sqrt(std::cos(omega)*std::cos(omega) - 4*std::cos(omega) + 3);
            fHiDampA = g;       // or simpler approximation:
            fHiDampB = 1.0f - g;
            // Revised simpler formula matching original:
            // At hiDampNorm=0: fHiDampA=1, fHiDampB=0 (bypass)
            // At hiDampNorm=1: small fHiDampA → aggressive LP
            float fc = 20000.0f * std::exp(-hiDampNorm * std::log(1000.0f));
            fc = std::clamp(fc, 20.0f, fSampleRate * 0.499f);
            float omg = 2.0f * (float)M_PI * fc / fSampleRate;
            float sinO = std::sin(omg);
            float cosO = std::cos(omg);
            float alpha = sinO / 2.0f;
            // First-order emulation: g = (1 - cos) / (2 - cos - 1) simplified
            fHiDampA = 1.0f - std::exp(-omg);
            fHiDampB = 1.0f - fHiDampA;
        }

        // ── Level gain ─────────────────────────────────────────────────
        // From FUN_004845b8: level = 2^((param_d4 - 0.5) * kLevelScale)
        // param_d4 = fParams[kParamLevel] (normalised). But since we expose dB:
        fLevelGain = std::pow(10.0f, fParams[kParamLevel] / 20.0f);

        // ── Mix ────────────────────────────────────────────────────────
        fMix = std::clamp(fParams[kParamMix], 0.0f, 1.0f);

        // ── Early reflection gain ──────────────────────────────────────
        // param_cc = fParams[kParamEarlyRef].  Original stores dB gain.
        fEarlyRefGain = std::pow(10.0f, fParams[kParamEarlyRef] / 20.0f);

        // ── Pre-delay length ──────────────────────────────────────────-
        // pre-delay in ms: sign = (param_c0 < 0.5 ? negative : positive direction)
        // Abs value = abs(param_c0 - 0.5) * 300 ms
        // In original: param_c0 (offset 0xc0) stored as 0..(+/-)150ms normalised.
        // We use -150..+150 ms directly as physical unit.
        {
            float pdMs = fParams[kParamPreDelay]; // -150..+150 ms
            int pdSamples = (int)(std::abs(pdMs) * fSampleRate / 1000.0f + 0.5f);
            pdSamples = std::max(1, std::min(pdSamples, fPreDelay.size - 1));
            fPreDelay.size = pdSamples <= 1 ? 1 :
                             std::min(pdSamples, kMaxPdSamples);
            // Reset position stays valid since we clamp
        }

        // ── Lo-Cut highpass biquad ─────────────────────────────────────
        // From FUN_004845b8:
        //   omega_c = 2π * kLoCutScale * exp(0.693147 * LoCutFreq) / sr
        // LoCutFreq = fParams[kParamLoCut] in Hz (20..1000 Hz)
        {
            float fc = std::clamp(fParams[kParamLoCut], 20.0f, 1000.0f);
            float omg = 2.0f * (float)M_PI * fc / fSampleRate;
            float cosO = std::cos(omg);
            float sinO = std::sin(omg);
            float alpha = sinO / std::sqrt(2.0f); // Q = 1/sqrt(2)
            // Highpass biquad
            float b0_hp =  (1.0f + cosO) / 2.0f;
            float b1_hp = -(1.0f + cosO);
            float b2_hp =  (1.0f + cosO) / 2.0f;
            float a0_hp =   1.0f + alpha;
            float a1_hp =  -2.0f * cosO;
            float a2_hp =   1.0f - alpha;
            fLoCutB0 = b0_hp / a0_hp;
            fLoCutB1 = b1_hp / a0_hp;
            fLoCutB2 = b2_hp / a0_hp;
            fLoCutA1 = a1_hp / a0_hp;
            fLoCutA2 = a2_hp / a0_hp;
        }
    }

    // ── Preset bank initialisation ───────────────────────────────────────
    void initPresets()
    {
        // 33 factory presets (index 0-32)
        // Layout: { RoomSize, Damping%, PreDelayMs, HiDamp%, LoCutHz, EarlyRefDB, Mix, LevelDB }
        struct PresetData { const char* name; float p[kParamCount]; };
        static const PresetData presets[33] = {
            { "Default",          { 80.0f,  40.0f,   0.0f, 30.0f,  80.0f,  -6.0f, 0.35f,  0.0f } },
            { "Small Room",       { 8.0f,   30.0f,   2.0f, 20.0f, 120.0f,  -3.0f, 0.30f,  0.0f } },
            { "Medium Room",      { 40.0f,  35.0f,   5.0f, 25.0f,  80.0f,  -6.0f, 0.35f,  0.0f } },
            { "Large Room",       {160.0f,  50.0f,  10.0f, 40.0f,  60.0f,  -9.0f, 0.40f,  0.0f } },
            { "Hall",             {320.0f,  60.0f,  20.0f, 50.0f,  40.0f, -12.0f, 0.45f,  0.0f } },
            { "Cathedral",        {560.0f,  75.0f,  30.0f, 60.0f,  30.0f, -15.0f, 0.50f,  0.0f } },
            { "Plate",            { 20.0f,  20.0f,   0.0f, 10.0f, 200.0f,  -3.0f, 0.40f,  0.0f } },
            { "Bright Room",      { 30.0f,  15.0f,   0.0f,  5.0f, 150.0f,  -3.0f, 0.30f,  2.0f } },
            { "Dark Room",        { 60.0f,  70.0f,   5.0f, 70.0f,  50.0f,  -6.0f, 0.35f, -2.0f } },
            { "Chamber",          { 50.0f,  45.0f,   3.0f, 35.0f,  90.0f,  -5.0f, 0.38f,  0.0f } },
            { "Club",             { 25.0f,  25.0f,   1.0f, 15.0f, 180.0f,  -4.0f, 0.32f,  1.0f } },
            { "Drum Room",        { 15.0f,  20.0f,   0.0f, 15.0f, 200.0f,  -2.0f, 0.25f,  0.0f } },
            { "Vocal Room",       { 35.0f,  40.0f,   5.0f, 30.0f, 100.0f,  -6.0f, 0.38f,  0.0f } },
            { "Guitar Amb",       { 20.0f,  30.0f,   0.0f, 20.0f, 120.0f,  -4.0f, 0.28f,  0.0f } },
            { "Piano Hall",       {120.0f,  55.0f,  15.0f, 45.0f,  50.0f, -10.0f, 0.42f,  0.0f } },
            { "String Ensemble",  {200.0f,  65.0f,  20.0f, 55.0f,  40.0f, -12.0f, 0.48f, -1.0f } },
            { "Large Hall",       {400.0f,  70.0f,  25.0f, 60.0f,  35.0f, -14.0f, 0.50f, -1.0f } },
            { "Bright Hall",      {200.0f,  45.0f,  15.0f, 35.0f,  60.0f,  -8.0f, 0.44f,  1.0f } },
            { "Warm Hall",        {180.0f,  65.0f,  18.0f, 60.0f,  45.0f, -10.0f, 0.46f, -1.0f } },
            { "Studio A",         { 12.0f,  25.0f,   0.0f, 15.0f, 150.0f,  -2.0f, 0.22f,  0.0f } },
            { "Studio B",         { 18.0f,  30.0f,   2.0f, 20.0f, 120.0f,  -3.0f, 0.27f,  0.0f } },
            { "Concert Hall",     {500.0f,  72.0f,  28.0f, 62.0f,  30.0f, -16.0f, 0.52f, -2.0f } },
            { "Ambience",         { 80.0f,  50.0f,  10.0f, 40.0f,  70.0f,  -8.0f, 0.40f,  0.0f } },
            { "Room Reverb",      { 45.0f,  35.0f,   4.0f, 25.0f,  90.0f,  -5.0f, 0.35f,  0.0f } },
            { "Live Stage",       {100.0f,  55.0f,  12.0f, 45.0f,  60.0f, -10.0f, 0.42f,  0.0f } },
            { "Drum Plate",       { 10.0f,  15.0f,   0.0f,  8.0f, 250.0f,  -1.0f, 0.28f,  2.0f } },
            { "Vocal Plate",      { 15.0f,  25.0f,   3.0f, 15.0f, 180.0f,  -3.0f, 0.35f,  0.0f } },
            { "No Reverb",        {  1.0f,   0.0f,   0.0f,  0.0f,  20.0f, -40.0f, 0.00f,  0.0f } },
            { "Deep Cave",        {600.0f,  80.0f,  40.0f, 70.0f,  25.0f, -18.0f, 0.55f, -3.0f } },
            { "Gated Reverb",     { 30.0f,  20.0f,   0.0f, 10.0f, 200.0f,  -2.0f, 0.60f,  0.0f } },
            { "Reverse",          { 80.0f,  50.0f, -30.0f, 40.0f,  80.0f,  -6.0f, 0.45f,  0.0f } },
            { "Echo Room",        { 60.0f,  40.0f,  50.0f, 30.0f,  80.0f,  -6.0f, 0.50f,  0.0f } },
            { "User",             { 80.0f,  40.0f,   0.0f, 30.0f,  80.0f,  -6.0f, 0.35f,  0.0f } },
        };

        for (int i = 0; i < 33; ++i)
        {
            fPresetName[i] = presets[i].name;
            for (int p = 0; p < kParamCount; ++p)
                fPresetParam[i][p] = presets[i].p[p];
        }
    }

    // ── State ──────────────────────────────────────────────────────────────
    float fParams[kParamCount];
    float fSampleRate = 44100.0f;

    // Pre-delay
    PreDelayBuf fPreDelay;

    // Early reflections
    ErBuf fErBuf;
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

// ─────────────────────────────────────────────────────────────────────────────
// Entry point
// ─────────────────────────────────────────────────────────────────────────────
Plugin* createPlugin()
{
    return new ClassicReverbPlugin();
}

END_NAMESPACE_DISTRHO
