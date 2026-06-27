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

    // Optional logo shown in the module window title bar instead of text
    virtual juce::Image getLogo() const { return {}; }

    // Called before audio starts — store sample rate / reset state.
    virtual void prepareToPlay(double /*sampleRate*/, int /*samplesPerBlock*/) {}

    // Called on the audio thread each block. Default is pass-through.
    virtual void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&,
                              int /*startSample*/, int /*numSamples*/) {}

    // Bypass — audio thread reads this; UI thread writes it.
    void setEnabled(bool e) noexcept { enabled.store(e); }
    bool isEnabled()  const noexcept { return enabled.load(); }

    // Setup persistence — modules override these to save / restore their parameters.
    // Attributes are written directly on the element passed in; no child elements needed.
    virtual void saveState  (juce::XmlElement&)       const {}
    virtual void loadState  (const juce::XmlElement&)       {}

private:
    std::atomic<bool> enabled { true };
};
