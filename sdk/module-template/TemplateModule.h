#pragma once
#include <JuceHeader.h>
#include "IModule.h"
#include "SynthParameters.h"

// Rename this class (and every reference to it) to match your module.
class TemplateModule : public IModule
{
public:
    // SynthParameters gives access to the synth voice state (waveform, filter,
    // envelopes, etc.).  Store a reference if you need it; ignore it if you don't.
    explicit TemplateModule(SynthParameters& params);

    // Name shown in the chain strip and window title.
    juce::String getName() const override   { return "Template"; }

    // Initial window content height in pixels (window opens at 320×320 regardless,
    // but this hint can be used by future layout code).
    int getPreferredHeight() const override { return 120; }

    // Called once before audio starts, and again if the device changes.
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;

    // Called on the audio thread every block.  startSample and numSamples define
    // the slice of `buffer` you should read/write.  Buffer already contains the
    // rendered synth audio when this is called.
    void processBlock(juce::AudioBuffer<float>& buffer,
                      juce::MidiBuffer&         midi,
                      int startSample, int numSamples) override;

    // Return a heap-allocated Component that becomes the window's content view.
    // Called once on the message thread; the returned component is owned by the window.
    std::unique_ptr<juce::Component> createEditor() override;

private:
    // Use std::atomic for any state written by the UI and read by the audio thread.
    std::atomic<float> gain { 1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TemplateModule)
};
