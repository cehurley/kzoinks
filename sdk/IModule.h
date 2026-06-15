#pragma once
#include <JuceHeader.h>

// Base interface for all rack modules.
// Audio-effect modules override processBlock; parameter/synth modules only provide an editor.
class IModule
{
public:
    virtual ~IModule() = default;

    virtual juce::String getName() const = 0;

    // Preferred height of the editor component in the rack
    virtual int getPreferredHeight() const { return 180; }

    // Returns a new editor component owned by the caller
    virtual std::unique_ptr<juce::Component> createEditor() = 0;

    // Called before audio starts — store sample rate / reset state.
    virtual void prepareToPlay(double /*sampleRate*/, int /*samplesPerBlock*/) {}

    // Called on the audio thread each block. Default is pass-through.
    virtual void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&,
                              int /*startSample*/, int /*numSamples*/) {}
};
