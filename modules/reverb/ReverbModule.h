#pragma once
#include <JuceHeader.h>
#include "IModule.h"
#include "SynthParameters.h"

class ReverbModule : public IModule
{
public:
    explicit ReverbModule(SynthParameters&);

    juce::String getName()   const override { return "Reverb"; }

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&,
                      int startSample, int numSamples) override;

    std::unique_ptr<juce::Component> createEditor() override;
    void saveState(juce::XmlElement&)        const override;
    void loadState(const juce::XmlElement&)        override;

    // Written by UI thread, read by audio thread via applyParams()
    std::atomic<float> roomSize  { 0.5f };  // 0–1
    std::atomic<float> damping   { 0.5f };  // 0–1
    std::atomic<float> width     { 0.8f };  // 0–1
    std::atomic<float> mix       { 0.35f }; // 0–1 dry/wet

private:
    juce::Reverb reverb;

    void applyParams();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReverbModule)
};
