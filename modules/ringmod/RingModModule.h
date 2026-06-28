#pragma once
#include <JuceHeader.h>
#include "IModule.h"
#include "SynthParameters.h"

class RingModModule : public IModule
{
public:
    explicit RingModModule(SynthParameters&);

    juce::String getName()        const override { return "Ring Mod"; }
    int getPreferredHeight()      const override { return 215; }

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&,
                      int startSample, int numSamples) override;

    std::unique_ptr<juce::Component> createEditor() override;
    void saveState(juce::XmlElement&)        const override;
    void loadState(const juce::XmlElement&)        override;

    enum Waveform { Sine = 0, Triangle = 1, Square = 2 };

    // UI → audio
    std::atomic<float> freq   { 220.0f }; // 1–5000 Hz carrier frequency
    std::atomic<int>   wave   { Sine   }; // carrier shape
    std::atomic<float> depth  { 1.0f   }; // 0=tremolo-style AM (unipolar carrier) .. 1=full ring mod (bipolar)
    std::atomic<float> spread { 0.0f   }; // 0–180 degrees, L/R carrier phase offset for stereo width
    std::atomic<float> mix    { 1.0f   }; // 0–1 dry/wet

    // Audio → UI: carrier phase for the sweep display (0–1)
    std::atomic<float> carrierPhaseOut { 0.0f };

    // Public so the editor can draw the carrier waveform.
    static float carrierSample(double phase, int waveform) noexcept;

private:
    double sampleRate     = 44100.0;
    double carrierPhase[2] = { 0.0, 0.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RingModModule)
};
