#pragma once
#include <atomic>

// Shared synth state written by the UI (message thread) and read by voices (audio thread).
// All fields are std::atomic for thread safety.
struct SynthParameters
{
    // Oscillators
    std::atomic<int>   waveform  { 0 };      // 0=saw, 1=square, 2=sine
    std::atomic<float> detune    { 7.0f };   // osc2 detune in cents

    // Dirt — pre-filter soft saturation with low-end emphasis
    std::atomic<float> dirt      { 0.0f };   // 0-1

    // Filter
    std::atomic<float> filterCutoff { 4000.0f };
    std::atomic<float> filterRes    { 0.8f };
    std::atomic<float> filterEnvAmt { 0.5f };

    // Amp envelope
    std::atomic<float> ampAttack  { 0.01f };
    std::atomic<float> ampDecay   { 0.1f };
    std::atomic<float> ampSustain { 0.8f };
    std::atomic<float> ampRelease { 0.4f };

    // Filter envelope
    std::atomic<float> filtAttack  { 0.005f };
    std::atomic<float> filtDecay   { 0.3f };
    std::atomic<float> filtSustain { 0.4f };
    std::atomic<float> filtRelease { 0.6f };
};
