#pragma once
#include <JuceHeader.h>
#include <array>
#include "SynthEngine.h"
#include "IModule.h"
#include "ChainStrip.h"
#include "ModuleWindow.h"
#include "OutputPanel.h"

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
    void exportSetups();

    // Creates the SetupMenuModel and registers it; call once after construction.
    juce::MenuBarModel* createMenuModel();

private:
    SynthEngine synthEngine;

    juce::MidiKeyboardState     keyboardState;
    SynthKeyboard               keyboard;
    juce::MidiMessageCollector  midiCollector;

    juce::Slider volumeSlider;
    juce::Label  volumeLabel;
    juce::Image  kzoinksImage;
    juce::Rectangle<int> skullBounds;

    juce::TextButton octaveDownBtn { "-" };
    juce::TextButton octaveUpBtn   { "+" };
    juce::Label      octaveLabel;
    int              keyboardOctave { 5 };

    // The core synth voice ("Voice") is always present — not part of the
    // swappable FX rack below. It has no processBlock/processMidi override, so
    // it never needs the fxSlotsLock.
    std::unique_ptr<IModule>      voiceModule;
    std::unique_ptr<ModuleWindow> voiceWindow;
    juce::TextButton              voiceButton { "VOICE" };

    // Fixed-size FX insert rack (Logic-Pro-channel-strip style): each slot is
    // independently empty or holds one module instance, freely reassignable and
    // reorderable, duplicates allowed.
    struct FxSlot
    {
        std::unique_ptr<IModule>      module;   // nullptr = empty
        std::unique_ptr<ModuleWindow> window;   // nullptr = empty
    };
    static constexpr int numFxSlots = 10;
    std::array<FxSlot, numFxSlots> fxSlots;

    // Guards `module` (not `window`, which is message-thread-only) against the
    // audio thread iterating fxSlots concurrently with a slot being reassigned.
    // Held only for the brief pointer-swap, never across construction/destruction
    // of the module itself — see assignSlot()/clearSlot().
    juce::CriticalSection fxSlotsLock;

    juce::StringArray fxCatalog;  // available module type names, excluding "Voice"

    double lastSampleRate = 44100.0;
    int    lastBlockSize  = 512;

    ChainStrip chainStrip;

    void assignSlot(int index, const juce::String& typeName);
    void clearSlot (int index);
    void refreshChainStripDisplay();
    juce::String slotWindowKey(int index) const { return "moduleWindow_slot" + juce::String(index); }

    std::atomic<float> vuLeft  { 0.0f };
    std::atomic<float> vuRight { 0.0f };
    OutputPanel        outputPanel { deviceManager, vuLeft, vuRight };

    std::unique_ptr<juce::MenuBarModel> menuModel;
    std::unique_ptr<juce::FileChooser>  fileChooser;

    juce::LookAndFeel_V4 darkLook;

    void globalFocusChanged(juce::Component* focusedComponent) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
