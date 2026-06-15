#pragma once
#include <JuceHeader.h>
#include "SynthEngine.h"
#include "IModule.h"
#include "ChainStrip.h"
#include "ModuleWindow.h"

// Fixes two focus-related MIDI issues:
//
// 1. Machine-gun: JUCE's focusLost() calls resetAnyActiveNotes(), sending note-offs
//    the instant a slider steals focus → OS key-repeat then rapid-fires new note-ons.
//    Fix: override focusLost to be a no-op.
//
// 2. Hanging notes: with focusLost suppressed, key-release events go to whichever
//    control has focus — the keyboard never sees them, so notes hang forever.
//    Fix: register as a KeyListener on the top-level window so key events are
//    received regardless of which control currently holds focus.
class SynthKeyboard : public juce::MidiKeyboardComponent,
                      private juce::KeyListener
{
public:
    SynthKeyboard(juce::MidiKeyboardState& s, Orientation o)
        : juce::MidiKeyboardComponent(s, o), state(s) {}

    void focusLost(FocusChangeType) override {}

    void silenceAllNotes() { state.allNotesOff(0); }

    // Expose the KeyListener interface so MainComponent can register this on
    // module windows — private inheritance hides it from everyone else.
    juce::KeyListener* asKeyListener() { return this; }

private:
    void parentHierarchyChanged() override
    {
        MidiKeyboardComponent::parentHierarchyChanged();
        auto* top = getParentComponent() ? getTopLevelComponent() : nullptr;
        if (top != registeredTop)
        {
            if (registeredTop != nullptr)
                registeredTop->removeKeyListener(this);
            registeredTop = top;
            if (registeredTop != nullptr)
                registeredTop->addKeyListener(this);
        }
    }

    using MidiKeyboardComponent::keyPressed;
    using MidiKeyboardComponent::keyStateChanged;

    bool keyPressed(const juce::KeyPress& key, juce::Component* originator) override
    {
        if (originator == this) return false;
        return MidiKeyboardComponent::keyPressed(key);
    }

    bool keyStateChanged(bool isKeyDown, juce::Component* originator) override
    {
        if (originator == this) return false;
        return MidiKeyboardComponent::keyStateChanged(isKeyDown);
    }

    juce::MidiKeyboardState& state;
    juce::Component*         registeredTop = nullptr;
};

class MainComponent : public juce::AudioAppComponent,
                      private juce::FocusChangeListener
{
public:
    MainComponent();
    ~MainComponent() override;

    void prepareToPlay(int samplesPerBlock, double sampleRate) override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override;
    void releaseResources() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    // ---- Setup persistence -------------------------------------------------
    juce::String      currentSetupName;
    juce::StringArray getSetupNames() const;
    void showNewSetupDialog();
    void saveCurrentSetup();          // re-saves currentSetupName
    void saveSetupAs(const juce::String& name);
    void loadSetup  (const juce::String& name);

    // Creates the SetupMenuModel and registers it; call once after construction.
    juce::MenuBarModel* createMenuModel();

private:
    SynthEngine synthEngine;

    juce::MidiKeyboardState     keyboardState;
    SynthKeyboard               keyboard;
    juce::MidiMessageCollector  midiCollector;

    juce::Slider volumeSlider;
    juce::Label  volumeLabel;

    std::vector<std::unique_ptr<IModule>>       modules;
    std::vector<std::unique_ptr<ModuleWindow>>  moduleWindows;
    ChainStrip                                  chainStrip;

    std::unique_ptr<juce::MenuBarModel> menuModel;

    juce::LookAndFeel_V4 darkLook;

    void globalFocusChanged(juce::Component* focusedComponent) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
