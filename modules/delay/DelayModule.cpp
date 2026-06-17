#include "DelayModule.h"
#include "ModuleRegistry.h"

// ---- DSP -------------------------------------------------------------------

DelayModule::DelayModule(SynthParameters&) {}

void DelayModule::prepareToPlay(double sr, int)
{
    sampleRate = sr;
    bufSize    = (int)(sr * kMaxDelaySec);
    bufL.assign((size_t)bufSize, 0.0f);
    bufR.assign((size_t)bufSize, 0.0f);
    writePtr  = 0;
    hcStateL  = hcStateR = 0.0f;
}

// Linear-interpolated read from circular buffer
float DelayModule::readBuf(const std::vector<float>& buf, float delaySamples) const noexcept
{
    const int d0 = (int)delaySamples;
    const float frac = delaySamples - (float)d0;

    const int i0 = (writePtr - d0 - 1 + bufSize) % bufSize;
    const int i1 = (i0 - 1 + bufSize)             % bufSize;
    return buf[(size_t)i0] + frac * (buf[(size_t)i1] - buf[(size_t)i0]);
}

void DelayModule::processBlock(juce::AudioBuffer<float>& buffer,
                               juce::MidiBuffer&,
                               int startSample, int numSamples)
{
    if (bufSize == 0) return;

    const float timePar = time    .load();
    const float fbPar   = feedback.load();
    const float hcPar   = highCut .load();
    const float mixPar  = mix     .load();
    const bool  ppPar   = pingPong.load();

    const float delaySamples = juce::jlimit(1.0f, (float)(bufSize - 1),
                                            timePar * 0.001f * (float)sampleRate);

    // One-pole high-cut coefficient for feedback path
    const float hcCoeff = 1.0f - std::exp(-juce::MathConstants<float>::twoPi
                                           * hcPar / (float)sampleRate);

    const int numCh = buffer.getNumChannels();
    float* dataL = buffer.getWritePointer(0, startSample);
    float* dataR = numCh >= 2 ? buffer.getWritePointer(1, startSample) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        const float dryL = dataL[i];
        const float dryR = dataR ? dataR[i] : dryL;

        // Read from delay lines
        float delL = readBuf(bufL, delaySamples);
        float delR = readBuf(bufR, delaySamples);

        // High-cut filter on feedback signal
        hcStateL += hcCoeff * (delL - hcStateL);
        hcStateR += hcCoeff * (delR - hcStateR);

        // Write into delay buffer — ping-pong crosses feedback channels
        bufL[(size_t)writePtr] = dryL + fbPar * (ppPar ? hcStateR : hcStateL);
        bufR[(size_t)writePtr] = dryR + fbPar * (ppPar ? hcStateL : hcStateR);

        writePtr = (writePtr + 1) % bufSize;

        // Output
        dataL[i] = dryL * (1.0f - mixPar) + delL * mixPar;
        if (dataR)
            dataR[i] = dryR * (1.0f - mixPar) + delR * mixPar;
    }
}

// ---- Editor ----------------------------------------------------------------

namespace
{

class EchoDisplay : public juce::Component
{
public:
    explicit EchoDisplay(DelayModule& m) : module(m) {}

    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().reduced(2);
        g.setColour(juce::Colour(0xff0a0e14));
        g.fillRoundedRectangle(b.toFloat(), 4.0f);

        const float fb  = module.feedback.load();
        const float t   = module.time    .load();   // ms
        const float maxT = 2000.0f;

        const float w = (float)b.getWidth()  - 4.0f;
        const float h = (float)b.getHeight() - 8.0f;
        const float ox = (float)b.getX() + 2.0f;
        const float oy = (float)b.getY() + 4.0f + h;

        // Draw each echo repeat as a vertical bar decaying in height
        float amp = 1.0f;
        for (int rep = 0; rep <= 12; ++rep)
        {
            const float xPos = ox + (t * (float)rep / maxT) * w;
            if (xPos > ox + w) break;

            const float barH = amp * h;

            // Gradient: bright at base
            const float alpha = amp * (rep == 0 ? 1.0f : 0.85f);
            g.setColour(rep == 0
                ? juce::Colour(0xff2255aa).withAlpha(alpha)
                : juce::Colour(0xff44aaff).withAlpha(alpha * 0.7f));

            const float bw = juce::jmax(2.0f, w * 0.015f);
            g.fillRoundedRectangle(xPos - bw * 0.5f, oy - barH, bw, barH, 1.5f);

            amp *= fb;
            if (amp < 0.01f) break;
        }

        // Time axis label
        g.setColour(juce::Colours::white.withAlpha(0.25f));
        g.setFont(juce::Font(9.0f));
        g.drawText(juce::String((int)t) + " ms",
                   b.removeFromBottom(12), juce::Justification::centredRight);
    }

    void setDirty() { repaint(); }

private:
    DelayModule& module;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EchoDisplay)
};

// ---------------------------------------------------------------------------

class DelayEditor : public juce::Component
{
public:
    explicit DelayEditor(DelayModule& m) : module(m), echoDisplay(m)
    {
        auto setupKnob = [this](juce::Slider& s, juce::Label& l,
                                const juce::String& name,
                                double lo, double hi, double val,
                                const juce::String& suffix = "")
        {
            s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
            s.setRange(lo, hi);
            s.setValue(val, juce::dontSendNotification);
            s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 14);
            s.setNumDecimalPlacesToDisplay(1);
            if (suffix.isNotEmpty()) s.setTextValueSuffix(suffix);
            addAndMakeVisible(s);

            l.setText(name, juce::dontSendNotification);
            l.setJustificationType(juce::Justification::centred);
            l.setFont(juce::Font(10.0f));
            addAndMakeVisible(l);
        };

        setupKnob(timeKnob, timeLabel, "Time",     1.0,  2000.0, module.time    .load(), " ms");
        setupKnob(fbKnob,   fbLabel,   "Feedback", 0.0,  0.95,   module.feedback.load());
        setupKnob(hcKnob,   hcLabel,   "High Cut", 500.0, 20000.0, module.highCut.load(), " Hz");
        setupKnob(mixKnob,  mixLabel,  "Mix",      0.0,  1.0,    module.mix     .load());

        timeKnob.onValueChange = [this]
        {
            module.time.store((float)timeKnob.getValue());
            echoDisplay.setDirty();
        };
        fbKnob.onValueChange = [this]
        {
            module.feedback.store((float)fbKnob.getValue());
            echoDisplay.setDirty();
        };
        hcKnob .onValueChange = [this] { module.highCut .store((float)hcKnob .getValue()); };
        mixKnob.onValueChange = [this] { module.mix     .store((float)mixKnob .getValue()); };

        pingPongBtn.setButtonText("Ping-Pong");
        pingPongBtn.setToggleState(module.pingPong.load(), juce::dontSendNotification);
        pingPongBtn.setClickingTogglesState(true);
        pingPongBtn.setColour(juce::TextButton::buttonColourId,   juce::Colour(0xff1a1a2a));
        pingPongBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff0055cc));
        pingPongBtn.setColour(juce::TextButton::textColourOffId,  juce::Colours::white.withAlpha(0.4f));
        pingPongBtn.setColour(juce::TextButton::textColourOnId,   juce::Colours::white);
        pingPongBtn.onStateChange = [this]
        {
            module.pingPong.store(pingPongBtn.getToggleState());
        };
        addAndMakeVisible(pingPongBtn);

        addAndMakeVisible(echoDisplay);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff0d1117));

        auto titleBar = getLocalBounds().removeFromTop(22);
        g.setColour(juce::Colour(0xff1a1a2a));
        g.fillRoundedRectangle(titleBar.toFloat(), 6.0f);
        g.setColour(juce::Colour(0xff88aaff));
        g.setFont(juce::Font(12.0f, juce::Font::bold));
        g.drawText("DELAY", titleBar, juce::Justification::centred);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(8);
        area.removeFromTop(22 + 4);

        auto knobRow = area.removeFromTop(80);
        const int kw = knobRow.getWidth() / 4;

        auto layoutKnob = [&](juce::Label& lbl, juce::Slider& knob)
        {
            auto col = knobRow.removeFromLeft(kw);
            lbl .setBounds(col.removeFromTop(14));
            knob.setBounds(col);
        };

        layoutKnob(timeLabel, timeKnob);
        layoutKnob(fbLabel,   fbKnob);
        layoutKnob(hcLabel,   hcKnob);
        layoutKnob(mixLabel,  mixKnob);

        area.removeFromTop(6);
        pingPongBtn.setBounds(area.removeFromTop(24).removeFromLeft(100));
        area.removeFromTop(4);
        echoDisplay.setBounds(area);
    }

private:
    DelayModule& module;

    juce::Slider    timeKnob, fbKnob, hcKnob, mixKnob;
    juce::Label     timeLabel, fbLabel, hcLabel, mixLabel;
    juce::TextButton pingPongBtn;
    EchoDisplay      echoDisplay;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DelayEditor)
};

} // namespace

std::unique_ptr<juce::Component> DelayModule::createEditor()
{
    return std::make_unique<DelayEditor>(*this);
}

// ---- Persistence -----------------------------------------------------------

void DelayModule::saveState(juce::XmlElement& xml) const
{
    xml.setAttribute("time",     time    .load());
    xml.setAttribute("feedback", feedback.load());
    xml.setAttribute("highCut",  highCut .load());
    xml.setAttribute("mix",      mix     .load());
    xml.setAttribute("pingPong", (int)pingPong.load());
}

void DelayModule::loadState(const juce::XmlElement& xml)
{
    time    .store((float)xml.getDoubleAttribute("time",     time    .load()));
    feedback.store((float)xml.getDoubleAttribute("feedback", feedback.load()));
    highCut .store((float)xml.getDoubleAttribute("highCut",  highCut .load()));
    mix     .store((float)xml.getDoubleAttribute("mix",      mix     .load()));
    pingPong.store(        xml.getIntAttribute   ("pingPong", 0) != 0);
}

REGISTER_MODULE(DelayModule);
