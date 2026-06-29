#include "DistortionModule.h"
#include "ModuleRegistry.h"
#include <DistortionData.h>

// ---- waveshapers -----------------------------------------------------------

float DistortionModule::shape(float x, int t, float drv) noexcept
{
    switch (t)
    {
        case 0: // Overdrive — tanh soft clip, normalised to ±1
        {
            const float td = std::tanh(drv);
            return td > 0.0f ? std::tanh(x * drv) / td : x;
        }
        case 1: // Distortion — hard clip
            return juce::jlimit(-1.0f, 1.0f, x * drv);

        case 2: // Fuzz — asymmetric exponential, approaches ±1 asymptotically
            return x >= 0.0f ?  (1.0f - std::exp(-x * drv))
                             : -(1.0f - std::exp( x * drv));

        case 3: // Fold — wavefolder, signal reflects back at ±1
        {
            // Add a large multiple of 4 (1000) to guarantee a positive fmod argument
            // while preserving the modular result (1000 = 250 × 4).
            const float s = x * drv;
            const float f = std::fmod(s + 1001.0f, 4.0f);
            return 1.0f - std::abs(f - 2.0f);
        }
        default: return x;
    }
}

// ---- DSP -------------------------------------------------------------------

DistortionModule::DistortionModule(SynthParameters&) {}

void DistortionModule::prepareToPlay(double sr, int)
{
    sampleRate    = sr;
    toneState[0]  = toneState[1] = 0.0f;
}

void DistortionModule::processBlock(juce::AudioBuffer<float>& buffer,
                                    juce::MidiBuffer&,
                                    int startSample, int numSamples)
{
    const int   t   = type .load();
    const float drv = drive.load();
    const float tn  = tone .load();
    const float mx  = mix  .load();
    const float lvl = level.load();

    // One-pole LP at 3 kHz — tone blends between filtered (dark) and full (bright)
    const float lp = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi
                                      * 3000.0f / (float)sampleRate);

    const int numCh = juce::jmin(buffer.getNumChannels(), 2);
    for (int ch = 0; ch < numCh; ++ch)
    {
        float* data = buffer.getWritePointer(ch, startSample);
        for (int i = 0; i < numSamples; ++i)
        {
            const float dry = data[i];
            float       wet = shape(dry, t, drv);

            // Tone: 0 = LP only (dark), 1 = wet only (bright)
            toneState[ch] += lp * (wet - toneState[ch]);
            wet = toneState[ch] + (wet - toneState[ch]) * tn;

            data[i] = (dry * (1.0f - mx) + wet * mx) * lvl;
        }
    }
}

// ---- Editor ----------------------------------------------------------------

namespace
{

class KnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    KnobLookAndFeel()
    {
        knobImage = juce::ImageCache::getFromMemory(DistortionAssets::knob_png,
                                                    DistortionAssets::knob_pngSize);
    }

    void drawRotarySlider(juce::Graphics& g,
                          int x, int y, int width, int height,
                          float sliderPos,
                          float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider&) override
    {
        if (!knobImage.isValid()) return;

        const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        const float size  = (float)juce::jmin(width, height);
        const float cx    = (float)x + (float)width  * 0.5f;
        const float cy    = (float)y + (float)height * 0.5f;

        const float scale = size / (float)knobImage.getWidth();

        juce::AffineTransform t =
            juce::AffineTransform::translation(-(float)knobImage.getWidth()  * 0.5f,
                                               -(float)knobImage.getHeight() * 0.5f)
                                 .rotated(angle)
                                 .scaled(scale)
                                 .translated(cx, cy);

        g.drawImageTransformed(knobImage, t, false);
    }

private:
    juce::Image knobImage;
};

// Draws the waveshaper transfer function (input X → output Y) for the current
// type and drive.  Repaints whenever any parameter changes.
class CurveDisplay : public juce::Component
{
public:
    int   type  = 0;
    float drive = 4.0f;

    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().reduced(1);
        g.setColour(juce::Colour(0xff080810));
        g.fillRoundedRectangle(b.toFloat(), 4.0f);

        const float w  = (float)b.getWidth();
        const float h  = (float)b.getHeight();
        const float cx = (float)b.getX() + w * 0.5f;
        const float cy = (float)b.getY() + h * 0.5f;

        // Grid
        g.setColour(juce::Colour(0xff1a1a30));
        g.drawHorizontalLine((int)cy, (float)b.getX(), (float)b.getRight());
        g.drawVerticalLine  ((int)cx, (float)b.getY(), (float)b.getBottom());

        // Diagonal unity-gain reference
        g.setColour(juce::Colour(0xff1e1e3a));
        g.drawLine((float)b.getX(), (float)b.getBottom(),
                   (float)b.getRight(), (float)b.getY(), 1.0f);

        // Waveshaper curve
        static const juce::Colour typeColour[] = {
            juce::Colour(0xffff8844),  // overdrive — orange
            juce::Colour(0xffff3344),  // distortion — red
            juce::Colour(0xffcc44ff),  // fuzz — purple
            juce::Colour(0xff44ffcc),  // fold — cyan
        };
        g.setColour(typeColour[juce::jlimit(0, 3, type)]);

        juce::Path curve;
        for (int px = 0; px <= (int)w; ++px)
        {
            float x  = ((float)px / w) * 2.0f - 1.0f;
            float y  = DistortionModule::shape(x, type, drive);
            float sy = cy - y * h * 0.44f;

            if (px == 0) curve.startNewSubPath((float)b.getX() + (float)px, sy);
            else         curve.lineTo         ((float)b.getX() + (float)px, sy);
        }
        g.strokePath(curve, juce::PathStrokeType(1.8f,
                                                  juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
    }
};

class DistortionEditor : public juce::Component
{
public:
    explicit DistortionEditor(DistortionModule& m) : module(m)
    {
        // Type selector
        typeBox.addItem("Overdrive",  1);

        typeBox.addItem("Distortion", 2);
        typeBox.addItem("Fuzz",       3);
        typeBox.addItem("Fold",       4);
        typeBox.setSelectedId(module.type.load() + 1, juce::dontSendNotification);
        typeBox.onChange = [this]
        {
            module.type.store(typeBox.getSelectedId() - 1);
            curve.type = module.type.load();
            curve.repaint();
        };
        addAndMakeVisible(typeBox);

        auto setupKnob = [this](juce::Slider& s, juce::Label& l,
                                const juce::String& name,
                                double lo, double hi, double val)
        {
            s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
            s.setRange(lo, hi);
            s.setValue(val, juce::dontSendNotification);
            s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 52, 14);
            addAndMakeVisible(s);

            l.setText(name, juce::dontSendNotification);
            l.setJustificationType(juce::Justification::centred);
            l.setFont(juce::Font(10.0f));
            addAndMakeVisible(l);
        };

        setupKnob(driveKnob, driveLabel, "Drive", 1.0,  20.0, module.drive.load());
        setupKnob(toneKnob,  toneLabel,  "Tone",  0.0,  1.0,  module.tone .load());
        setupKnob(mixKnob,   mixLabel,   "Mix",   0.0,  1.0,  module.mix  .load());
        setupKnob(levelKnob, levelLabel, "Level", 0.0,  1.0,  module.level.load());

        for (auto* s : { &driveKnob, &toneKnob, &mixKnob, &levelKnob })
            s->setLookAndFeel(&knobLook);

        driveKnob.setSkewFactorFromMidPoint(5.0);

        driveKnob.onValueChange = [this]
        {
            module.drive.store((float)driveKnob.getValue());
            curve.drive = module.drive.load();
            curve.repaint();
        };
        toneKnob .onValueChange = [this] { module.tone .store((float)toneKnob .getValue()); };
        mixKnob  .onValueChange = [this] { module.mix  .store((float)mixKnob  .getValue()); };
        levelKnob.onValueChange = [this] { module.level.store((float)levelKnob.getValue()); };

        curve.type  = module.type .load();
        curve.drive = module.drive.load();
        addAndMakeVisible(curve);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff0d1117));

        auto titleBar = getLocalBounds().removeFromTop(22);
        g.setColour(juce::Colour(0xff1a1a2e));
        g.fillRoundedRectangle(titleBar.toFloat(), 6.0f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(8);
        area.removeFromTop(22);
        area.removeFromTop(6);

        typeBox.setBounds(area.removeFromTop(24));
        area.removeFromTop(6);

        auto knobRow = area.removeFromTop(72);
        int  kw      = knobRow.getWidth() / 4;

        auto layoutKnob = [&](juce::Label& lbl, juce::Slider& knob)
        {
            auto col = knobRow.removeFromLeft(kw);
            lbl .setBounds(col.removeFromTop(14));
            knob.setBounds(col);
        };
        layoutKnob(driveLabel, driveKnob);
        layoutKnob(toneLabel,  toneKnob);
        layoutKnob(mixLabel,   mixKnob);
        layoutKnob(levelLabel, levelKnob);

        area.removeFromTop(6);
        curve.setBounds(area);
    }

    ~DistortionEditor() override
    {
        for (auto* s : { &driveKnob, &toneKnob, &mixKnob, &levelKnob })
            s->setLookAndFeel(nullptr);
    }

private:
    DistortionModule& module;

    KnobLookAndFeel  knobLook;
    juce::ComboBox   typeBox;

    juce::Slider driveKnob, toneKnob, mixKnob, levelKnob;
    juce::Label  driveLabel, toneLabel, mixLabel, levelLabel;

    CurveDisplay curve;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DistortionEditor)
};

} // namespace

std::unique_ptr<juce::Component> DistortionModule::createEditor()
{
    return std::make_unique<DistortionEditor>(*this);
}

juce::Image DistortionModule::getLogo() const
{
    return juce::ImageCache::getFromMemory(
        DistortionAssets::distortionplate_png, DistortionAssets::distortionplate_pngSize);
}

void DistortionModule::saveState(juce::XmlElement& xml) const
{
    xml.setAttribute("type",  type .load());
    xml.setAttribute("drive", drive.load());
    xml.setAttribute("tone",  tone .load());
    xml.setAttribute("mix",   mix  .load());
    xml.setAttribute("level", level.load());
}

void DistortionModule::loadState(const juce::XmlElement& xml)
{
    type .store(xml.getIntAttribute   ("type",  type .load()));
    drive.store((float)xml.getDoubleAttribute("drive", drive.load()));
    tone .store((float)xml.getDoubleAttribute("tone",  tone .load()));
    mix  .store((float)xml.getDoubleAttribute("mix",   mix  .load()));
    level.store((float)xml.getDoubleAttribute("level", level.load()));
}

REGISTER_MODULE(DistortionModule);
