#pragma once
#include <JuceHeader.h>

class OutputPanel : public juce::Component,
                    private juce::Timer,
                    private juce::ChangeListener
{
public:
    OutputPanel(juce::AudioDeviceManager& dm,
                std::atomic<float>& vuL,
                std::atomic<float>& vuR);

    ~OutputPanel() override;

    void paint    (juce::Graphics&) override;
    void resized  ()                override;

private:
    juce::AudioDeviceManager& deviceManager;
    std::atomic<float>&       vuLeft;
    std::atomic<float>&       vuRight;

    juce::ComboBox deviceBox;
    juce::ComboBox pairBox;
    juce::Label    deviceLabel;
    juce::Label    pairLabel;

    // Smoothed display levels (UI thread only)
    float dispLeft  = 0.0f;
    float dispRight = 0.0f;

    juce::Rectangle<int> vuBounds;

    void populateDevices();
    void populatePairs();
    void onDeviceChanged();
    void onPairChanged();

    void timerCallback()                              override;
    void changeListenerCallback(juce::ChangeBroadcaster*) override;

    void paintVU(juce::Graphics& g, juce::Rectangle<int> bounds,
                 float level, const juce::String& label);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OutputPanel)
};
