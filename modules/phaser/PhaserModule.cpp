#include "PhaserModule.h"
#include "ModuleRegistry.h"
#include <PhaserData.h>

// ---- DSP -------------------------------------------------------------------

PhaserModule::PhaserModule(SynthParameters&) {}

float PhaserModule::apf(APFState& s, float x, float a) noexcept
{
    // First-order all-pass: H(z) = (z^-1 - a) / (1 - a*z^-1)
    float y = a * x + s.x1 - a * s.y1;
    s.x1 = x;
    s.y1 = y;
    return y;
}

void PhaserModule::prepareToPlay(double sr, int)
{
    sampleRate       = sr;
    lfoPhaseInternal = 0.0;

    for (auto& ch : apfState)
        for (auto& s : ch)
            s = {};

    feedbackSample[0] = feedbackSample[1] = 0.0f;
}

void PhaserModule::processBlock(juce::AudioBuffer<float>& buffer,
                                juce::MidiBuffer&,
                                int startSample, int numSamples)
{
    const float ratePar = rate    .load();
    const float depPar  = depth   .load();
    const float cenPar  = center  .load();
    const float fbPar   = feedback.load();
    const float mixPar  = mix     .load();

    // Clamp stages to even values 2/4/6/8
    const int stageCount = juce::jlimit(2, kMaxStages, (stages.load() / 2) * 2);

    const double lfoInc = ratePar / sampleRate;
    const int numCh = juce::jmin(buffer.getNumChannels(), 2);

    for (int i = 0; i < numSamples; ++i)
    {
        lfoPhaseInternal += lfoInc;
        if (lfoPhaseInternal >= 1.0) lfoPhaseInternal -= 1.0;

        const float lfo = std::sin(juce::MathConstants<float>::twoPi
                                   * (float)lfoPhaseInternal);

        // Exponential frequency modulation: centre ± depth octaves
        const float fc = juce::jlimit(
            20.0f,
            (float)(sampleRate * 0.45),
            cenPar * std::exp(lfo * depPar * 1.5f));

        const float t = std::tan(juce::MathConstants<float>::pi
                                 * fc / (float)sampleRate);
        const float a = (t - 1.0f) / (t + 1.0f);

        for (int ch = 0; ch < numCh; ++ch)
        {
            float* data = buffer.getWritePointer(ch, startSample);
            const float dry = data[i];

            float x = dry + fbPar * feedbackSample[ch];
            for (int st = 0; st < stageCount; ++st)
                x = apf(apfState[ch][st], x, a);

            feedbackSample[ch] = x;
            data[i] = dry * (1.0f - mixPar) + x * mixPar;
        }
    }

    lfoPhase.store((float)lfoPhaseInternal);
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
            PhaserAssets::whknob_png, PhaserAssets::whknob_pngSize);
    }

    void drawRotarySlider(juce::Graphics& g,
                          int x, int y, int width, int height,
                          float sliderPos,
                          float /*startAngle*/, float /*endAngle*/,
                          juce::Slider&) override
    {
        if (!knobImage.isValid()) return;

        const float angle = juce::MathConstants<float>::pi * 1.25f
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

class LFODisplay : public juce::Component, private juce::Timer
{
public:
    explicit LFODisplay(PhaserModule& m) : module(m) { startTimerHz(30); }

    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().reduced(2);
        g.setColour(juce::Colour(0xff0a0f16));
        g.fillRoundedRectangle(b.toFloat(), 4.0f);

        const float phase = module.lfoPhase.load();
        const int w = b.getWidth();
        const int h = b.getHeight();
        const float cx = (float)b.getX();
        const float cy = (float)b.getY() + (float)h * 0.5f;
        const float amp = (float)h * 0.38f;

        // Draw sine wave
        juce::Path wave;
        for (int px = 0; px <= w; ++px)
        {
            const float t = (float)px / (float)w;
            const float y = cy - amp * std::sin(juce::MathConstants<float>::twoPi * t);
            if (px == 0) wave.startNewSubPath(cx + (float)px, y);
            else         wave.lineTo          (cx + (float)px, y);
        }
        g.setColour(juce::Colour(0xff224466));
        g.strokePath(wave, juce::PathStrokeType(1.0f));

        // Moving dot at current LFO position
        const float dotX = cx + phase * (float)w;
        const float dotY = cy - amp * std::sin(juce::MathConstants<float>::twoPi * phase);
        g.setColour(juce::Colour(0xff44aaff));
        g.fillEllipse(dotX - 4.0f, dotY - 4.0f, 8.0f, 8.0f);

        // Glow ring
        g.setColour(juce::Colour(0x4044aaff));
        g.drawEllipse(dotX - 7.0f, dotY - 7.0f, 14.0f, 14.0f, 1.5f);
    }

private:
    PhaserModule& module;
    void timerCallback() override { repaint(); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LFODisplay)
};

// ---------------------------------------------------------------------------

class PhaserEditor : public juce::Component
{
public:
    explicit PhaserEditor(PhaserModule& m) : module(m), lfoDisplay(m)
    {
        for (auto* s : { &rateKnob, &depthKnob, &cenKnob, &fbKnob, &mixKnob })
            s->setLookAndFeel(&knobLook);
        auto setupKnob = [this](juce::Slider& s, juce::Label& l,
                                const juce::String& name,
                                double lo, double hi, double val,
                                const juce::String& suffix = "")
        {
            s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
            s.setRange(lo, hi);
            s.setValue(val, juce::dontSendNotification);
            s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 56, 14);
            s.setNumDecimalPlacesToDisplay(2);
            if (suffix.isNotEmpty()) s.setTextValueSuffix(suffix);
            addAndMakeVisible(s);

            l.setText(name, juce::dontSendNotification);
            l.setJustificationType(juce::Justification::centred);
            l.setFont(juce::Font(10.0f));
            addAndMakeVisible(l);
        };

        setupKnob(rateKnob,  rateLabel,  "Rate",     0.05, 10.0, module.rate    .load(), " Hz");
        setupKnob(depthKnob, depthLabel, "Depth",    0.0,  1.0,  module.depth   .load());
        setupKnob(cenKnob,   cenLabel,   "Center",   100,  4000, module.center  .load(), " Hz");
        setupKnob(fbKnob,    fbLabel,    "Feedback", -0.99, 0.99, module.feedback.load());
        setupKnob(mixKnob,   mixLabel,   "Mix",      0.0,  1.0,  module.mix     .load());

        rateKnob .onValueChange = [this] { module.rate    .store((float)rateKnob .getValue()); };
        depthKnob.onValueChange = [this] { module.depth   .store((float)depthKnob.getValue()); };
        cenKnob  .onValueChange = [this] { module.center  .store((float)cenKnob  .getValue()); };
        fbKnob   .onValueChange = [this] { module.feedback.store((float)fbKnob   .getValue()); };
        mixKnob  .onValueChange = [this] { module.mix     .store((float)mixKnob  .getValue()); };

        stagesBox.addItem("2 stages", 1);
        stagesBox.addItem("4 stages", 2);
        stagesBox.addItem("6 stages", 3);
        stagesBox.addItem("8 stages", 4);
        const int stagesVal = module.stages.load();
        stagesBox.setSelectedId(stagesVal / 2, juce::dontSendNotification);
        stagesBox.onChange = [this]
        {
            module.stages.store(stagesBox.getSelectedId() * 2);
        };
        addAndMakeVisible(stagesBox);

        stagesLabel.setText("Stages", juce::dontSendNotification);
        stagesLabel.setJustificationType(juce::Justification::centred);
        stagesLabel.setFont(juce::Font(10.0f));
        addAndMakeVisible(stagesLabel);

        addAndMakeVisible(lfoDisplay);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff0d1117));

        auto titleBar = getLocalBounds().removeFromTop(22);
        g.setColour(juce::Colour(0xff1a1a3a));
        g.fillRoundedRectangle(titleBar.toFloat(), 6.0f);
        g.setColour(juce::Colour(0xff88aaff));
        g.setFont(juce::Font(12.0f, juce::Font::bold));
        g.drawText("PHASER", titleBar, juce::Justification::centred);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(8);
        area.removeFromTop(22 + 4);

        // Knob row
        auto knobRow = area.removeFromTop(80);
        const int kw = knobRow.getWidth() / 5;

        auto layoutKnob = [&](juce::Label& lbl, juce::Slider& knob)
        {
            auto col = knobRow.removeFromLeft(kw);
            lbl .setBounds(col.removeFromTop(14));
            knob.setBounds(col);
        };

        layoutKnob(rateLabel,  rateKnob);
        layoutKnob(depthLabel, depthKnob);
        layoutKnob(cenLabel,   cenKnob);
        layoutKnob(fbLabel,    fbKnob);
        layoutKnob(mixLabel,   mixKnob);

        area.removeFromTop(6);

        // Stages row
        auto stagesRow = area.removeFromTop(36);
        stagesLabel.setBounds(stagesRow.removeFromLeft(50));
        stagesBox  .setBounds(stagesRow.removeFromLeft(100));

        area.removeFromTop(4);
        lfoDisplay.setBounds(area);
    }

    ~PhaserEditor() override
    {
        for (auto* s : { &rateKnob, &depthKnob, &cenKnob, &fbKnob, &mixKnob })
            s->setLookAndFeel(nullptr);
    }

private:
    KnobLookAndFeel knobLook;   // must outlive knobs
    PhaserModule& module;

    juce::Slider rateKnob, depthKnob, cenKnob, fbKnob, mixKnob;
    juce::Label  rateLabel, depthLabel, cenLabel, fbLabel, mixLabel;

    juce::ComboBox stagesBox;
    juce::Label    stagesLabel;

    LFODisplay lfoDisplay;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhaserEditor)
};

} // namespace

std::unique_ptr<juce::Component> PhaserModule::createEditor()
{
    return std::make_unique<PhaserEditor>(*this);
}

// ---- Persistence -----------------------------------------------------------

void PhaserModule::saveState(juce::XmlElement& xml) const
{
    xml.setAttribute("rate",     rate    .load());
    xml.setAttribute("depth",    depth   .load());
    xml.setAttribute("center",   center  .load());
    xml.setAttribute("feedback", feedback.load());
    xml.setAttribute("mix",      mix     .load());
    xml.setAttribute("stages",   stages  .load());
}

void PhaserModule::loadState(const juce::XmlElement& xml)
{
    rate    .store((float)xml.getDoubleAttribute("rate",     rate    .load()));
    depth   .store((float)xml.getDoubleAttribute("depth",   depth   .load()));
    center  .store((float)xml.getDoubleAttribute("center",  center  .load()));
    feedback.store((float)xml.getDoubleAttribute("feedback",feedback .load()));
    mix     .store((float)xml.getDoubleAttribute("mix",     mix     .load()));
    stages  .store(        xml.getIntAttribute   ("stages", stages  .load()));
}

REGISTER_MODULE(PhaserModule);
