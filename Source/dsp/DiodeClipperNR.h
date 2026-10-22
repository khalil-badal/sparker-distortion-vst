#pragma once
#include <JuceHeader.h>

// ===========================================================================
// DiodeClipperNR — shared Newton-Raphson implicit diode-feedback clipper.
//
// Op-amp inverting stage: input current Vin/Rin balances feedback-branch
// current through Rf in parallel with two antiparallel 1N4148 pairs
// (D2-D5). At the virtual-ground node:
//
//   Vin/Rin + Vout/Rf + Ir*(exp(Vout/nVt) - exp(-Vout/nVt)) = 0
//
// Solved per-sample via Newton-Raphson. The nonlinearity is MEMORYLESS —
// prevVout is only a convergence seed, not true state. Extracted from
// BurnerCircuit.cpp/BlazerCircuit.cpp where it was duplicated in anonymous
// namespaces with identical math.
//
// Ir is doubled vs. a single 1N4148 pair because D2-D5 form two
// antiparallel pairs in parallel (BOM-confirmed 4-diode count on both
// the REVV G3 and Friedman BE-OD PCBs).
// ===========================================================================
namespace diodeclipper
{
    constexpr float kIs      = 2.52e-9f;    // 1N4148 SPICE-fit saturation current
    constexpr float kN       = 1.752f;      // 1N4148 SPICE-fit ideality factor
    constexpr float kVt      = 0.02585f;    // thermal voltage at room temp
    constexpr float kNVt     = kN * kVt;
    constexpr float kIrPair  = 2.0f * kIs;  // two antiparallel pairs in parallel

    // Vout clamp: 4.0f / kNVt ≈ 88.3, safely under float exp() overflow
    // (~88.7) while far beyond where a real clipped signal should sit. This
    // prevents exp() overflow → inf → inf-inf = NaN that would permanently
    // corrupt prevVout.
    constexpr float kVoutClamp = 4.0f;

    inline float solve (float vin, float rin, float rf, float& prevVout) noexcept
    {
        float vout = juce::jlimit (-kVoutClamp, kVoutClamp, prevVout);

        for (int iter = 0; iter < 8; ++iter)
        {
            const float e1 = std::exp ( vout / kNVt);
            const float e2 = std::exp (-vout / kNVt);
            const float f  = (vout / rf) + kIrPair * (e1 - e2) + (vin / rin);
            const float fp = (1.0f / rf) + (kIrPair / kNVt) * (e1 + e2);

            const float step = f / fp;
            vout = juce::jlimit (-kVoutClamp, kVoutClamp, vout - step);

            if (std::abs (step) < 1.0e-6f)
                break;
        }

        prevVout = vout;
        return vout;
    }
}
