#pragma once
#include <JuceHeader.h>
#include "IModule.h"
#include "SynthParameters.h"

class DistortionModule : public IModule
{
public:
    explicit DistortionModule(SynthParameters&);

    juce::String getName()        const override { return "Distortion"; }
    int getPreferredHeight()      const override { return 220; }

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&,
                      int startSample, int numSamples) override;

    std::unique_ptr<juce::Component> createEditor() override;
    juce::Image getLogo() const override;
    void saveState(juce::XmlElement&)        const override;
    void loadState(const juce::XmlElement&)        override;

    // Written by UI thread, read by audio thread
    std::atomic<int>   type  { 0 };    // 0=Overdrive 1=Distortion 2=Fuzz 3=Fold
    std::atomic<float> drive { 4.0f }; // 1–20
    std::atomic<float> tone  { 0.7f }; // 0=dark … 1=bright
    std::atomic<float> mix   { 1.0f }; // dry/wet
    std::atomic<float> level { 0.7f }; // output level

    // Public so the editor can draw the transfer-function curve
    static float shape(float x, int type, float drive) noexcept;

private:
    double sampleRate = 44100.0;
    float  toneState[2] { 0.0f, 0.0f };  // per-channel tone filter state

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DistortionModule)
};
