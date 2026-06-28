#include "ArpModule.h"
#include "ModuleRegistry.h"

// ---- DSP -------------------------------------------------------------------

ArpModule::ArpModule(SynthParameters&) {}

void ArpModule::prepareToPlay(double sr, int)
{
    sampleRate  = sr;
    stepPhase   = 0.0;
    noteIsOn    = false;
    activeNote  = -1;
    currentStep = 0;
    heldNotes.clear();
    sequence.clear();
}

void ArpModule::rebuildSequence()
{
    const int pat  = pattern.load();
    const int octs = octaves.load();

    // Expand held notes across octaves (set is already sorted ascending)
    std::vector<int> base;
    for (int oct = 0; oct < octs; ++oct)
        for (int n : heldNotes)
            if (n + oct * 12 <= 127)
                base.push_back(n + oct * 12);

    switch (pat)
    {
        case 0: // Up
            sequence = base;
            break;

        case 1: // Down
            sequence = base;
            std::reverse(sequence.begin(), sequence.end());
            break;

        case 2: // Up-Down — ascend then descend, no repeated endpoints
            sequence = base;
            if (base.size() > 1)
                for (int i = (int)base.size() - 2; i >= 1; --i)
                    sequence.push_back(base[(size_t)i]);
            break;

        case 3: // Random — re-shuffle on every rebuild
            sequence = base;
            for (int i = (int)sequence.size() - 1; i > 0; --i)
            {
                int j = std::rand() % (i + 1);
                std::swap(sequence[(size_t)i], sequence[(size_t)j]);
            }
            break;
    }

    if (!sequence.empty())
        currentStep = currentStep % (int)sequence.size();

    seqLength.store((int)sequence.size());
}

void ArpModule::processMidi(juce::MidiBuffer& midi,
                            int startSample, int numSamples)
{
    const double bpmVal        = bpm   .load();
    const int    subdivVal     = subdiv.load();
    const double gateVal       = gate  .load();

    const double samplesPerStep = (sampleRate * 60.0 / bpmVal) * (4.0 / subdivVal);
    const double noteOffPhase   = samplesPerStep * gateVal;

    // --- Collect incoming MIDI, update held notes -------------------------
    bool notesChanged = false;
    juce::MidiBuffer passThrough; // non-note events pass through unchanged

    for (auto it : midi)
    {
        auto m = it.getMessage();
        if (m.isNoteOn())
        {
            heldNotes.insert(m.getNoteNumber());
            notesChanged = true;
        }
        else if (m.isNoteOff())
        {
            heldNotes.erase(m.getNoteNumber());
            notesChanged = true;
        }
        else
        {
            passThrough.addEvent(m, it.samplePosition);
        }
    }

    heldCount.store((int)heldNotes.size());

    if (notesChanged || seqDirty)
    {
        rebuildSequence();
        seqDirty = false;
    }

    // --- Build output MIDI ------------------------------------------------
    juce::MidiBuffer outMidi;
    outMidi.addEvents(passThrough, 0, -1, 0);

    if (sequence.empty())
    {
        // Nothing held — cut any ringing note and reset phase
        if (noteIsOn && activeNote >= 0)
        {
            outMidi.addEvent(juce::MidiMessage::noteOff(1, activeNote), startSample);
            noteIsOn  = false;
            activeNote = -1;
            currentNote.store(-1);
        }
        stepPhase = 0.0;
        midi = outMidi;
        return;
    }

    // --- Advance arp step-by-step within the block -----------------------
    for (int i = 0; i < numSamples; ++i)
    {
        const int absPos = startSample + i;

        // Gate: note-off at the fraction of step
        if (noteIsOn && stepPhase >= noteOffPhase)
        {
            outMidi.addEvent(juce::MidiMessage::noteOff(1, activeNote), absPos);
            noteIsOn = false;
        }

        // Step boundary: advance and fire next note
        if (stepPhase >= samplesPerStep)
        {
            stepPhase -= samplesPerStep;

            // Force note-off in case gate == 1.0
            if (noteIsOn)
            {
                outMidi.addEvent(juce::MidiMessage::noteOff(1, activeNote), absPos);
                noteIsOn = false;
            }

            currentStep = (currentStep + 1) % (int)sequence.size();
            activeNote  = sequence[(size_t)currentStep];

            outMidi.addEvent(
                juce::MidiMessage::noteOn(1, activeNote, (juce::uint8)100), absPos);
            noteIsOn = true;

            stepIndex  .store(currentStep);
            currentNote.store(activeNote);
        }

        stepPhase += 1.0;
    }

    midi = outMidi;
}

// ---- Editor ----------------------------------------------------------------

namespace
{

// Animated row of step dots — lights up the current step
class StepDisplay : public juce::Component, private juce::Timer
{
public:
    explicit StepDisplay(ArpModule& m) : module(m) { startTimerHz(30); }

    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().reduced(2);
        g.setColour(juce::Colour(0xff080c10));
        g.fillRoundedRectangle(b.toFloat(), 4.0f);

        const int len     = module.seqLength .load();
        const int current = module.stepIndex .load();
        const int held    = module.heldCount .load();
        const int note    = module.currentNote.load();

        if (len == 0 || held == 0)
        {
            g.setColour(juce::Colours::white.withAlpha(0.2f));
            g.setFont(juce::Font(10.0f));
            g.drawText("play a chord...", b, juce::Justification::centred);
            return;
        }

        // Step dots (up to 16 visible)
        const int visible = juce::jmin(len, 16);
        const float dotSize = juce::jmin(12.0f, (float)(b.getWidth() - 8) / visible - 3.0f);
        const float spacing = (float)(b.getWidth() - 8) / visible;
        const float dotY    = (float)b.getCentreY() - dotSize * 0.5f - 6.0f;

        for (int i = 0; i < visible; ++i)
        {
            const float dx = (float)b.getX() + 4.0f + i * spacing + (spacing - dotSize) * 0.5f;
            const bool  active = (i == current % visible);

            g.setColour(active ? juce::Colour(0xff44ddff)
                                : juce::Colour(0xff1a3040));
            g.fillEllipse(dx, dotY, dotSize, dotSize);

            if (active)
            {
                g.setColour(juce::Colour(0xff44ddff).withAlpha(0.3f));
                g.fillEllipse(dx - 2, dotY - 2, dotSize + 4, dotSize + 4);
            }
        }

        // Current note name
        if (note >= 0)
        {
            const juce::String noteName =
                juce::MidiMessage::getMidiNoteName(note, true, true, 4);
            g.setColour(juce::Colours::white.withAlpha(0.75f));
            g.setFont(juce::Font(10.0f));
            g.drawText(noteName, b.withTop(b.getCentreY() + 4),
                       juce::Justification::centred);
        }
    }

private:
    ArpModule& module;
    void timerCallback() override { repaint(); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StepDisplay)
};

class ArpEditor : public juce::Component
{
public:
    explicit ArpEditor(ArpModule& m) : module(m), display(m)
    {
        // BPM knob
        bpmKnob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        bpmKnob.setRange(40.0, 240.0);
        bpmKnob.setValue(module.bpm.load(), juce::dontSendNotification);
        bpmKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 52, 14);
        bpmKnob.setNumDecimalPlacesToDisplay(0);
        bpmKnob.onValueChange = [this] { module.bpm.store((float)bpmKnob.getValue()); };
        addAndMakeVisible(bpmKnob);

        bpmLabel.setText("BPM", juce::dontSendNotification);
        bpmLabel.setJustificationType(juce::Justification::centred);
        bpmLabel.setFont(juce::Font(10.0f));
        addAndMakeVisible(bpmLabel);

        // Gate knob
        gateKnob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        gateKnob.setRange(0.05, 1.0);
        gateKnob.setValue(module.gate.load(), juce::dontSendNotification);
        gateKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 52, 14);
        gateKnob.setNumDecimalPlacesToDisplay(2);
        gateKnob.onValueChange = [this] { module.gate.store((float)gateKnob.getValue()); };
        addAndMakeVisible(gateKnob);

        gateLabel.setText("Gate", juce::dontSendNotification);
        gateLabel.setJustificationType(juce::Justification::centred);
        gateLabel.setFont(juce::Font(10.0f));
        addAndMakeVisible(gateLabel);

        // Subdivision
        subdivBox.addItem("1/4",  1);
        subdivBox.addItem("1/8",  2);
        subdivBox.addItem("1/16", 3);
        subdivBox.addItem("1/32", 4);
        auto subdivToId = [](int s) {
            if (s == 4)  return 1;
            if (s == 8)  return 2;
            if (s == 16) return 3;
            return 4;
        };
        auto idToSubdiv = [](int id) {
            if (id == 1) return 4;
            if (id == 2) return 8;
            if (id == 3) return 16;
            return 32;
        };
        subdivBox.setSelectedId(subdivToId(module.subdiv.load()), juce::dontSendNotification);
        subdivBox.onChange = [this, idToSubdiv] {
            module.subdiv.store(idToSubdiv(subdivBox.getSelectedId()));
        };
        addAndMakeVisible(subdivBox);

        subdivLabel.setText("Rate", juce::dontSendNotification);
        subdivLabel.setJustificationType(juce::Justification::centred);
        subdivLabel.setFont(juce::Font(10.0f));
        addAndMakeVisible(subdivLabel);

        // Pattern
        patternBox.addItem("Up",      1);
        patternBox.addItem("Down",    2);
        patternBox.addItem("Up-Down", 3);
        patternBox.addItem("Random",  4);
        patternBox.setSelectedId(module.pattern.load() + 1, juce::dontSendNotification);
        patternBox.onChange = [this] {
            module.pattern.store(patternBox.getSelectedId() - 1);
            module.seqDirty = true;
        };
        addAndMakeVisible(patternBox);

        patternLabel.setText("Pattern", juce::dontSendNotification);
        patternLabel.setJustificationType(juce::Justification::centred);
        patternLabel.setFont(juce::Font(10.0f));
        addAndMakeVisible(patternLabel);

        // Octaves
        octaveBox.addItem("1 Oct", 1);
        octaveBox.addItem("2 Oct", 2);
        octaveBox.addItem("3 Oct", 3);
        octaveBox.addItem("4 Oct", 4);
        octaveBox.setSelectedId(module.octaves.load(), juce::dontSendNotification);
        octaveBox.onChange = [this] {
            module.octaves.store(octaveBox.getSelectedId());
            module.seqDirty = true;
        };
        addAndMakeVisible(octaveBox);

        octaveLabel.setText("Octaves", juce::dontSendNotification);
        octaveLabel.setJustificationType(juce::Justification::centred);
        octaveLabel.setFont(juce::Font(10.0f));
        addAndMakeVisible(octaveLabel);

        addAndMakeVisible(display);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff0a0f14));

        auto titleBar = getLocalBounds().removeFromTop(22);
        g.setColour(juce::Colour(0xff1a2a3a));
        g.fillRoundedRectangle(titleBar.toFloat(), 6.0f);
        g.setColour(juce::Colour(0xff44ddff));
        g.setFont(juce::Font(12.0f, juce::Font::bold));
        g.drawText("ARPEGGIATOR", titleBar, juce::Justification::centred);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(8);
        area.removeFromTop(22 + 6);

        display.setBounds(area.removeFromBottom(44));
        area.removeFromBottom(6);

        // Two combo rows
        auto comboRow = area.removeFromTop(40);
        int  cw       = comboRow.getWidth() / 2;

        auto layoutCombo = [](juce::Rectangle<int>& row, int w,
                               juce::Label& lbl, juce::ComboBox& box)
        {
            auto col = row.removeFromLeft(w).reduced(4, 0);
            lbl.setBounds(col.removeFromTop(14));
            box.setBounds(col.removeFromTop(22));
        };

        layoutCombo(comboRow, cw, patternLabel, patternBox);
        layoutCombo(comboRow, cw, subdivLabel,  subdivBox);

        area.removeFromTop(4);

        auto comboRow2 = area.removeFromTop(40);
        layoutCombo(comboRow2, cw, octaveLabel, octaveBox);

        area.removeFromTop(4);

        // Knobs
        auto knobRow = area;
        int  kw      = knobRow.getWidth() / 2;

        auto layoutKnob = [](juce::Rectangle<int>& row, int w,
                              juce::Label& lbl, juce::Slider& knob)
        {
            auto col = row.removeFromLeft(w);
            lbl .setBounds(col.removeFromTop(14));
            knob.setBounds(col);
        };

        layoutKnob(knobRow, kw, bpmLabel,  bpmKnob);
        layoutKnob(knobRow, kw, gateLabel, gateKnob);
    }

private:
    ArpModule&  module;
    StepDisplay display;

    juce::Slider   bpmKnob, gateKnob;
    juce::Label    bpmLabel, gateLabel;
    juce::ComboBox subdivBox, patternBox, octaveBox;
    juce::Label    subdivLabel, patternLabel, octaveLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ArpEditor)
};

} // namespace

std::unique_ptr<juce::Component> ArpModule::createEditor()
{
    return std::make_unique<ArpEditor>(*this);
}

// ---- Persistence -----------------------------------------------------------

void ArpModule::saveState(juce::XmlElement& xml) const
{
    xml.setAttribute("bpm",     bpm    .load());
    xml.setAttribute("subdiv",  subdiv .load());
    xml.setAttribute("pattern", pattern.load());
    xml.setAttribute("octaves", octaves.load());
    xml.setAttribute("gate",    gate   .load());
}

void ArpModule::loadState(const juce::XmlElement& xml)
{
    bpm    .store((float)xml.getDoubleAttribute("bpm",     bpm    .load()));
    subdiv .store(       xml.getIntAttribute   ("subdiv",  subdiv .load()));
    pattern.store(       xml.getIntAttribute   ("pattern", pattern.load()));
    octaves.store(       xml.getIntAttribute   ("octaves", octaves.load()));
    gate   .store((float)xml.getDoubleAttribute("gate",    gate   .load()));
    seqDirty = true;
}

REGISTER_MODULE(ArpModule);
