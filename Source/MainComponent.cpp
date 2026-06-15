#include "MainComponent.h"
#include "ModuleRegistry.h"
#include "AppState.h"

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

    // Restore saved window positions (keyed by module name, survives reorder)
    if (auto* props = getAppProperties())
    {
        for (size_t i = 0; i < moduleWindows.size(); ++i)
        {
            auto state = props->getValue("moduleWindow_" + modules[i]->getName());
            if (state.isNotEmpty())
                moduleWindows[i]->restoreWindowStateFromString(state);
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

    juce::Desktop::getInstance().addFocusChangeListener(this);

    setSize(720, 260);
    setAudioChannels(0, 2);

    for (auto& dev : juce::MidiInput::getAvailableDevices())
    {
        deviceManager.setMidiInputDeviceEnabled(dev.identifier, true);
        deviceManager.addMidiInputDeviceCallback(dev.identifier, &midiCollector);
    }
}

MainComponent::~MainComponent()
{
    // Save each module window's position/size before teardown
    if (auto* props = getAppProperties())
    {
        for (size_t i = 0; i < moduleWindows.size(); ++i)
            props->setValue("moduleWindow_" + modules[i]->getName(),
                            moduleWindows[i]->getWindowStateAsString());
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

    synthEngine.renderNextBlock(*info.buffer, midi, info.startSample, info.numSamples);

    for (auto& m : modules)
        if (m->isEnabled())
            m->processBlock(*info.buffer, midi, info.startSample, info.numSamples);

    info.buffer->applyGain(info.startSample, info.numSamples,
                           (float)volumeSlider.getValue());
}

void MainComponent::releaseResources() {}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1a1a2e));
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced(12);

    keyboard.setBounds(area.removeFromBottom(110));
    area.removeFromBottom(8);

    auto knobArea = area.removeFromRight(90);
    volumeLabel.setBounds(knobArea.removeFromTop(20));
    volumeSlider.setBounds(knobArea);

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

juce::MenuBarModel* MainComponent::createMenuModel()
{
    menuModel = std::make_unique<SetupMenuModel>(*this);
    return menuModel.get();
}
