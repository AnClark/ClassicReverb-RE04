#ifndef CLASSIC_REVERB_STRUCTURES_H_INCLUDED
#define CLASSIC_REVERB_STRUCTURES_H_INCLUDED

#include <cstring>

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

#endif // CLASSIC_REVERB_STRUCTURES_H_INCLUDED
