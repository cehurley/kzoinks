#include "AutoWahModule.h"
#include "ModuleRegistry.h"

// ---- DSP -------------------------------------------------------------------

AutoWahModule::AutoWahModule(SynthParameters&) {}

void AutoWahModule::prepareToPlay(double sr, int)
{
    sampleRate = sr;
    svf[0] = svf[1] = {};
    env[0] = env[1] = 0.0f;
}

void AutoWahModule::processBlock(juce::AudioBuffer<float>& buffer,
                                 juce::MidiBuffer&,
                                 int startSample, int numSamples)
{
    const float sens  = sensitivity.load();
    const float attMs = attack     .load();
    const float relMs = release    .load();
    const float base  = baseFreq   .load();
    const float rng   = range      .load();
    const float q     = resonance  .load();
    const float mx    = mix        .load();

    const float sr    = (float)sampleRate;
    const float attC  = 1.0f - std::exp(-1.0f / (sr * attMs  * 0.001f));
    const float relC  = 1.0f - std::exp(-1.0f / (sr * relMs  * 0.001f));

    const int numCh = juce::jmin(buffer.getNumChannels(), 2);

    float peakEnv = 0.0f;

    for (int ch = 0; ch < numCh; ++ch)
    {
        float* data = buffer.getWritePointer(ch, startSample);

        for (int i = 0; i < numSamples; ++i)
        {
            const float dry = data[i];
            const float abs = std::abs(dry);

            // Envelope follower
            const float coeff = (abs > env[ch]) ? attC : relC;
            env[ch] += coeff * (abs - env[ch]);

            // Map envelope → filter cutoff
            const float cutoff = juce::jlimit(20.0f, 18000.0f,
                                              base + env[ch] * sens * rng);

            // Chamberlin SVF — bandpass output for the classic wah sound
            const float f  = 2.0f * std::sin(juce::MathConstants<float>::pi * cutoff / sr);
            const float fC = juce::jlimit(0.0f, 0.98f, f); // clamp for stability

            // Dynamic Q limit (same stability criterion as SynthVoice)
            const float qMax = (4.0f - fC * fC) / (2.0f * fC) * 0.98f;
            const float qC   = juce::jlimit(0.1f, qMax, q);

            svf[ch].low  += fC * svf[ch].band;
            const float high = dry - svf[ch].low - svf[ch].band / qC;
            svf[ch].band += fC * high;

            // Blend bandpass with dry signal
            data[i] = dry * (1.0f - mx) + svf[ch].band * mx;

            if (ch == 0) peakEnv = juce::jmax(peakEnv, env[ch]);
        }
    }

    envLevel.store(peakEnv);
}

// ---- Editor ----------------------------------------------------------------

namespace
{

class WahDisplay : public juce::Component, private juce::Timer
{
public:
    explicit WahDisplay(AutoWahModule& m) : module(m) { startTimerHz(30); }

    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().reduced(1);
        g.setColour(juce::Colour(0xff080c10));
        g.fillRoundedRectangle(b.toFloat(), 4.0f);

        const float level = juce::jlimit(0.0f, 1.0f, module.envLevel.load()
                                         * module.sensitivity.load() * 2.0f);

        // Background frequency sweep bar
        const int barX = b.getX() + 4;
        const int barW = b.getWidth() - 8;
        const int barY = b.getCentreY() - 6;
        const int barH = 12;

        g.setColour(juce::Colour(0xff1a2a1a));
        g.fillRoundedRectangle((float)barX, (float)barY,
                               (float)barW, (float)barH, 3.0f);

        // Active fill proportional to envelope
        g.setColour(juce::Colour(0xff33cc55).withAlpha(0.8f));
        g.fillRoundedRectangle((float)barX, (float)barY,
                               (float)barW * level, (float)barH, 3.0f);

        // Cursor line
        const int cx = barX + (int)((float)barW * level);
        g.setColour(juce::Colours::white.withAlpha(0.9f));
        g.fillRoundedRectangle((float)(cx - 2), (float)(barY - 4),
                               4.0f, (float)(barH + 8), 2.0f);

        // Frequency label
        const float base  = module.baseFreq.load();
        const float rng   = module.range   .load();
        const float sens  = module.sensitivity.load();
        const float freq  = base + module.envLevel.load() * sens * rng;
        const juce::String freqText = juce::String((int)juce::jlimit(20.0f, 18000.0f, freq)) + " Hz";

        g.setColour(juce::Colours::white.withAlpha(0.5f));
        g.setFont(juce::Font(9.0f));
        g.drawText(freqText, b, juce::Justification::centredBottom);
    }

private:
    AutoWahModule& module;
    void timerCallback() override { repaint(); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WahDisplay)
};

class AutoWahEditor : public juce::Component
{
public:
    explicit AutoWahEditor(AutoWahModule& m) : module(m), display(m)
    {
        auto setup = [this](juce::Slider& s, juce::Label& l,
                            const juce::String& name,
                            double lo, double hi, double val,
                            bool skew = false)
        {
            s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
            s.setRange(lo, hi);
            s.setValue(val, juce::dontSendNotification);
            s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 52, 14);
            s.setNumDecimalPlacesToDisplay(1);
            if (skew) s.setSkewFactorFromMidPoint((lo + hi) * 0.25);
            addAndMakeVisible(s);

            l.setText(name, juce::dontSendNotification);
            l.setJustificationType(juce::Justification::centred);
            l.setFont(juce::Font(10.0f));
            addAndMakeVisible(l);
        };

        setup(sensKnob,    sensLabel,    "Sense",   0.0,    1.0,    module.sensitivity.load());
        setup(attKnob,     attLabel,     "Attack",  1.0,    200.0,  module.attack     .load(), true);
        setup(relKnob,     relLabel,     "Release", 10.0,   500.0,  module.release    .load(), true);
        setup(resoKnob,    resoLabel,    "Reso",    0.5,    8.0,    module.resonance  .load());
        setup(baseKnob,    baseLabel,    "Base",    50.0,   800.0,  module.baseFreq   .load(), true);
        setup(rangeKnob,   rangeLabel,   "Range",   500.0,  8000.0, module.range      .load(), true);
        setup(mixKnob,     mixLabel,     "Mix",     0.0,    1.0,    module.mix        .load());

        sensKnob .onValueChange = [this] { module.sensitivity.store((float)sensKnob .getValue()); };
        attKnob  .onValueChange = [this] { module.attack     .store((float)attKnob  .getValue()); };
        relKnob  .onValueChange = [this] { module.release    .store((float)relKnob  .getValue()); };
        resoKnob .onValueChange = [this] { module.resonance  .store((float)resoKnob .getValue()); };
        baseKnob .onValueChange = [this] { module.baseFreq   .store((float)baseKnob .getValue()); };
        rangeKnob.onValueChange = [this] { module.range      .store((float)rangeKnob.getValue()); };
        mixKnob  .onValueChange = [this] { module.mix        .store((float)mixKnob  .getValue()); };

        addAndMakeVisible(display);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff0d1a0d));

        auto titleBar = getLocalBounds().removeFromTop(22);
        g.setColour(juce::Colour(0xff1a3a1a));
        g.fillRoundedRectangle(titleBar.toFloat(), 6.0f);
        g.setColour(juce::Colour(0xff55ee77));
        g.setFont(juce::Font(12.0f, juce::Font::bold));
        g.drawText("AUTO-WAH", titleBar, juce::Justification::centred);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(8);
        area.removeFromTop(22 + 6);

        display.setBounds(area.removeFromBottom(36));
        area.removeFromBottom(6);

        // Two rows of knobs: 4 on top, 3 on bottom centred
        auto row1 = area.removeFromTop(80);
        auto row2 = area;

        const int kw = row1.getWidth() / 4;
        auto layoutKnob = [](juce::Rectangle<int>& row, int w,
                              juce::Label& lbl, juce::Slider& knob)
        {
            auto col = row.removeFromLeft(w);
            lbl .setBounds(col.removeFromTop(14));
            knob.setBounds(col);
        };

        layoutKnob(row1, kw, sensLabel,  sensKnob);
        layoutKnob(row1, kw, attLabel,   attKnob);
        layoutKnob(row1, kw, relLabel,   relKnob);
        layoutKnob(row1, kw, resoLabel,  resoKnob);

        // Centre 3 knobs in row2
        const int kw2 = row2.getWidth() / 4;
        row2.removeFromLeft(kw2 / 2);
        layoutKnob(row2, kw2, baseLabel,  baseKnob);
        layoutKnob(row2, kw2, rangeLabel, rangeKnob);
        layoutKnob(row2, kw2, mixLabel,   mixKnob);
    }

private:
    AutoWahModule& module;
    WahDisplay     display;

    juce::Slider sensKnob, attKnob, relKnob, resoKnob, baseKnob, rangeKnob, mixKnob;
    juce::Label  sensLabel, attLabel, relLabel, resoLabel, baseLabel, rangeLabel, mixLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AutoWahEditor)
};

} // namespace

std::unique_ptr<juce::Component> AutoWahModule::createEditor()
{
    return std::make_unique<AutoWahEditor>(*this);
}

// ---- Persistence -----------------------------------------------------------

void AutoWahModule::saveState(juce::XmlElement& xml) const
{
    xml.setAttribute("sensitivity", sensitivity.load());
    xml.setAttribute("attack",      attack     .load());
    xml.setAttribute("release",     release    .load());
    xml.setAttribute("baseFreq",    baseFreq   .load());
    xml.setAttribute("range",       range      .load());
    xml.setAttribute("resonance",   resonance  .load());
    xml.setAttribute("mix",         mix        .load());
}

void AutoWahModule::loadState(const juce::XmlElement& xml)
{
    sensitivity.store((float)xml.getDoubleAttribute("sensitivity", sensitivity.load()));
    attack     .store((float)xml.getDoubleAttribute("attack",      attack     .load()));
    release    .store((float)xml.getDoubleAttribute("release",     release    .load()));
    baseFreq   .store((float)xml.getDoubleAttribute("baseFreq",    baseFreq   .load()));
    range      .store((float)xml.getDoubleAttribute("range",       range      .load()));
    resonance  .store((float)xml.getDoubleAttribute("resonance",   resonance  .load()));
    mix        .store((float)xml.getDoubleAttribute("mix",         mix        .load()));
}

REGISTER_MODULE(AutoWahModule);
