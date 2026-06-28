#pragma once
#include <JuceHeader.h>
#include "IModule.h"
#include "SynthParameters.h"
#include <set>

class StepSequencerModule : public IModule
{
public:
    static constexpr int numSteps = 16;

    // Cycled through by clicking a step button.
    enum class StepState : int { Off = 0, Root = 1, OctaveUp = 2, OctaveDown = 3 };

    explicit StepSequencerModule(SynthParameters&);

    juce::String getName()        const override { return "Step Seq"; }
    int getPreferredHeight()      const override { return 220; }

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void processMidi(juce::MidiBuffer&, int startSample, int numSamples) override;

    std::unique_ptr<juce::Component> createEditor() override;
    void saveState(juce::XmlElement&)        const override;
    void loadState(const juce::XmlElement&)        override;

    // UI → audio
    std::atomic<float> bpm     { 120.0f }; // 40-240
    std::atomic<int>   subdiv  { 16 };     // 4 / 8 / 16 / 32, steps are this note value
    std::atomic<float> gate    { 0.6f };   // 0.05-1.0, fraction of a step the note stays on
    std::atomic<bool>  running { false };

    // Per-step pattern. UI thread writes single ints directly (no trigger flag
    // needed — a single atomic int write is already safe for the audio thread to read).
    std::atomic<int> steps[numSteps];

    // UI thread requests; audio thread consumes and clears.
    std::atomic<bool> trigPlayStop { false };
    std::atomic<bool> trigClear    { false };

    // Audio → UI (display)
    std::atomic<int> currentStep { -1 };
    std::atomic<int> rootNoteOut { 60 };

private:
    double sampleRate = 44100.0;
    double stepPhase  = 0.0;
    int    curStep    = 0;
    bool   noteIsOn   = false;
    int    activeNote = -1;
    int    rootNote   = 60;

    std::set<int> heldNotes;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StepSequencerModule)
};
