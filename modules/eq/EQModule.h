#pragma once
#include <JuceHeader.h>
#include "IModule.h"
#include "SynthParameters.h"

class EQModule : public IModule
{
public:
    static constexpr int   numBands = 8;
    static constexpr float maxGainDB = 12.0f;
    static constexpr float bandQ     = 1.41f;  // ~1-octave bandwidth

    static constexpr std::array<float, numBands> frequencies =
        { 63.f, 125.f, 250.f, 500.f, 1000.f, 2000.f, 4000.f, 8000.f };

    static constexpr std::array<const char*, numBands> labels =
        { "63", "125", "250", "500", "1k", "2k", "4k", "8k" };

    explicit EQModule(SynthParameters&);  // synth params not used by EQ

    juce::String getName() const override        { return "Graphic EQ"; }
    int          getPreferredHeight() const override { return 165; }

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi,
                      int startSample, int numSamples) override;

    std::unique_ptr<juce::Component> createEditor() override;
    void saveState(juce::XmlElement&)        const override;
    void loadState(const juce::XmlElement&)        override;

    std::atomic<float> gains[numBands];  // each ±maxGainDB dB, default 0

private:
    struct BiquadState { float x1=0.f, x2=0.f, y1=0.f, y2=0.f; };
    BiquadState states[2][numBands];  // [channel][band]

    double sampleRate = 44100.0;

    // Fills b0n, b1n, b2n, a2n (all divided by a0). For peak filters a1n == b1n.
    static void peakCoeffs(float freq, float gainDB, float Q, double sr,
                            float& b0n, float& b1n, float& b2n, float& a2n) noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EQModule)
};
