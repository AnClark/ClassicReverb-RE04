#ifndef CLASSIC_REVERB_DEFINES_H_INCLUDED
#define CLASSIC_REVERB_DEFINES_H_INCLUDED

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
// Feature flags
// ─────────────────────────────────────────────────────────────────────────────

// Set to 1 to use the perceptually uniform Damping curve (RT60 evenly
// distributed across the knob range).  Set to 0 to restore the original
// linear mapping from the disassembly.
#define CLASSIC_REVERB_IMPROVED_DAMPING_CURVE 1

// Set to 1 to restrict the Hi-Damp low-pass cutoff to a 2-decade range
// (200 Hz – 20 kHz) instead of 3 decades (20 Hz – 20 kHz).
// The original 3-decade range saturates around d=75 %: once fc < ~200 Hz
// the HF energy is already fully suppressed within the first few comb
// reflections, making the upper quarter of the knob perceptually inert.
// Narrowing to 2 decades keeps the full knob range audibly useful.
// Set to 0 to restore the unconstrained (20 Hz floor) behaviour.
#define CLASSIC_REVERB_IMPROVED_HIDAMP_RANGE 1

// Set to 1 to use a logarithmic mapping for the Room Size parameter,
// giving a more natural progression of RT60 values and more extreme tails at large sizes.
// This matches the behaviour of the original Classic Reverb plugin.
#define CLASSIC_REVERB_LOGARITHMIC_ROOM_SIZE 1

// Set to 1 to apply tanh soft-clipping to the final output.
// Formula: y = C * tanh(x / C), where C = kSoftClipCeiling.
// Properties:
//   - Slope = 1 at x = 0  (fully transparent for normal-level signals)
//   - Soft knee begins around |x| ≈ C / 3 ≈ 1.0 (0 dBFS)
//   - Hard asymptote at ±C (output never exceeds kSoftClipCeiling)
// Set to 0 to bypass (original plugin behaviour: no output limiting).
#define CLASSIC_REVERB_OUTPUT_SOFT_CLIP 1

// Set to 1 to inject a tiny randomisation noise (amplitude = kModAmp = 6e-8,
// extracted from _DAT_00485f5c in the original DLL) into the reverb input
// signal after the pre-delay stage but before early reflections and the
// allpass / comb network.  The same noise sample is added to both L and R
// channels, matching the original binary's behaviour.
//
// Purpose:
//   - Prevents comb filters from resonating at exact harmonic frequencies,
//     which can cause subtle "pitched flutter" artefacts on sustained tones.
//   - Provides mild de-correlation across the FDN network.
//   - Amplitude (~−164 dBFS) is completely inaudible but mathematically
//     ensures the impulse response is never perfectly periodic.
//
// Set to 0 for a fully deterministic reverb network (may exhibit
// deterministic comb resonances under certain test signals).
#define CLASSIC_REVERB_INPUT_NOISE_MODULATION 1

// Soft-clip ceiling in linear scale.  = 10^(+5/20) ≈ 1.778 (+5 dBFS).
// Signals well below 0 dBFS pass through unaffected; peaks above 0 dBFS
// are progressively attenuated; hard asymptote at +5 dBFS.
static constexpr float kSoftClipCeiling = 1.77827941f;  // 10^(5/20)
static constexpr float kSoftClipCeilingInv = 1.0f / kSoftClipCeiling;   // precomputed reciprocal for efficiency

#endif // CLASSIC_REVERB_DEFINES_H_INCLUDED
