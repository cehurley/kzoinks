#pragma once
#include <JuceHeader.h>
#include "IModule.h"
#include "SynthParameters.h"

class OscilloscopeModule : public IModule
{
public:
    // FIFO large enough for ~185ms at 44.1kHz — gives the trigger search plenty of range
    static constexpr int fifoSize      = 8192;
    // Samples drawn per frame — ~23ms, covers 2+ cycles down to ~87Hz
    static constexpr int displaySamples = 1024;

    explicit OscilloscopeModule(SynthParameters&);

    juce::String getName() const override        { return "Oscilloscope"; }
    int getPreferredHeight() const override      { return 155; }

    void prepareToPlay(double sr, int) override;
    void processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer&,
                      int startSample, int numSamples) override;

    std::unique_ptr<juce::Component> createEditor() override;

    // Lock-free audio → UI ring buffer written by audio thread, read by UI thread
    juce::AbstractFifo             fifo { fifoSize };
    std::array<float, fifoSize>    ringBuffer {};

private:
    double sampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OscilloscopeModule)
};
