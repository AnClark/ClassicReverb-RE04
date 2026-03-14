#ifndef CLASSIC_REVERB_STRUCTURES_H_INCLUDED
#define CLASSIC_REVERB_STRUCTURES_H_INCLUDED

#include <cstring>
#include "DistrhoDetails.hpp"   // For ParameterRanges

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

constexpr DISTRHO::ParameterRanges kParamRanges[kParamCount] = {
    // def, min, max
    {  80.0f, 0.625f, 640.0f   },   // Room Size, m²
    {  40.0f, 0.0f,   100.0f   },   // Damping, %
    {  0.0f,  -150.0f, 150.0f, },   // Pre-delay, ms
    {  30.0f, 0.0f,   100.0f,  },   // Hi Damping, %
    {  80.0f, 20.0f,  1000.0f, },   // Lo-Cut, Hz
    {  -6.0f, -40.0f, 6.0f,    },   // Early Reflections, dB
    {  35.0f, 0.0f,   100.0f,  },   // Mix, %
    {  0.0f,  -10.0f, 10.0f,   },   // Level, dB
};

// ─────────────────────────────────────────────────────────────────────────────
// Circular buffer helpers (fixed maximum size)
// ─────────────────────────────────────────────────────────────────────────────
// Buffer sizes are scaled for sample rates up to 192 kHz.
// Derivation (192 kHz, room scale = 32):
//   kMaxCombSamples : ceil(0.004007 * 32 * 192000) + 8  = 24 563  → 25 000
//   kMaxErSamples   : ceil(0.003520 *      192000) + 5  =    681  →    700
//   kMaxPdSamples   : ceil(0.151    *      192000) + 4  = 28 996  → 29 200
static constexpr int kMaxCombSamples = 25000;
static constexpr int kMaxApSamples   = 1200;  // max AP delay ≈ 61 @ 192 kHz
static constexpr int kMaxErSamples   = 700;
static constexpr int kMaxPdSamples   = 29200;

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

// Pre-delay stores stereo pairs.
// Uses a fixed-size ring buffer (always kMaxPdSamples).  The desired delay
// length is passed per-call so that changing it mid-stream never invalidates
// the write pointer and never requires a buffer clear.
struct PreDelayBuf
{
    float bufL[kMaxPdSamples];
    float bufR[kMaxPdSamples];
    int   writePos = 0;

    void reset()
    {
        writePos = 0;
        std::memset(bufL, 0, sizeof(bufL));
        std::memset(bufR, 0, sizeof(bufR));
    }

    // Write inL/inR at the current write head, then read back delaySamples
    // behind it.  delaySamples == 0 → pure pass-through (read then write to
    // the same slot, effectively zero delay).
    void process(float inL, float inR, float& outL, float& outR, int delaySamples)
    {
        // First write so that delaySamples==0 returns the current input.
        bufL[writePos] = inL;
        bufR[writePos] = inR;
        int readPos = writePos - delaySamples;
        if (readPos < 0) readPos += kMaxPdSamples;
        outL = bufL[readPos];
        outR = bufR[readPos];
        if (++writePos >= kMaxPdSamples) writePos = 0;
    }
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

#endif // CLASSIC_REVERB_STRUCTURES_H_INCLUDED
