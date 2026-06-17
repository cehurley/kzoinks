#pragma once
#include <JuceHeader.h>
#include "IModule.h"
#include "SynthParameters.h"

class PhaserModule : public IModule
{
public:
    explicit PhaserModule(SynthParameters&);

    juce::String getName()        const override { return "Phaser"; }
    int getPreferredHeight()      const override { return 200; }

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&,
                      int startSample, int numSamples) override;

    std::unique_ptr<juce::Component> createEditor() override;
    void saveState(juce::XmlElement&)        const override;
    void loadState(const juce::XmlElement&)        override;

    // UI-writable, audio-readable
    std::atomic<float> rate     { 0.5f   };  // 0.05–10 Hz
    std::atomic<float> depth    { 0.8f   };  // 0–1
    std::atomic<float> center   { 800.0f };  // Hz, base sweep frequency
    std::atomic<float> feedback { 0.5f   };  // -0.99–0.99
    std::atomic<float> mix      { 0.5f   };  // 0–1 dry/wet
    std::atomic<int>   stages   { 4      };  // 2, 4, 6, or 8

    // Audio→UI: current LFO phase for the sweep display (0–1)
    std::atomic<float> lfoPhase { 0.0f };

private:
    static constexpr int kMaxStages = 8;

    struct APFState { float x1 = 0.0f, y1 = 0.0f; };

    APFState apfState[2][kMaxStages];
    float    feedbackSample[2] = {};

    double sampleRate        = 44100.0;
    double lfoPhaseInternal  = 0.0;

    static float apf(APFState& s, float x, float a) noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhaserModule)
};
