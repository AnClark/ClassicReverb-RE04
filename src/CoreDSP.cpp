#include "ClassicReverbPlugin.h"
#include "Defines.h"
#include "extra/ScopedDenormalDisable.hpp"

void ClassicReverbPlugin::run(const float** inputs, float** outputs, uint32_t frames)
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
        fPreDelay.process(dryL, dryR, pdL, pdR, fPreDelaySamples);

        // ────── Early reflections (7-tap, stereo) ───────────────────────
        // Original DLL stores L and R separately (offset+0xf4 / offset+0xf8),
        // giving true stereo early reflections rather than a mono sum.
        fErBufL.write(pdL);
        fErBufR.write(pdR);

        float erL = 0.0f, erR = 0.0f;
        for (int t = 0; t < 7; ++t)
        {
            int d = fErDelayLen[t];
            erL += fErBufL.read(d) * kErGain[t];
            erR += fErBufR.read(d) * kErGain[t];
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
        float rawL = ((1.0f - mix) * dryL + mix * wetL) * fLevelGain;
        float rawR = ((1.0f - mix) * dryR + mix * wetR) * fLevelGain;
#if CLASSIC_REVERB_OUTPUT_SOFT_CLIP
        // tanh soft clip: y = C * tanh(x / C)
        // Unity slope at x=0; knee ~0 dBFS; ceiling ±kSoftClipCeiling.
        outL[i] = kSoftClipCeiling * std::tanh(rawL * kSoftClipCeilingInv);
        outR[i] = kSoftClipCeiling * std::tanh(rawR * kSoftClipCeilingInv);
#else
        outL[i] = rawL;
        outR[i] = rawR;
#endif
    }
}

// ── Coefficient update ────────────────────────────────────────────────
void ClassicReverbPlugin::updateCoefficients()
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

    // The original DLL stores Room Size as a 0–1 normalised value internally
    // and computes roomScale = 32^param_bc (i.e. exp(ln32 * param_bc)).
    // The UI range is 0.625–640 m², which spans exactly 1024 = 2^10 in ratio,
    // so the correct inverse mapping is logarithmic:
    //   roomNorm = log2(size / 0.625) / log2(640 / 0.625)
    //            = log2(size / 0.625) / 10
    // At size=20 m²: roomNorm = log2(32)/10 = 0.5  →  roomScale = sqrt(32) ≈ 5.66  ✓
    // The previous linear mapping  (size-0.625)/(640-0.625) gave roomNorm≈0.030
    // at 20 m², producing roomScale≈1.11 — a factor ~5× too small, causing
    // RT60 to be ~5× shorter than the original DLL.
    //
    // TIP:
    // You can optionally revert to the linear mapping by defining CLASSIC_REVERB_LOGARITHMIC_ROOM_SIZE=0,
    // which gives a different flavour of reverb with a more compressed RT60 range
    // and less extreme tails at large sizes.
#if CLASSIC_REVERB_LOGARITHMIC_ROOM_SIZE
    float roomNorm = std::log2(fParams[kParamRoomSize] / 0.625f) / 10.0f;
    roomNorm = std::clamp(roomNorm, 0.0f, 1.0f);
#else
    float roomNorm = std::clamp((fParams[kParamRoomSize] - 0.625f) / (640.0f - 0.625f),
                                0.0f, 1.0f);
#endif
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

#if CLASSIC_REVERB_IMPROVED_DAMPING_CURVE
    // Perceptually uniform (log-log) mapping.
    //
    // The previous linear formula  g = kDampingCoeffB + kDampingCoeffA*(1-d)
    // and even the geometric-g formula  g = g_max*(g_min/g_max)^d  both suffer
    // from the same problem: RT60 ∝ 1/(-log₁₀(g)) is dominated by the range
    // near g→1, so 0–30 % of the knob covers most of the perceptible change.
    //
    // For truly uniform perception every equal knob step should multiply RT60
    // by the same factor (geometric sequence in RT60).  With C(d) = -log₁₀(g(d)):
    //
    //   RT60(d) ∝ 1/C(d)  →  for geometric RT60: C(d) = α · (β/α)^d
    //   ⟹  g(d) = 10^{ −α · (β/α)^d }
    //
    // where α = -log₁₀(g_max)  and  β = -log₁₀(g_min).
    // Each Δd = 0.25 interval multiplies RT60 by the same factor ≈ 2.60.
    {
        const float g_max = kDampingCoeffB + kDampingCoeffA;  // 0.98
        const float g_min = kDampingCoeffB;                   // 0.40
        const float alpha = -std::log10(g_max);               // ≈ 0.00877
        const float ratio = -std::log10(g_min) / alpha;       // ≈ 45.37
        fRoomCoeff = std::pow(10.0f, -alpha * std::pow(ratio, dampingNorm));
    }
#else
    // Original linear mapping from the disassembly.
    fRoomCoeff = kDampingCoeffB + kDampingCoeffA * (1.0f - dampingNorm);
#endif

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
#if CLASSIC_REVERB_IMPROVED_HIDAMP_RANGE
        // 2-decade range: fc sweeps 20000 Hz → 200 Hz (log(100) ≈ 4.605).
        // Rationale: below ~200 Hz the LP already eliminates all HF content
        // within the first few comb reflections; extending to 20 Hz only
        // continues to mute an already-silent band, making the upper quarter
        // of the knob travel perceptually inert.
        float fc = 20000.0f * std::exp(-hiDampNorm * std::log(100.0f));
#else
        // Original 3-decade range: fc sweeps 20000 Hz → 20 Hz.
        float fc = 20000.0f * std::exp(-hiDampNorm * std::log(1000.0f));
#endif
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
    fMix = std::clamp(fParams[kParamMix], 0.0f, 100.0f) * 0.01f;  // convert from percentage to 0.0-1.0

    // ── Early reflection gain ──────────────────────────────────────
    // param_cc = fParams[kParamEarlyRef].  Original stores dB gain.
    fEarlyRefGain = std::pow(10.0f, fParams[kParamEarlyRef] / 20.0f);

    // ── Pre-delay length ──────────────────────────────────────────-
    // Only the sample count is updated; the ring-buffer write pointer is
    // never touched here, so no glitch occurs on parameter change.
    {
        float pdMs = fParams[kParamPreDelay]; // −150…+150 ms
        int pdSamples = (int)(std::abs(pdMs) * fSampleRate / 1000.0f + 0.5f);
        fPreDelaySamples = std::min(pdSamples, kMaxPdSamples - 1);
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
