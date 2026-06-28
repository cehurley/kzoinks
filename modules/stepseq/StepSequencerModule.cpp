#include "StepSequencerModule.h"
#include "ModuleRegistry.h"
#include "StepSeqData.h"

// ---- DSP -------------------------------------------------------------------

StepSequencerModule::StepSequencerModule(SynthParameters&)
{
    for (auto& s : steps) s.store((int)StepState::Off);
}

void StepSequencerModule::prepareToPlay(double sr, int)
{
    sampleRate = sr;
    stepPhase  = 0.0;
    curStep    = 0;
    noteIsOn   = false;
    activeNote = -1;
    rootNote   = 60;
    heldNotes.clear();
    currentStep.store(-1);
}

void StepSequencerModule::processMidi(juce::MidiBuffer& midi,
                                      int startSample, int numSamples)
{
    if (trigPlayStop.exchange(false))
        running.store(!running.load());

    if (trigClear.exchange(false))
        for (auto& s : steps) s.store((int)StepState::Off);

    // Track held notes for the root pitch. While stopped, the sequencer is
    // transparent — notes pass through untouched so the keyboard plays normally.
    // Only while running does it take over note output, like an arpeggiator.
    const bool isRunning = running.load();
    juce::MidiBuffer passThrough;
    for (auto it : midi)
    {
        auto m = it.getMessage();
        if (m.isNoteOn())       heldNotes.insert(m.getNoteNumber());
        else if (m.isNoteOff()) heldNotes.erase(m.getNoteNumber());

        if (!isRunning || !(m.isNoteOn() || m.isNoteOff()))
            passThrough.addEvent(m, it.samplePosition);
    }
    rootNote = heldNotes.empty() ? 60 : *heldNotes.rbegin();
    rootNoteOut.store(rootNote);

    juce::MidiBuffer outMidi;
    outMidi.addEvents(passThrough, 0, -1, 0);

    if (!isRunning)
    {
        if (noteIsOn)
        {
            outMidi.addEvent(juce::MidiMessage::noteOff(1, activeNote), startSample);
            noteIsOn = false;
        }
        stepPhase = 0.0;
        curStep   = 0;
        currentStep.store(-1);
        midi = outMidi;
        return;
    }

    const double bpmVal    = bpm.load();
    const int    subdivVal = subdiv.load();
    const double samplesPerStep = (sampleRate * 60.0 / bpmVal) * (4.0 / subdivVal);
    const double noteOffPhase   = samplesPerStep * juce::jlimit(0.05f, 1.0f, gate.load());

    for (int i = 0; i < numSamples; ++i)
    {
        const int absPos = startSample + i;

        if (noteIsOn && stepPhase >= noteOffPhase)
        {
            outMidi.addEvent(juce::MidiMessage::noteOff(1, activeNote), absPos);
            noteIsOn = false;
        }

        if (stepPhase >= samplesPerStep)
        {
            stepPhase -= samplesPerStep;

            if (noteIsOn)
            {
                outMidi.addEvent(juce::MidiMessage::noteOff(1, activeNote), absPos);
                noteIsOn = false;
            }

            curStep = (curStep + 1) % numSteps;
            currentStep.store(curStep);

            const auto st = (StepState)steps[(size_t)curStep].load();
            if (st != StepState::Off)
            {
                const int offset = st == StepState::OctaveUp   ?  12
                                  : st == StepState::OctaveDown ? -12 : 0;
                activeNote = juce::jlimit(0, 127, rootNote + offset);
                outMidi.addEvent(juce::MidiMessage::noteOn(1, activeNote, (juce::uint8)100), absPos);
                noteIsOn = true;
            }
        }

        stepPhase += 1.0;
    }

    midi = outMidi;
}

// ---- Editor ----------------------------------------------------------------

namespace
{

class KnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    KnobLookAndFeel()
    {
        knobImage = juce::ImageCache::getFromMemory(
            StepSeqAssets::yellowknob_png, StepSeqAssets::yellowknob_pngSize);
    }

    void drawRotarySlider(juce::Graphics& g,
                          int x, int y, int width, int height,
                          float sliderPos,
                          float /*startAngle*/, float /*endAngle*/,
                          juce::Slider&) override
    {
        if (!knobImage.isValid()) return;

        const float angle = juce::MathConstants<float>::pi * 0.75f
                            * (2.0f * sliderPos - 1.0f);

        const int size = juce::jmin(width, height);
        const float cx = (float)x + (float)width  * 0.5f;
        const float cy = (float)y + (float)height * 0.5f;

        juce::Graphics::ScopedSaveState saved(g);
        g.addTransform(juce::AffineTransform::rotation(angle, cx, cy));
        g.drawImage(knobImage,
                    (int)(cx - size * 0.5f), (int)(cy - size * 0.5f),
                    size, size,
                    0, 0, knobImage.getWidth(), knobImage.getHeight());
    }

private:
    juce::Image knobImage;
};

juce::Colour stepColour(StepSequencerModule::StepState st)
{
    switch (st)
    {
        case StepSequencerModule::StepState::Off:        return juce::Colour(0xff1a1f28);
        case StepSequencerModule::StepState::Root:       return juce::Colour(0xff44ddaa);
        case StepSequencerModule::StepState::OctaveUp:   return juce::Colour(0xff44aaff);
        case StepSequencerModule::StepState::OctaveDown: return juce::Colour(0xffdd8844);
    }
    return juce::Colours::grey;
}

class StepButton : public juce::Component
{
public:
    StepButton(StepSequencerModule& m, int idx) : module(m), index(idx) {}

    void mouseDown(const juce::MouseEvent&) override
    {
        const int next = (module.steps[(size_t)index].load() + 1) % 4;
        module.steps[(size_t)index].store(next);
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        const auto st = (StepSequencerModule::StepState)module.steps[(size_t)index].load();
        const bool isCurrent = module.currentStep.load() == index;

        auto b = getLocalBounds().toFloat().reduced(1.5f);
        g.setColour(stepColour(st));
        g.fillRoundedRectangle(b, 3.0f);

        if (isCurrent)
        {
            g.setColour(juce::Colours::white.withAlpha(0.9f));
            g.drawRoundedRectangle(b, 3.0f, 1.6f);
        }

        // Beat markers: slightly brighter outline every 4th step
        if (index % 4 == 0)
        {
            g.setColour(juce::Colours::white.withAlpha(0.15f));
            g.drawRoundedRectangle(b, 3.0f, 1.0f);
        }
    }

private:
    StepSequencerModule& module;
    int index;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StepButton)
};

class StepSeqEditor : public juce::Component, private juce::Timer
{
public:
    explicit StepSeqEditor(StepSequencerModule& m) : module(m)
    {
        for (int i = 0; i < StepSequencerModule::numSteps; ++i)
        {
            auto btn = std::make_unique<StepButton>(module, i);
            addAndMakeVisible(*btn);
            stepButtons.push_back(std::move(btn));
        }

        playStopBtn.setButtonText("PLAY");
        playStopBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1a1a2a));
        playStopBtn.onClick = [this] { module.trigPlayStop.store(true); };
        addAndMakeVisible(playStopBtn);

        clearBtn.setButtonText("CLEAR");
        clearBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1a1a2a));
        clearBtn.onClick = [this] { module.trigClear.store(true); };
        addAndMakeVisible(clearBtn);

        for (auto* s : { &bpmKnob, &gateKnob })
            s->setLookAndFeel(&knobLook);

        auto setupKnob = [this](juce::Slider& s, juce::Label& l,
                                const juce::String& name,
                                double lo, double hi, double val, int decimals)
        {
            s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
            s.setRange(lo, hi);
            s.setValue(val, juce::dontSendNotification);
            s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 52, 14);
            s.setNumDecimalPlacesToDisplay(decimals);
            addAndMakeVisible(s);

            l.setText(name, juce::dontSendNotification);
            l.setJustificationType(juce::Justification::centred);
            l.setFont(juce::Font(10.0f));
            addAndMakeVisible(l);
        };

        setupKnob(bpmKnob,  bpmLabel,  "BPM",  40.0, 240.0, module.bpm.load(), 0);
        setupKnob(gateKnob, gateLabel, "Gate", 0.05, 1.0,   module.gate.load(), 2);

        bpmKnob.onValueChange  = [this] { module.bpm .store((float)bpmKnob.getValue()); };
        gateKnob.onValueChange = [this] { module.gate.store((float)gateKnob.getValue()); };

        rateBox.addItem("1/4",  1);
        rateBox.addItem("1/8",  2);
        rateBox.addItem("1/16", 3);
        rateBox.addItem("1/32", 4);
        auto subdivToId = [](int s) { return s == 4 ? 1 : s == 8 ? 2 : s == 16 ? 3 : 4; };
        auto idToSubdiv = [](int id) { return id == 1 ? 4 : id == 2 ? 8 : id == 3 ? 16 : 32; };
        rateBox.setSelectedId(subdivToId(module.subdiv.load()), juce::dontSendNotification);
        rateBox.onChange = [this, idToSubdiv] { module.subdiv.store(idToSubdiv(rateBox.getSelectedId())); };
        addAndMakeVisible(rateBox);

        rateLabel.setText("Rate", juce::dontSendNotification);
        rateLabel.setJustificationType(juce::Justification::centred);
        rateLabel.setFont(juce::Font(10.0f));
        addAndMakeVisible(rateLabel);

        startTimerHz(20);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff0d1117));

        auto titleBar = getLocalBounds().removeFromTop(22);
        g.setColour(juce::Colour(0xff1a1a2a));
        g.fillRoundedRectangle(titleBar.toFloat(), 6.0f);
        g.setColour(juce::Colour(0xff44ddaa));
        g.setFont(juce::Font(12.0f, juce::Font::bold));
        g.drawText("STEP SEQ", titleBar, juce::Justification::centred);

        g.setColour(juce::Colours::white.withAlpha(0.35f));
        g.setFont(juce::Font(10.0f));
        g.drawText(juce::MidiMessage::getMidiNoteName(module.rootNoteOut.load(), true, true, 4),
                   titleBar.removeFromRight(60), juce::Justification::centredRight);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(8);
        area.removeFromTop(22 + 6);

        auto controlRow = area.removeFromTop(70);
        const int cw = controlRow.getWidth() / 5;

        auto layoutKnob = [&](juce::Label& lbl, juce::Slider& knob)
        {
            auto col = controlRow.removeFromLeft(cw);
            lbl.setBounds(col.removeFromTop(14));
            knob.setBounds(col);
        };
        layoutKnob(bpmLabel, bpmKnob);
        layoutKnob(gateLabel, gateKnob);

        auto rateCol = controlRow.removeFromLeft(cw);
        rateLabel.setBounds(rateCol.removeFromTop(14));
        rateBox.setBounds(rateCol.removeFromTop(22));

        playStopBtn.setBounds(controlRow.removeFromLeft(cw).reduced(4, 22));
        clearBtn.setBounds(controlRow.removeFromLeft(cw).reduced(4, 22));

        area.removeFromTop(8);

        const int gap = 3;
        const int bw  = (area.getWidth() - gap * (StepSequencerModule::numSteps - 1))
                        / StepSequencerModule::numSteps;
        int x = area.getX();
        for (auto& btn : stepButtons)
        {
            btn->setBounds(x, area.getY(), bw, area.getHeight());
            x += bw + gap;
        }
    }

    ~StepSeqEditor() override
    {
        for (auto* s : { &bpmKnob, &gateKnob })
            s->setLookAndFeel(nullptr);
    }

private:
    void timerCallback() override
    {
        playStopBtn.setButtonText(module.running.load() ? "STOP" : "PLAY");
        for (auto& btn : stepButtons) btn->repaint();
    }

    KnobLookAndFeel knobLook;   // must outlive knobs
    StepSequencerModule& module;

    std::vector<std::unique_ptr<StepButton>> stepButtons;
    juce::TextButton playStopBtn, clearBtn;
    juce::Slider     bpmKnob, gateKnob;
    juce::Label      bpmLabel, gateLabel, rateLabel;
    juce::ComboBox   rateBox;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StepSeqEditor)
};

} // namespace

std::unique_ptr<juce::Component> StepSequencerModule::createEditor()
{
    return std::make_unique<StepSeqEditor>(*this);
}

// ---- Persistence -----------------------------------------------------------

void StepSequencerModule::saveState(juce::XmlElement& xml) const
{
    xml.setAttribute("bpm",    bpm.load());
    xml.setAttribute("subdiv", subdiv.load());
    xml.setAttribute("gate",   gate.load());

    juce::String pattern;
    for (auto& s : steps) pattern << s.load();
    xml.setAttribute("pattern", pattern);
}

void StepSequencerModule::loadState(const juce::XmlElement& xml)
{
    bpm   .store((float)xml.getDoubleAttribute("bpm",    bpm.load()));
    subdiv.store(       xml.getIntAttribute   ("subdiv", subdiv.load()));
    gate  .store((float)xml.getDoubleAttribute("gate",   gate.load()));

    const auto pattern = xml.getStringAttribute("pattern");
    for (int i = 0; i < numSteps && i < pattern.length(); ++i)
        steps[(size_t)i].store(pattern.substring(i, i + 1).getIntValue());
}

REGISTER_MODULE(StepSequencerModule);
