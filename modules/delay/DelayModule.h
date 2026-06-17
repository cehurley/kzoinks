#pragma once
#include <JuceHeader.h>
#include "IModule.h"
#include "SynthParameters.h"

class DelayModule : public IModule
{
public:
    explicit DelayModule(SynthParameters&);

    juce::String getName()        const override { return "Delay"; }
    int getPreferredHeight()      const override { return 200; }

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&,
                      int startSample, int numSamples) override;

    std::unique_ptr<juce::Component> createEditor() override;
    void saveState(juce::XmlElement&)        const override;
    void loadState(const juce::XmlElement&)        override;

    std::atomic<float> time     { 375.0f };  // ms, 1–2000
    std::atomic<float> feedback { 0.4f   };  // 0–0.95
    std::atomic<float> highCut  { 5000.0f }; // Hz, feedback filter
    std::atomic<float> mix      { 0.4f   };  // 0–1
    std::atomic<bool>  pingPong { false  };

private:
    static constexpr int kMaxDelaySec = 3;

    std::vector<float> bufL, bufR;
    int   writePtr  = 0;
    int   bufSize   = 0;
    float hcStateL  = 0.0f;
    float hcStateR  = 0.0f;
    double sampleRate = 44100.0;

    float readBuf(const std::vector<float>& buf, float delaySamples) const noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DelayModule)
};
