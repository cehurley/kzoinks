#include "OutputPanel.h"

OutputPanel::OutputPanel(juce::AudioDeviceManager& dm,
                         std::atomic<float>& vuL,
                         std::atomic<float>& vuR)
    : deviceManager(dm), vuLeft(vuL), vuRight(vuR)
{
    deviceLabel.setText("Output", juce::dontSendNotification);
    deviceLabel.setFont(juce::Font(10.0f, juce::Font::bold));
    deviceLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.6f));
    addAndMakeVisible(deviceLabel);

    pairLabel.setText("Channels", juce::dontSendNotification);
    pairLabel.setFont(juce::Font(10.0f));
    pairLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.6f));
    addAndMakeVisible(pairLabel);

    deviceBox.onChange = [this] { onDeviceChanged(); };
    addAndMakeVisible(deviceBox);

    pairBox.onChange = [this] { onPairChanged(); };
    addAndMakeVisible(pairBox);

    deviceManager.addChangeListener(this);
    populateDevices();

    startTimerHz(30);
}

OutputPanel::~OutputPanel()
{
    deviceManager.removeChangeListener(this);
}

// ---- Device / channel population -------------------------------------------

void OutputPanel::populateDevices()
{
    deviceBox.onChange = nullptr;

    auto* type = deviceManager.getCurrentDeviceTypeObject();
    if (!type) return;

    const auto names   = type->getDeviceNames(false);
    const auto current = deviceManager.getCurrentAudioDevice()
                         ? deviceManager.getCurrentAudioDevice()->getName()
                         : juce::String{};

    deviceBox.clear(juce::dontSendNotification);
    for (int i = 0; i < names.size(); ++i)
        deviceBox.addItem(names[i], i + 1);

    const int idx = names.indexOf(current);
    deviceBox.setSelectedId(idx >= 0 ? idx + 1 : 1, juce::dontSendNotification);

    deviceBox.onChange = [this] { onDeviceChanged(); };

    populatePairs();
}

void OutputPanel::populatePairs()
{
    pairBox.onChange = nullptr;

    auto* dev = deviceManager.getCurrentAudioDevice();
    if (!dev) { pairBox.clear(); return; }

    const auto chNames  = dev->getOutputChannelNames();
    const int  numPairs = chNames.size() / 2;

    juce::AudioDeviceManager::AudioDeviceSetup setup;
    deviceManager.getAudioDeviceSetup(setup);

    pairBox.clear(juce::dontSendNotification);
    for (int i = 0; i < numPairs; ++i)
    {
        auto L = chNames[i * 2]    .trimCharactersAtEnd(" LlMm");
        auto R = chNames[i * 2 + 1].trimCharactersAtEnd(" RrMm");
        auto label = L.isNotEmpty() && R.isNotEmpty()
                     ? L + " + " + R
                     : juce::String(i * 2 + 1) + " + " + juce::String(i * 2 + 2);
        pairBox.addItem(label, i + 1);
    }

    int activePair = 0;
    for (int i = 0; i < numPairs; ++i)
        if (setup.outputChannels[i * 2] || setup.outputChannels[i * 2 + 1])
            { activePair = i; break; }

    pairBox.setSelectedId(activePair + 1, juce::dontSendNotification);
    pairBox.onChange = [this] { onPairChanged(); };
}

// ---- Callbacks -------------------------------------------------------------

void OutputPanel::onDeviceChanged()
{
    const int id = deviceBox.getSelectedId();
    if (id <= 0) return;

    auto* type = deviceManager.getCurrentDeviceTypeObject();
    if (!type) return;

    const auto names = type->getDeviceNames(false);
    if (id - 1 >= names.size()) return;

    juce::AudioDeviceManager::AudioDeviceSetup setup;
    deviceManager.getAudioDeviceSetup(setup);
    setup.outputDeviceName       = names[id - 1];
    setup.useDefaultOutputChannels = true;
    deviceManager.setAudioDeviceSetup(setup, true);

    populatePairs();
}

void OutputPanel::onPairChanged()
{
    const int pair = pairBox.getSelectedId() - 1;
    if (pair < 0) return;

    juce::AudioDeviceManager::AudioDeviceSetup setup;
    deviceManager.getAudioDeviceSetup(setup);
    setup.outputChannels.clear();
    setup.outputChannels.setBit(pair * 2);
    setup.outputChannels.setBit(pair * 2 + 1);
    setup.useDefaultOutputChannels = false;
    deviceManager.setAudioDeviceSetup(setup, true);
}

void OutputPanel::changeListenerCallback(juce::ChangeBroadcaster*)
{
    juce::MessageManager::callAsync([this] { populateDevices(); });
}

// ---- Timer -----------------------------------------------------------------

void OutputPanel::timerCallback()
{
    const float coeff = 0.3f;
    dispLeft  += coeff * (vuLeft .load() - dispLeft);
    dispRight += coeff * (vuRight.load() - dispRight);

    // Decay stored peaks so meter falls naturally
    vuLeft .store(vuLeft .load() * 0.88f);
    vuRight.store(vuRight.load() * 0.88f);

    repaint();
}

// ---- Drawing ---------------------------------------------------------------

static void drawVUBar(juce::Graphics& g, juce::Rectangle<int> b,
                      float level, const juce::String& chan)
{
    g.setColour(juce::Colour(0xff080808));
    g.fillRoundedRectangle(b.toFloat(), 3.0f);

    const float db   = 20.0f * std::log10(std::max(0.00001f, level));
    const float norm = juce::jlimit(0.0f, 1.0f, (db + 48.0f) / 48.0f);
    const int   fillH = (int)((float)(b.getHeight() - 14) * norm);

    auto meterBounds = b.reduced(3).withTrimmedBottom(14);

    // Draw filled bar from bottom
    auto fillArea = meterBounds.removeFromBottom(fillH);

    // Colour: green → yellow → red
    const float redThresh  = 0.875f; // ~-6 dB
    const float yellThresh = 0.65f;  // ~-16 dB

    if (norm >= redThresh)
    {
        int redH = (int)((float)fillH * ((norm - redThresh) / (1.0f - redThresh)));
        auto greenPart = fillArea.removeFromBottom(fillH - redH);
        g.setColour(juce::Colour(0xff22aa33));
        g.fillRect(greenPart);
        if (norm > yellThresh)
        {
            int yellH = (int)((float)(fillH - redH) * ((norm - yellThresh) / (redThresh - yellThresh)));
            auto yellPart = greenPart.removeFromTop(yellH);
            g.setColour(juce::Colour(0xffddcc00));
            g.fillRect(yellPart);
        }
        g.setColour(juce::Colour(0xffee2222));
        g.fillRect(fillArea);
    }
    else if (norm >= yellThresh)
    {
        int yellH = (int)((float)fillH * ((norm - yellThresh) / (redThresh - yellThresh)));
        auto greenPart = fillArea.removeFromBottom(fillH - yellH);
        g.setColour(juce::Colour(0xff22aa33));
        g.fillRect(greenPart);
        g.setColour(juce::Colour(0xffddcc00));
        g.fillRect(fillArea);
    }
    else
    {
        g.setColour(juce::Colour(0xff22aa33));
        g.fillRect(fillArea);
    }

    // Channel label
    g.setColour(juce::Colours::white.withAlpha(0.5f));
    g.setFont(juce::Font(9.0f, juce::Font::bold));
    g.drawText(chan, b.removeFromBottom(14), juce::Justification::centred);
}

void OutputPanel::paint(juce::Graphics& g)
{
    g.setColour(juce::Colour(0xff111122));
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 6.0f);

    if (vuBounds.getWidth() > 0)
    {
        auto vb = vuBounds;
        const int w = (vb.getWidth() - 4) / 2;
        drawVUBar(g, vb.removeFromLeft(w),  dispLeft,  "L");
        vb.removeFromLeft(4);
        drawVUBar(g, vb,                    dispRight, "R");
    }
}

void OutputPanel::resized()
{
    auto area = getLocalBounds().reduced(6);

    deviceLabel.setBounds(area.removeFromTop(13));
    deviceBox  .setBounds(area.removeFromTop(22));
    area.removeFromTop(4);

    pairLabel.setBounds(area.removeFromTop(13));
    pairBox  .setBounds(area.removeFromTop(22));
    area.removeFromTop(6);

    vuBounds = area;
}
