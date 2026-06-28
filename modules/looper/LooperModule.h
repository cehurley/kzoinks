#pragma once
#include <JuceHeader.h>
#include "IModule.h"
#include "SynthParameters.h"

class LooperModule : public IModule
{
public:
    explicit LooperModule(SynthParameters&);

    juce::String getName()        const override { return "Looper"; }
    int getPreferredHeight()      const override { return 200; }

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&,
                      int startSample, int numSamples) override;

    std::unique_ptr<juce::Component> createEditor() override;
    void saveState(juce::XmlElement&)        const override;
    void loadState(const juce::XmlElement&)        override;

    enum class State { Empty, Recording, Playing, Stopped, Overdubbing };

    std::atomic<float> level { 0.8f }; // 0-1, loop playback level
    std::atomic<float> decay { 1.0f }; // 0.5-1, overdub feedback decay

    std::atomic<int>   state        { (int)State::Empty };
    std::atomic<float> playProgress { 0.0f }; // 0-1, UI display only
    std::atomic<float> loopLenSec   { 0.0f };

    // UI thread sets these; audio thread consumes and clears them so all
    // actual state transitions happen on the audio thread.
    std::atomic<bool> trigRecord   { false };
    std::atomic<bool> trigPlayStop { false };
    std::atomic<bool> trigOverdub  { false };
    std::atomic<bool> trigUndo     { false };
    std::atomic<bool> trigClear    { false };

private:
    static constexpr int kMaxLoopSec = 30;

    std::vector<float> bufL, bufR;
    std::vector<float> undoL, undoR;
    int    maxSamples = 0;
    int    recLen     = 0;
    int    playhead   = 0;
    int    undoLen    = 0;
    double sampleRate = 44100.0;

    void handleRecordTrigger();
    void handlePlayStopTrigger();
    void handleOverdubTrigger();
    void handleUndoTrigger();
    void handleClearTrigger();
    void finishRecording();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LooperModule)
};
