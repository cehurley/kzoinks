#pragma once
#include <JuceHeader.h>
#include "IModule.h"
#include "SynthParameters.h"

class AutoWahModule : public IModule
{
public:
    explicit AutoWahModule(SynthParameters&);

    juce::String getName() const override { return "Auto-Wah"; }

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&,
                      int startSample, int numSamples) override;

    std::unique_ptr<juce::Component> createEditor() override;
    juce::Image getLogo() const override;
    void saveState(juce::XmlElement&)        const override;
    void loadState(const juce::XmlElement&)        override;

    // UI → audio thread parameters
    std::atomic<float> sensitivity { 0.7f };  // 0–1: how hard the envelope drives the filter
    std::atomic<float> attack      { 8.0f };  // ms
    std::atomic<float> release     { 80.0f }; // ms
    std::atomic<float> baseFreq    { 200.0f };// Hz — filter rests here with no input
    std::atomic<float> range       { 3000.0f};// Hz — max sweep above baseFreq
    std::atomic<float> resonance   { 3.5f };  // Q — higher = more "quacky"
    std::atomic<float> mix         { 1.0f };  // dry/wet

    // Audio → UI: current envelope level for the display (written by audio thread)
    std::atomic<float> envLevel { 0.0f };

private:
    double sampleRate = 44100.0;

    // Per-channel SVF state
    struct SVF { float low = 0, band = 0; };
    SVF svf[2];

    // Per-channel envelope follower state
    float env[2] = { 0.0f, 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AutoWahModule)
};
