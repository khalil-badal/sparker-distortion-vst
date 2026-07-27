#pragma once

// ---------------------------------------------------------------------------
// CircuitMode — top-level pedal-circuit selector for the 3-mode plugin.
//
//   EMBER  — Suhr Riot (existing, fully implemented; see dsp/EmberCircuit.h)
//   BURNER — REVV G3   (stub; see dsp/BurnerCircuit.h)
//   BLAZER — Friedman BE-OD (stub; see dsp/BlazerCircuit.h)
// ---------------------------------------------------------------------------
enum class CircuitMode
{
    EMBER = 0,
    BURNER,
    BLAZER
};
