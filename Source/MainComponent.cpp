#include "MainComponent.h"
#include "ModuleRegistry.h"
#include "AppState.h"
#include <AppAssetData.h>

// ---- Menu bar model --------------------------------------------------------

namespace
{

class SetupMenuModel : public juce::MenuBarModel
{
public:
    SetupMenuModel(MainComponent& mc) : main(mc) {}

    juce::StringArray getMenuBarNames() override
    {
        return { "Setups" };
    }

    juce::PopupMenu getMenuForIndex(int index, const juce::String&) override
    {
        juce::PopupMenu menu;
        if (index == 0)
        {
            menu.addItem(1, "New Setup...");
            menu.addItem(2, "Save Setup",
                         main.currentSetupName.isNotEmpty(),
                         false);
            menu.addItem(3, "Export Setups...",
                         !main.getSetupNames().isEmpty(),
                         false);
            menu.addSeparator();

            const auto names = main.getSetupNames();
            for (int i = 0; i < names.size(); ++i)
            {
                const bool isCurrent = (names[i] == main.currentSetupName);
                menu.addItem(100 + i, names[i], true, isCurrent);
            }

            if (names.isEmpty())
            {
                menu.addItem(-1, "(no setups saved yet)", false, false);
            }
        }
        return menu;
    }

    void menuItemSelected(int id, int) override
    {
        if      (id == 1)   main.showNewSetupDialog();
        else if (id == 2)   main.saveCurrentSetup();
        else if (id == 3)   main.exportSetups();
        else if (id >= 100) main.loadSetup(main.getSetupNames()[id - 100]);
    }

    void menuBarActivated(bool) override {}

private:
    MainComponent& main;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SetupMenuModel)
};

} // namespace

MainComponent::MainComponent()
    : keyboard(keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    darkLook.setColourScheme(juce::LookAndFeel_V4::getDarkColourScheme());
    setLookAndFeel(&darkLook);

    kzoinksImage = juce::ImageCache::getFromMemory(AppAssets::kzoinks_png,
                                                  AppAssets::kzoinks_pngSize);

    addAndMakeVisible(volumeSlider);
    volumeSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    volumeSlider.setRange(0.0, 1.0);
    volumeSlider.setValue(0.7, juce::dontSendNotification);
    volumeSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);

    addAndMakeVisible(volumeLabel);
    volumeLabel.setText("Volume", juce::dontSendNotification);
    volumeLabel.setJustificationType(juce::Justification::centred);

    addAndMakeVisible(keyboard);
    keyboard.setAvailableRange(36, 96);
    keyboard.setKeyPressBaseOctave(keyboardOctave);

    octaveLabel.setText("Oct " + juce::String(keyboardOctave), juce::dontSendNotification);
    octaveLabel.setJustificationType(juce::Justification::centred);
    octaveLabel.setFont(juce::Font(11.0f));
    addAndMakeVisible(octaveLabel);

    auto shiftOctave = [this](int delta)
    {
        keyboardOctave = juce::jlimit(0, 10, keyboardOctave + delta);
        keyboard.setKeyPressBaseOctave(keyboardOctave);
        octaveLabel.setText("Oct " + juce::String(keyboardOctave), juce::dontSendNotification);
        octaveDownBtn.setEnabled(keyboardOctave > 0);
        octaveUpBtn  .setEnabled(keyboardOctave < 10);
    };

    octaveDownBtn.onClick = [shiftOctave] { shiftOctave(-1); };
    octaveUpBtn  .onClick = [shiftOctave] { shiftOctave(+1); };
    addAndMakeVisible(octaveDownBtn);
    addAndMakeVisible(octaveUpBtn);

    // The core synth voice always exists, outside the swappable FX rack below.
    voiceModule = ModuleRegistry::getInstance().createByName("Voice", synthEngine.params);
    jassert(voiceModule != nullptr);
    voiceWindow = std::make_unique<ModuleWindow>(voiceModule->getName(), voiceModule->createEditor(), 0);
    {
        auto logo = voiceModule->getLogo();
        if (logo.isValid())
            voiceWindow->setTitleLogo(logo);
    }
    voiceWindow->addKeyListener(keyboard.asKeyListener());
    voiceButton.onClick = [this]
    {
        voiceWindow->setVisible(true);
        voiceWindow->toFront(true);
    };
    addAndMakeVisible(voiceButton);

    // Catalog of module types offered in each FX slot's dropdown — everything
    // except "Voice", which isn't a swappable insert.
    fxCatalog = ModuleRegistry::getInstance().getAvailableNames(synthEngine.params);
    fxCatalog.removeString("Voice");
    chainStrip.setCatalog(fxCatalog);

    // FX slots start empty; the auto-loaded setup (see Main.cpp) populates them.
    refreshChainStripDisplay();

    // Drag-to-reorder: swap module+window between the two slots. The module
    // pointer swap is locked because the audio thread iterates fxSlots every
    // block; the window swap isn't (windows are message-thread-only).
    chainStrip.onReorder = [this](int from, int to)
    {
        {
            const juce::ScopedLock sl(fxSlotsLock);
            std::swap(fxSlots[(size_t)from].module, fxSlots[(size_t)to].module);
        }
        std::swap(fxSlots[(size_t)from].window, fxSlots[(size_t)to].window);
        refreshChainStripDisplay();
    };

    // Power button: enable / disable a slot's module (IModule's enabled flag is
    // already an atomic, so no extra locking needed here).
    chainStrip.onToggle = [this](int index, bool enabled)
    {
        if (auto& m = fxSlots[(size_t)index].module)
            m->setEnabled(enabled);
    };

    // Click a filled slot's visibility dot: show the window and bring it to front.
    chainStrip.onShow = [this](int index)
    {
        if (auto* win = fxSlots[(size_t)index].window.get())
        {
            win->setVisible(true);
            win->toFront(true);
        }
    };

    // Dropdown: replace (or clear) this slot's module type.
    chainStrip.onTypeChange = [this](int index, const juce::String& newType)
    {
        assignSlot(index, newType);
        refreshChainStripDisplay();
    };

    // Restore the voice window's saved position/visibility. Each FX slot
    // window's geometry is restored lazily as that slot is assigned a module
    // (see assignSlot) since slots start empty here.
    if (auto* props = getAppProperties())
    {
        auto state = props->getValue("moduleWindow_voice");
        if (state.isNotEmpty())
            voiceWindow->restoreWindowStateFromString(state);
        voiceWindow->setVisible(props->getBoolValue("moduleWindow_voice_visible", true));
    }

    addAndMakeVisible(chainStrip);
    addAndMakeVisible(outputPanel);

    juce::Desktop::getInstance().addFocusChangeListener(this);

    setSize(860, 300);
    setAudioChannels(0, 2);

    for (auto& dev : juce::MidiInput::getAvailableDevices())
    {
        deviceManager.setMidiInputDeviceEnabled(dev.identifier, true);
        deviceManager.addMidiInputDeviceCallback(dev.identifier, &midiCollector);
    }
}

MainComponent::~MainComponent()
{
    // Save the voice window and each filled FX slot's position/size/visibility
    // before teardown, keyed by slot index (not module name — duplicates are
    // allowed now, so name alone wouldn't be a stable key).
    if (auto* props = getAppProperties())
    {
        props->setValue("moduleWindow_voice", voiceWindow->getWindowStateAsString());
        props->setValue("moduleWindow_voice_visible", voiceWindow->isVisible());

        for (int i = 0; i < numFxSlots; ++i)
        {
            auto& slot = fxSlots[(size_t)i];
            if (!slot.window) continue;
            const auto key = slotWindowKey(i);
            props->setValue(key, slot.window->getWindowStateAsString());
            props->setValue(key + "_visible", slot.window->isVisible());
        }
        props->saveIfNeeded();
    }

    voiceWindow->removeKeyListener(keyboard.asKeyListener());
    for (auto& slot : fxSlots)
        if (slot.window)
            slot.window->removeKeyListener(keyboard.asKeyListener());

    juce::Desktop::getInstance().removeFocusChangeListener(this);

    for (auto& dev : juce::MidiInput::getAvailableDevices())
        deviceManager.removeMidiInputDeviceCallback(dev.identifier, &midiCollector);

    setLookAndFeel(nullptr);
    shutdownAudio();
}

// ---- FX slot management -----------------------------------------------------

void MainComponent::clearSlot(int index)
{
    auto& slot = fxSlots[(size_t)index];

    // Detach the module pointer under the lock, but destroy it afterwards —
    // keeps the locked section to a bare pointer swap, never object teardown.
    std::unique_ptr<IModule> oldModule;
    {
        const juce::ScopedLock sl(fxSlotsLock);
        oldModule = std::move(slot.module);
    }

    if (slot.window)
    {
        slot.window->removeKeyListener(keyboard.asKeyListener());
        slot.window.reset();
    }
}

void MainComponent::assignSlot(int index, const juce::String& typeName)
{
    clearSlot(index);
    if (typeName.isEmpty())
        return;

    auto newModule = ModuleRegistry::getInstance().createByName(typeName, synthEngine.params);
    if (!newModule) return;

    // Built and prepared fully before it's ever visible to the audio thread.
    newModule->prepareToPlay(lastSampleRate, lastBlockSize);

    auto win = std::make_unique<ModuleWindow>(newModule->getName(), newModule->createEditor(), index);
    auto logo = newModule->getLogo();
    if (logo.isValid())
        win->setTitleLogo(logo);
    win->addKeyListener(keyboard.asKeyListener());

    if (auto* props = getAppProperties())
    {
        const auto key   = slotWindowKey(index);
        const auto state = props->getValue(key);
        if (state.isNotEmpty())
            win->restoreWindowStateFromString(state);
        win->setVisible(props->getBoolValue(key + "_visible", true));
    }

    auto& slot = fxSlots[(size_t)index];
    {
        const juce::ScopedLock sl(fxSlotsLock);
        slot.module = std::move(newModule);
    }
    slot.window = std::move(win);
}

void MainComponent::refreshChainStripDisplay()
{
    std::vector<ChainStrip::SlotData> data;
    for (auto& slot : fxSlots)
    {
        ChainStrip::SlotData d;
        if (slot.module)
        {
            d.typeName = slot.module->getName();
            d.window   = slot.window.get();
            d.enabled  = slot.module->isEnabled();
        }
        data.push_back(d);
    }
    chainStrip.setSlots(std::move(data));
}

void MainComponent::globalFocusChanged(juce::Component* focusedComponent)
{
    if (focusedComponent == nullptr)
    {
        keyboard.silenceAllNotes();
        synthEngine.turnOffAllVoices(true);
    }
}

void MainComponent::prepareToPlay(int samplesPerBlock, double sampleRate)
{
    midiCollector.reset(sampleRate);
    synthEngine.setCurrentPlaybackSampleRate(sampleRate);

    lastSampleRate = sampleRate;
    lastBlockSize  = samplesPerBlock;

    voiceModule->prepareToPlay(sampleRate, samplesPerBlock);

    const juce::ScopedLock sl(fxSlotsLock);
    for (auto& slot : fxSlots)
        if (slot.module)
            slot.module->prepareToPlay(sampleRate, samplesPerBlock);
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& info)
{
    info.clearActiveBufferRegion();

    juce::MidiBuffer midi;
    midiCollector.removeNextBlockOfMessages(midi, info.numSamples);
    keyboardState.processNextMidiBuffer(midi, info.startSample, info.numSamples, true);

    // MPE lower zone: ch1 is the master channel — note-on/off sent there are ignored
    // by MPESynthesiser. Remap legacy (non-MPE) note messages to ch2 so external
    // keyboards not in MPE mode still trigger voices.
    juce::MidiBuffer remapped;
    for (auto meta : midi)
    {
        auto msg = meta.getMessage();
        if (msg.getChannel() == 1 && (msg.isNoteOn() || msg.isNoteOff()))
            msg = juce::MidiMessage(msg.getRawData()[0] | 0x01,
                                   msg.getRawData()[1],
                                   msg.getRawData()[2],
                                   msg.getTimeStamp());
        remapped.addEvent(msg, meta.samplePosition);
    }

    {
        // Locked for the whole render pass — held only briefly and rarely
        // contended (only when a slot is mid-reassignment on the message
        // thread; see assignSlot/clearSlot).
        const juce::ScopedLock sl(fxSlotsLock);

        // Note generators (arp, step sequencer) get first crack at the MIDI
        // buffer so notes they add/remove actually reach the engine's render
        // this block.
        for (auto& slot : fxSlots)
            if (slot.module && slot.module->isEnabled())
                slot.module->processMidi(remapped, info.startSample, info.numSamples);

        synthEngine.renderNextBlock(*info.buffer, remapped, info.startSample, info.numSamples);

        for (auto& slot : fxSlots)
            if (slot.module && slot.module->isEnabled())
                slot.module->processBlock(*info.buffer, remapped, info.startSample, info.numSamples);
    }

    info.buffer->applyGain(info.startSample, info.numSamples,
                           (float)volumeSlider.getValue());

    // Feed VU meter — peak over block, with decay so meter falls between frames
    const int   nCh  = info.buffer->getNumChannels();
    float pkL = 0.0f, pkR = 0.0f;
    if (nCh > 0) pkL = info.buffer->getMagnitude(0, info.startSample, info.numSamples);
    if (nCh > 1) pkR = info.buffer->getMagnitude(1, info.startSample, info.numSamples);
    vuLeft .store(std::max(pkL, vuLeft .load() * 0.92f));
    vuRight.store(std::max(pkR, vuRight.load() * 0.92f));
}

void MainComponent::releaseResources() {}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1a1a2e));

    if (kzoinksImage.isValid() && !skullBounds.isEmpty())
        g.drawImageWithin(kzoinksImage,
                          skullBounds.getX(), skullBounds.getY(),
                          skullBounds.getWidth(), skullBounds.getHeight(),
                          juce::RectanglePlacement::centred |
                          juce::RectanglePlacement::onlyReduceInSize);
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced(12);

    auto keyboardArea = area.removeFromBottom(110);

    // Octave shift controls sit in the top-left corner of the keyboard area
    auto octaveRow = keyboardArea.removeFromTop(20);
    octaveDownBtn.setBounds(octaveRow.removeFromLeft(24));
    octaveLabel  .setBounds(octaveRow.removeFromLeft(44));
    octaveUpBtn  .setBounds(octaveRow.removeFromLeft(24));

    keyboard.setBounds(keyboardArea);
    area.removeFromBottom(8);

    // Output panel — right edge
    auto outputArea = area.removeFromRight(160);
    outputPanel.setBounds(outputArea);
    area.removeFromRight(8);

    // Volume knob + skull image
    auto knobArea = area.removeFromRight(90);
    volumeLabel .setBounds(knobArea.removeFromTop(20));
    volumeSlider.setBounds(knobArea.removeFromTop(80));
    skullBounds = knobArea; // skull fills remaining space below knob
    area.removeFromRight(8);

    voiceButton.setBounds(area.removeFromTop(26));
    area.removeFromTop(6);

    chainStrip.setBounds(area);
}

// ---- Setup persistence -----------------------------------------------------

juce::StringArray MainComponent::getSetupNames() const
{
    if (auto* props = getAppProperties())
    {
        auto raw = props->getValue("setup.names");
        if (raw.isNotEmpty())
            return juce::StringArray::fromTokens(raw, "|", "");
    }
    return {};
}

void MainComponent::showNewSetupDialog()
{
    auto dialog = std::make_shared<juce::AlertWindow>("New Setup",
                                                       "Enter a name for this setup:",
                                                       juce::MessageBoxIconType::NoIcon);
    dialog->addTextEditor("name", currentSetupName.isNotEmpty() ? currentSetupName : "My Setup");
    dialog->addButton("Save",   1);
    dialog->addButton("Cancel", 0);

    dialog->enterModalState(true,
        juce::ModalCallbackFunction::create([this, dialog](int result)
        {
            if (result == 1)
            {
                auto name = dialog->getTextEditorContents("name").trim();
                if (name.isNotEmpty())
                    saveSetupAs(name);
            }
        }),
        false);
}

void MainComponent::saveSetupAs(const juce::String& name)
{
    auto* props = getAppProperties();
    if (!props) return;

    // Build XML for this setup: one <voice> (always present) plus one <slot>
    // per filled FX slot, keyed by index so duplicate module types don't collide.
    juce::XmlElement root("setup");

    auto* voiceEl = root.createNewChildElement("voice");
    voiceModule->saveState(*voiceEl);

    for (int i = 0; i < numFxSlots; ++i)
    {
        auto& slot = fxSlots[(size_t)i];
        if (!slot.module) continue;

        auto* entry = root.createNewChildElement("slot");
        entry->setAttribute("index",      i);
        // Named "moduleType" (not "type") because some modules' own saveState
        // writes an attribute literally called "type" (e.g. DistortionModule's
        // shape index) — using the same name here would get clobbered when
        // slot.module->saveState(*entry) runs right below, silently corrupting
        // the slot's module-type-on-load lookup.
        entry->setAttribute("moduleType", slot.module->getName());
        entry->setAttribute("enabled",    slot.module->isEnabled());
        slot.module->saveState(*entry);
    }

    props->setValue("setup." + name, root.toString());

    // Add to the names list if not already present
    auto names = getSetupNames();
    if (!names.contains(name))
    {
        names.add(name);
        props->setValue("setup.names", names.joinIntoString("|"));
    }

    currentSetupName = name;
    props->setValue("setup.current", name);
    props->saveIfNeeded();

    if (menuModel)
        menuModel->menuItemsChanged();
}

void MainComponent::saveCurrentSetup()
{
    if (currentSetupName.isNotEmpty())
        saveSetupAs(currentSetupName);
}

void MainComponent::loadSetup(const juce::String& name)
{
    auto* props = getAppProperties();
    if (!props) return;

    auto xmlStr = props->getValue("setup." + name);
    if (xmlStr.isEmpty()) return;

    auto root = juce::XmlDocument::parse(xmlStr);
    if (!root) return;

    if (auto* voiceEl = root->getChildByName("voice"))
    {
        voiceModule->loadState(*voiceEl);
        voiceWindow->setContentOwned(voiceModule->createEditor().release(), false);
    }

    for (int i = 0; i < numFxSlots; ++i)
        clearSlot(i);

    for (auto* entry : root->getChildIterator())
    {
        if (entry->getTagName() != "slot") continue;

        const int idx = entry->getIntAttribute("index", -1);
        if (idx < 0 || idx >= numFxSlots) continue;

        assignSlot(idx, entry->getStringAttribute("moduleType"));

        auto& slot = fxSlots[(size_t)idx];
        if (slot.module)
        {
            slot.module->setEnabled(entry->getBoolAttribute("enabled", true));
            slot.module->loadState(*entry);

            // Rebuild the editor so controls reflect loaded values
            slot.window->setContentOwned(slot.module->createEditor().release(), false);
        }
    }

    currentSetupName = name;
    props->setValue("setup.current", name);
    props->saveIfNeeded();

    refreshChainStripDisplay();

    if (menuModel)
        menuModel->menuItemsChanged();
}

void MainComponent::exportSetups()
{
    auto* props = getAppProperties();
    if (!props) return;

    const auto names = getSetupNames();
    if (names.isEmpty()) return;

    // Build JSON: { "version": 2, "current": "...", "setups": [...] }
    auto setupsArray = juce::Array<juce::var>();

    for (const auto& name : names)
    {
        auto xmlStr = props->getValue("setup." + name);
        auto root   = juce::XmlDocument::parse(xmlStr);
        if (!root) continue;

        auto modulesArray = juce::Array<juce::var>();

        for (auto* entry : root->getChildIterator())
        {
            const bool isVoice = entry->getTagName() == "voice";
            const bool isSlot  = entry->getTagName() == "slot";
            if (!isVoice && !isSlot) continue;

            auto moduleObj = std::make_unique<juce::DynamicObject>();
            moduleObj->setProperty("name",    isVoice ? juce::String("Voice")
                                                       : entry->getStringAttribute("type"));
            moduleObj->setProperty("enabled", isVoice ? true : entry->getBoolAttribute("enabled", true));
            if (isSlot)
                moduleObj->setProperty("slotIndex", entry->getIntAttribute("index"));

            for (int i = 0; i < entry->getNumAttributes(); ++i)
            {
                const auto attrName = entry->getAttributeName(i);
                if (attrName == "type" || attrName == "enabled" || attrName == "index") continue;

                const auto val = entry->getAttributeValue(i);
                // Store as number if parseable, otherwise string
                if (val.containsOnly("0123456789.-"))
                    moduleObj->setProperty(attrName, val.getDoubleValue());
                else
                    moduleObj->setProperty(attrName, val);
            }

            modulesArray.add(moduleObj.release());
        }

        auto setupObj = std::make_unique<juce::DynamicObject>();
        setupObj->setProperty("name",    name);
        setupObj->setProperty("modules", modulesArray);
        setupsArray.add(setupObj.release());
    }

    auto root = std::make_unique<juce::DynamicObject>();
    root->setProperty("version", 2);
    root->setProperty("current", currentSetupName);
    root->setProperty("setups",  setupsArray);

    const juce::String json = juce::JSON::toString(juce::var(root.release()), true);

    // File save dialog
    fileChooser = std::make_unique<juce::FileChooser>(
        "Export Setups",
        juce::File::getSpecialLocation(juce::File::userDesktopDirectory)
            .getChildFile("kzoinks-setups.json"),
        "*.json");

    fileChooser->launchAsync(
        juce::FileBrowserComponent::saveMode |
        juce::FileBrowserComponent::canSelectFiles |
        juce::FileBrowserComponent::warnAboutOverwriting,
        [json](const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file == juce::File{}) return;
            file.replaceWithText(json);
        });
}

juce::MenuBarModel* MainComponent::createMenuModel()
{
    menuModel = std::make_unique<SetupMenuModel>(*this);
    return menuModel.get();
}
