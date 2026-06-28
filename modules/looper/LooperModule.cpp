#include "LooperModule.h"
#include "ModuleRegistry.h"

// ---- DSP -------------------------------------------------------------------

LooperModule::LooperModule(SynthParameters&) {}

void LooperModule::prepareToPlay(double sr, int)
{
    sampleRate = sr;
    maxSamples = (int)(sr * kMaxLoopSec);
    bufL.assign((size_t)maxSamples, 0.0f);
    bufR.assign((size_t)maxSamples, 0.0f);
    undoL.assign((size_t)maxSamples, 0.0f);
    undoR.assign((size_t)maxSamples, 0.0f);
    recLen = 0;
    playhead = 0;
    undoLen = 0;
    playProgress.store(0.0f);
    loopLenSec.store(0.0f);
    state.store((int)State::Empty);
}

void LooperModule::finishRecording()
{
    if (recLen <= 0)
    {
        state.store((int)State::Empty);
        return;
    }
    playhead = 0;
    loopLenSec.store((float)recLen / (float)sampleRate);
    state.store((int)State::Playing);
}

void LooperModule::handleRecordTrigger()
{
    const State st = (State)state.load();
    if (st == State::Empty)
    {
        recLen = 0;
        playhead = 0;
        undoLen = 0;
        state.store((int)State::Recording);
    }
    else if (st == State::Recording)
    {
        finishRecording();
    }
}

void LooperModule::handlePlayStopTrigger()
{
    if (recLen <= 0) return;
    const State st = (State)state.load();
    if (st == State::Playing || st == State::Overdubbing)
        state.store((int)State::Stopped);
    else if (st == State::Stopped)
    {
        playhead = 0;
        state.store((int)State::Playing);
    }
}

void LooperModule::handleOverdubTrigger()
{
    const State st = (State)state.load();
    if (st == State::Playing && recLen > 0)
    {
        std::copy(bufL.begin(), bufL.begin() + recLen, undoL.begin());
        std::copy(bufR.begin(), bufR.begin() + recLen, undoR.begin());
        undoLen = recLen;
        state.store((int)State::Overdubbing);
    }
    else if (st == State::Overdubbing)
    {
        state.store((int)State::Playing);
    }
}

void LooperModule::handleUndoTrigger()
{
    if (undoLen > 0 && undoLen == recLen)
    {
        std::copy(undoL.begin(), undoL.begin() + undoLen, bufL.begin());
        std::copy(undoR.begin(), undoR.begin() + undoLen, bufR.begin());
        undoLen = 0;
    }
}

void LooperModule::handleClearTrigger()
{
    recLen = 0;
    playhead = 0;
    undoLen = 0;
    playProgress.store(0.0f);
    loopLenSec.store(0.0f);
    state.store((int)State::Empty);
}

void LooperModule::processBlock(juce::AudioBuffer<float>& buffer,
                                juce::MidiBuffer&,
                                int startSample, int numSamples)
{
    if (maxSamples == 0) return;

    if (trigRecord.exchange(false))   handleRecordTrigger();
    if (trigPlayStop.exchange(false)) handlePlayStopTrigger();
    if (trigOverdub.exchange(false))  handleOverdubTrigger();
    if (trigUndo.exchange(false))     handleUndoTrigger();
    if (trigClear.exchange(false))    handleClearTrigger();

    State st = (State)state.load();
    const float lvl = level.load();
    const float dec = juce::jlimit(0.5f, 1.0f, decay.load());

    const int numCh = buffer.getNumChannels();
    float* dataL = buffer.getWritePointer(0, startSample);
    float* dataR = numCh >= 2 ? buffer.getWritePointer(1, startSample) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        const float dryL = dataL[i];
        const float dryR = dataR ? dataR[i] : dryL;
        float outL = dryL;
        float outR = dryR;

        switch (st)
        {
            case State::Recording:
                if (recLen < maxSamples)
                {
                    bufL[(size_t)recLen] = dryL;
                    bufR[(size_t)recLen] = dryR;
                    ++recLen;
                }
                else
                {
                    finishRecording();
                    st = (State)state.load();
                }
                break;

            case State::Playing:
            case State::Overdubbing:
                if (recLen > 0)
                {
                    const float loopL = bufL[(size_t)playhead];
                    const float loopR = bufR[(size_t)playhead];

                    if (st == State::Overdubbing)
                    {
                        bufL[(size_t)playhead] = loopL * dec + dryL;
                        bufR[(size_t)playhead] = loopR * dec + dryR;
                    }

                    outL = dryL + lvl * loopL;
                    outR = dryR + lvl * loopR;

                    playhead = (playhead + 1) % recLen;
                    playProgress.store((float)playhead / (float)recLen);
                }
                break;

            case State::Stopped:
            case State::Empty:
            default:
                break;
        }

        dataL[i] = outL;
        if (dataR) dataR[i] = outR;
    }
}

// ---- Editor ----------------------------------------------------------------

namespace
{

juce::String stateLabel(LooperModule::State st)
{
    switch (st)
    {
        case LooperModule::State::Empty:       return "EMPTY";
        case LooperModule::State::Recording:   return "RECORDING";
        case LooperModule::State::Playing:     return "PLAYING";
        case LooperModule::State::Stopped:     return "STOPPED";
        case LooperModule::State::Overdubbing: return "OVERDUB";
    }
    return {};
}

juce::Colour stateColour(LooperModule::State st)
{
    switch (st)
    {
        case LooperModule::State::Empty:       return juce::Colours::white.withAlpha(0.25f);
        case LooperModule::State::Recording:   return juce::Colour(0xffff4444);
        case LooperModule::State::Playing:     return juce::Colour(0xff44dd66);
        case LooperModule::State::Stopped:     return juce::Colour(0xffffaa44);
        case LooperModule::State::Overdubbing: return juce::Colour(0xffffdd44);
    }
    return juce::Colours::white;
}

class LoopDisplay : public juce::Component, private juce::Timer
{
public:
    explicit LoopDisplay(LooperModule& m) : module(m) { startTimerHz(30); }

    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().reduced(2);
        g.setColour(juce::Colour(0xff0a0e14));
        g.fillRoundedRectangle(b.toFloat(), 4.0f);

        const auto st = (LooperModule::State)module.state.load();
        const auto col = stateColour(st);

        auto bar = b.reduced(6);
        auto status = bar.removeFromTop(18);
        g.setColour(col);
        g.setFont(juce::Font(11.0f, juce::Font::bold));
        g.drawText(stateLabel(st), status, juce::Justification::centredLeft);

        const float lenSec = module.loopLenSec.load();
        if (lenSec > 0.0f)
        {
            g.setColour(juce::Colours::white.withAlpha(0.4f));
            g.setFont(juce::Font(10.0f));
            g.drawText(juce::String(lenSec, 2) + " s", status, juce::Justification::centredRight);
        }

        bar.removeFromTop(4);
        auto track = bar.removeFromTop(14).toFloat();
        g.setColour(juce::Colour(0xff14181f));
        g.fillRoundedRectangle(track, 3.0f);

        const bool active = st == LooperModule::State::Playing || st == LooperModule::State::Overdubbing;
        if (active)
        {
            const float prog = module.playProgress.load();
            g.setColour(col.withAlpha(0.85f));
            g.fillRoundedRectangle(track.getX(), track.getY(),
                                    juce::jmax(3.0f, track.getWidth() * prog),
                                    track.getHeight(), 3.0f);
        }
    }

private:
    void timerCallback() override { repaint(); }

    LooperModule& module;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LoopDisplay)
};

// ---------------------------------------------------------------------------

class LooperEditor : public juce::Component, private juce::Timer
{
public:
    explicit LooperEditor(LooperModule& m) : module(m), display(m)
    {
        auto setupBtn = [this](juce::TextButton& btn, const juce::String& text)
        {
            btn.setButtonText(text);
            btn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1a1a2a));
            btn.setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.85f));
            addAndMakeVisible(btn);
        };

        setupBtn(recordBtn,   "REC");
        setupBtn(playStopBtn, "PLAY");
        setupBtn(overdubBtn,  "OVERDUB");
        setupBtn(undoBtn,     "UNDO");
        setupBtn(clearBtn,    "CLEAR");

        recordBtn.onClick   = [this] { module.trigRecord.store(true); };
        playStopBtn.onClick = [this] { module.trigPlayStop.store(true); };
        overdubBtn.onClick  = [this] { module.trigOverdub.store(true); };
        undoBtn.onClick     = [this] { module.trigUndo.store(true); };
        clearBtn.onClick    = [this] { module.trigClear.store(true); };

        auto setupKnob = [this](juce::Slider& s, juce::Label& l,
                                const juce::String& name,
                                double lo, double hi, double val)
        {
            s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
            s.setRange(lo, hi);
            s.setValue(val, juce::dontSendNotification);
            s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 14);
            s.setNumDecimalPlacesToDisplay(2);
            addAndMakeVisible(s);

            l.setText(name, juce::dontSendNotification);
            l.setJustificationType(juce::Justification::centred);
            l.setFont(juce::Font(10.0f));
            addAndMakeVisible(l);
        };

        setupKnob(levelKnob, levelLabel, "Level", 0.0, 1.0, module.level.load());
        setupKnob(decayKnob, decayLabel, "Decay", 0.5, 1.0, module.decay.load());

        levelKnob.onValueChange = [this] { module.level.store((float)levelKnob.getValue()); };
        decayKnob.onValueChange = [this] { module.decay.store((float)decayKnob.getValue()); };

        addAndMakeVisible(display);
        startTimerHz(15);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff0d1117));

        auto titleBar = getLocalBounds().removeFromTop(22);
        g.setColour(juce::Colour(0xff1a1a2a));
        g.fillRoundedRectangle(titleBar.toFloat(), 6.0f);
        g.setColour(juce::Colour(0xffffaa66));
        g.setFont(juce::Font(12.0f, juce::Font::bold));
        g.drawText("LOOPER", titleBar, juce::Justification::centred);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(8);
        area.removeFromTop(22 + 4);

        display.setBounds(area.removeFromTop(56));
        area.removeFromTop(6);

        auto btnRow = area.removeFromTop(28);
        const int bw = btnRow.getWidth() / 5;
        recordBtn.setBounds(btnRow.removeFromLeft(bw).reduced(2));
        playStopBtn.setBounds(btnRow.removeFromLeft(bw).reduced(2));
        overdubBtn.setBounds(btnRow.removeFromLeft(bw).reduced(2));
        undoBtn.setBounds(btnRow.removeFromLeft(bw).reduced(2));
        clearBtn.setBounds(btnRow.removeFromLeft(bw).reduced(2));

        area.removeFromTop(8);
        auto knobRow = area.removeFromTop(80);
        const int kw = knobRow.getWidth() / 2;

        auto layoutKnob = [&](juce::Label& lbl, juce::Slider& knob)
        {
            auto col = knobRow.removeFromLeft(kw);
            lbl.setBounds(col.removeFromTop(14));
            knob.setBounds(col);
        };

        layoutKnob(levelLabel, levelKnob);
        layoutKnob(decayLabel, decayKnob);
    }

private:
    void timerCallback() override
    {
        const auto st = (LooperModule::State)module.state.load();

        recordBtn.setEnabled(st == LooperModule::State::Empty || st == LooperModule::State::Recording);
        recordBtn.setButtonText(st == LooperModule::State::Recording ? "STOP" : "REC");

        playStopBtn.setEnabled(module.loopLenSec.load() > 0.0f
                                && st != LooperModule::State::Recording
                                && st != LooperModule::State::Overdubbing);
        playStopBtn.setButtonText(st == LooperModule::State::Stopped ? "PLAY" : "STOP");

        overdubBtn.setEnabled(st == LooperModule::State::Playing || st == LooperModule::State::Overdubbing);
        overdubBtn.setColour(juce::TextButton::buttonColourId,
                              st == LooperModule::State::Overdubbing ? juce::Colour(0xff665500)
                                                                      : juce::Colour(0xff1a1a2a));

        undoBtn.setEnabled(st != LooperModule::State::Recording);
        clearBtn.setEnabled(st != LooperModule::State::Recording);
    }

    LooperModule& module;
    LoopDisplay   display;

    juce::TextButton recordBtn, playStopBtn, overdubBtn, undoBtn, clearBtn;
    juce::Slider      levelKnob, decayKnob;
    juce::Label       levelLabel, decayLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LooperEditor)
};

} // namespace

std::unique_ptr<juce::Component> LooperModule::createEditor()
{
    return std::make_unique<LooperEditor>(*this);
}

// ---- Persistence -----------------------------------------------------------

void LooperModule::saveState(juce::XmlElement& xml) const
{
    xml.setAttribute("level", level.load());
    xml.setAttribute("decay", decay.load());
}

void LooperModule::loadState(const juce::XmlElement& xml)
{
    level.store((float)xml.getDoubleAttribute("level", level.load()));
    decay.store((float)xml.getDoubleAttribute("decay", decay.load()));
}

REGISTER_MODULE(LooperModule);
