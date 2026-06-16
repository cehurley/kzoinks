#pragma once
#include <JuceHeader.h>
#include "IModule.h"
#include "SynthParameters.h"
#include <set>

class ArpModule : public IModule
{
public:
    explicit ArpModule(SynthParameters&);

    juce::String getName() const override { return "Arp"; }

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&,
                      int startSample, int numSamples) override;

    std::unique_ptr<juce::Component> createEditor() override;
    void saveState(juce::XmlElement&)        const override;
    void loadState(const juce::XmlElement&)        override;

    // UI → audio
    std::atomic<float> bpm     { 120.0f }; // 40–240
    std::atomic<int>   subdiv  { 8 };      // 4 / 8 / 16 / 32
    std::atomic<int>   pattern { 0 };      // 0=Up 1=Down 2=Up-Down 3=Random
    std::atomic<int>   octaves { 1 };      // 1–4
    std::atomic<float> gate    { 0.6f };   // 0.1–1.0

    // Audio → UI (display)
    std::atomic<int> currentNote  { -1 };
    std::atomic<int> stepIndex    { 0 };
    std::atomic<int> seqLength    { 0 };
    std::atomic<int> heldCount    { 0 };

    bool seqDirty = false; // set by UI, consumed by audio thread on next block

private:
    double sampleRate  = 44100.0;
    double stepPhase   = 0.0;
    bool   noteIsOn    = false;
    int    activeNote  = -1;
    int    currentStep = 0;

    std::set<int>    heldNotes;
    std::vector<int> sequence;

    void rebuildSequence();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ArpModule)
};
