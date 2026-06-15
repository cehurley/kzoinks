#pragma once
#include <JuceHeader.h>
#include "SynthParameters.h"

class SynthVoice : public juce::MPESynthesiserVoice
{
public:
    explicit SynthVoice(const SynthParameters& params);

    void noteStarted() override;
    void noteStopped(bool allowTailOff) override;
    void notePressureChanged() override;
    void notePitchbendChanged() override;
    void noteTimbreChanged() override;
    void noteKeyStateChanged() override;

    void renderNextBlock(juce::AudioBuffer<float>& buffer,
                         int startSample, int numSamples) override;

    void setCurrentSampleRate(double newSampleRate) override;

private:
    const SynthParameters& params;

    // Oscillator phases
    double phase1 = 0.0, phase2 = 0.0;

    // Chamberlin SVF state
    float svfLow = 0.0f, svfBand = 0.0f;

    // Dirt one-pole LP state (pre-filter low-end seat)
    float dirtLP = 0.0f;

    // Smoothed parameter values — slew toward targets each sample to prevent
    // abrupt SVF coefficient changes from causing filter instability
    float smoothCutoff = 4000.0f;
    float smoothRes    = 0.8f;
    float smoothDirt   = 0.0f;
    float smoothCoeff  = 0.004f;  // recomputed in setCurrentSampleRate

    juce::ADSR ampEnv, filterEnv;

    // Per-note MPE values
    float noteVelocity = 0.0f;
    float notePressure = 0.0f;
    float noteTimbre   = 0.5f;

    double sampleRate = 44100.0;

    float polyBlep(double t, double dt) const noexcept;
    float sawSample   (double& phase, double freq) noexcept;
    float squareSample(double& phase, double freq) noexcept;
    float sineSample  (double& phase, double freq) noexcept;
    float processSVF  (float input, float cutoffHz, float q) noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SynthVoice)
};
