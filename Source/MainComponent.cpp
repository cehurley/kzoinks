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

    // Instantiate modules and open each one in its own floating window
    modules = ModuleRegistry::getInstance().createAll(synthEngine.params);
    for (int i = 0; i < (int)modules.size(); ++i)
    {
        auto& m = modules[(size_t)i];
        moduleWindows.push_back(
            std::make_unique<ModuleWindow>(m->getName(),
                                          m->createEditor(),
                                          i));
    }

    // Build the chain strip from the current module order
    auto buildChainStrip = [this]
    {
        juce::StringArray names;
        std::vector<ModuleWindow*> ptrs;
        for (size_t i = 0; i < modules.size(); ++i)
        {
            names.add(modules[i]->getName());
            ptrs.push_back(moduleWindows[i].get());
        }
        chainStrip.setModules(names, ptrs);
    };

    buildChainStrip();

    // Restore saved window positions and visibility (keyed by module name, survives reorder)
    if (auto* props = getAppProperties())
    {
        for (size_t i = 0; i < moduleWindows.size(); ++i)
        {
            const auto key = modules[i]->getName();
            auto state = props->getValue("moduleWindow_" + key);
            if (state.isNotEmpty())
                moduleWindows[i]->restoreWindowStateFromString(state);

            const bool visible = props->getBoolValue("moduleWindow_" + key + "_visible", true);
            moduleWindows[i]->setVisible(visible);
        }
    }

    // Forward key events from module windows to the keyboard so note-offs are
    // received even when a module window has OS focus (prevents hanging notes).
    for (auto& win : moduleWindows)
        win->addKeyListener(keyboard.asKeyListener());

    // Drag-to-reorder: keep modules and windows in sync with chain strip rows
    chainStrip.onReorder = [this](int from, int to)
    {
        auto m = std::move(modules[(size_t)from]);
        modules.erase(modules.begin() + from);
        modules.insert(modules.begin() + to, std::move(m));

        auto w = std::move(moduleWindows[(size_t)from]);
        moduleWindows.erase(moduleWindows.begin() + from);
        moduleWindows.insert(moduleWindows.begin() + to, std::move(w));
    };

    // Power button: enable / disable a module in the signal chain
    chainStrip.onToggle = [this](int index, bool enabled)
    {
        modules[(size_t)index]->setEnabled(enabled);
    };

    // Click a row name: show the window and bring it to front
    chainStrip.onShow = [this](int index)
    {
        auto* win = moduleWindows[(size_t)index].get();
        win->setVisible(true);
        win->toFront(true);
    };

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
    // Save each module window's position, size, and visibility before teardown
    if (auto* props = getAppProperties())
    {
        for (size_t i = 0; i < moduleWindows.size(); ++i)
        {
            const auto key = modules[i]->getName();
            props->setValue("moduleWindow_" + key,
                            moduleWindows[i]->getWindowStateAsString());
            props->setValue("moduleWindow_" + key + "_visible",
                            moduleWindows[i]->isVisible());
        }
        props->saveIfNeeded();
    }

    for (auto& win : moduleWindows)
        win->removeKeyListener(keyboard.asKeyListener());

    juce::Desktop::getInstance().removeFocusChangeListener(this);

    for (auto& dev : juce::MidiInput::getAvailableDevices())
        deviceManager.removeMidiInputDeviceCallback(dev.identifier, &midiCollector);

    setLookAndFeel(nullptr);
    shutdownAudio();
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
    for (auto& m : modules)
        m->prepareToPlay(sampleRate, samplesPerBlock);
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

    synthEngine.renderNextBlock(*info.buffer, remapped, info.startSample, info.numSamples);

    for (auto& m : modules)
        if (m->isEnabled())
            m->processBlock(*info.buffer, remapped, info.startSample, info.numSamples);

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

    // Build XML for this setup
    juce::XmlElement root("setup");
    for (size_t i = 0; i < modules.size(); ++i)
    {
        auto* entry = root.createNewChildElement("module");
        entry->setAttribute("name",    modules[i]->getName());
        entry->setAttribute("enabled", modules[i]->isEnabled());
        modules[i]->saveState(*entry);
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

    auto xml = props->getValue("setup." + name);
    if (xml.isEmpty()) return;

    auto root = juce::XmlDocument::parse(xml);
    if (!root) return;

    // Match saved module entries to current modules by name
    for (auto* entry : root->getChildIterator())
    {
        auto modName = entry->getStringAttribute("name");
        for (size_t i = 0; i < modules.size(); ++i)
        {
            if (modules[i]->getName() == modName)
            {
                modules[i]->setEnabled(entry->getBoolAttribute("enabled", true));
                modules[i]->loadState(*entry);

                // Rebuild the editor so controls reflect loaded values
                moduleWindows[i]->setContentOwned(modules[i]->createEditor().release(), false);
                break;
            }
        }
    }

    currentSetupName = name;
    props->setValue("setup.current", name);
    props->saveIfNeeded();

    // Refresh the chain strip to reflect any enabled-state changes
    {
        juce::StringArray names;
        std::vector<ModuleWindow*> ptrs;
        std::vector<bool> enabled;
        for (size_t i = 0; i < modules.size(); ++i)
        {
            names.add(modules[i]->getName());
            ptrs.push_back(moduleWindows[i].get());
            enabled.push_back(modules[i]->isEnabled());
        }
        chainStrip.setModules(names, ptrs, enabled);
    }

    if (menuModel)
        menuModel->menuItemsChanged();
}

void MainComponent::exportSetups()
{
    auto* props = getAppProperties();
    if (!props) return;

    const auto names = getSetupNames();
    if (names.isEmpty()) return;

    // Build JSON: { "version": 1, "current": "...", "setups": [...] }
    auto setupsArray = juce::Array<juce::var>();

    for (const auto& name : names)
    {
        auto xmlStr = props->getValue("setup." + name);
        auto root   = juce::XmlDocument::parse(xmlStr);
        if (!root) continue;

        auto modulesArray = juce::Array<juce::var>();

        for (auto* entry : root->getChildIterator())
        {
            auto moduleObj = std::make_unique<juce::DynamicObject>();
            moduleObj->setProperty("name",    entry->getStringAttribute("name"));
            moduleObj->setProperty("enabled", entry->getBoolAttribute("enabled", true));

            for (int i = 0; i < entry->getNumAttributes(); ++i)
            {
                const auto attrName = entry->getAttributeName(i);
                if (attrName == "name" || attrName == "enabled") continue;

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
    root->setProperty("version", 1);
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
