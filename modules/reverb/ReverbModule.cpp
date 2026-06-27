#include "ReverbModule.h"
#include "ModuleRegistry.h"
#include <ReverbData.h>

// ---- DSP -------------------------------------------------------------------

ReverbModule::ReverbModule(SynthParameters&) {}

void ReverbModule::applyParams()
{
    juce::Reverb::Parameters p;
    p.roomSize   = roomSize.load();
    p.damping    = damping .load();
    p.width      = width   .load();
    p.wetLevel   = mix     .load();
    p.dryLevel   = 1.0f - mix.load();
    p.freezeMode = 0.0f;
    reverb.setParameters(p);
}

void ReverbModule::prepareToPlay(double sampleRate, int)
{
    reverb.setSampleRate(sampleRate);
    applyParams();
    reverb.reset();
}

void ReverbModule::processBlock(juce::AudioBuffer<float>& buffer,
                                juce::MidiBuffer&,
                                int startSample, int numSamples)
{
    applyParams();

    const int numCh = buffer.getNumChannels();

    if (numCh >= 2)
    {
        reverb.processStereo(buffer.getWritePointer(0, startSample),
                             buffer.getWritePointer(1, startSample),
                             numSamples);
    }
    else if (numCh == 1)
    {
        reverb.processMono(buffer.getWritePointer(0, startSample), numSamples);
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
        knobImage = juce::ImageCache::getFromMemory(
            ReverbAssets::reverbknob_png, ReverbAssets::reverbknob_pngSize);
    }

    void drawRotarySlider(juce::Graphics& g,
                          int x, int y, int width, int height,
                          float sliderPos,
                          float, float,
                          juce::Slider&) override
    {
        if (!knobImage.isValid()) return;

        const float angle = juce::MathConstants<float>::pi * 0.75f
                            * (2.0f * sliderPos - 1.0f);

        const int   size = juce::jmin(width, height);
        const float cx   = (float)x + (float)width  * 0.5f;
        const float cy   = (float)y + (float)height * 0.5f;

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

class ReverbEditor : public juce::Component
{
public:
    explicit ReverbEditor(ReverbModule& m) : module(m)
    {
        for (auto* s : { &roomKnob, &dampKnob, &widthKnob, &mixKnob })
            s->setLookAndFeel(&knobLook);

        auto setup = [this](juce::Slider& s, juce::Label& l,
                            const juce::String& name,
                            double lo, double hi, double val)
        {
            s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
            s.setRange(lo, hi);
            s.setValue(val, juce::dontSendNotification);
            s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 56, 14);
            s.setNumDecimalPlacesToDisplay(2);
            addAndMakeVisible(s);

            l.setText(name, juce::dontSendNotification);
            l.setJustificationType(juce::Justification::centred);
            l.setFont(juce::Font(10.0f));
            addAndMakeVisible(l);
        };

        setup(roomKnob,  roomLabel,  "Room",    0.0, 1.0, module.roomSize.load());
        setup(dampKnob,  dampLabel,  "Damping", 0.0, 1.0, module.damping .load());
        setup(widthKnob, widthLabel, "Width",   0.0, 1.0, module.width   .load());
        setup(mixKnob,   mixLabel,   "Mix",     0.0, 1.0, module.mix     .load());

        roomKnob .onValueChange = [this] { module.roomSize.store((float)roomKnob .getValue()); repaint(); };
        dampKnob .onValueChange = [this] { module.damping .store((float)dampKnob .getValue()); repaint(); };
        widthKnob.onValueChange = [this] { module.width   .store((float)widthKnob.getValue()); };
        mixKnob  .onValueChange = [this] { module.mix     .store((float)mixKnob  .getValue()); };
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff0d1117));

        auto titleBar = getLocalBounds().removeFromTop(22);
        g.setColour(juce::Colour(0xff1a2a3a));
        g.fillRoundedRectangle(titleBar.toFloat(), 6.0f);
        g.setColour(juce::Colour(0xff88ccff));
        g.setFont(juce::Font(12.0f, juce::Font::bold));
        g.drawText("REVERB", titleBar, juce::Justification::centred);

        // Simple room shape illustration
        auto area = getLocalBounds().reduced(16);
        area.removeFromTop(22 + 8 + 80 + 8); // below title + knobs
        if (area.getHeight() > 20)
        {
            g.setColour(juce::Colour(0xff112233));
            g.fillRoundedRectangle(area.toFloat(), 8.0f);

            // Decay curve: exponential decay line across the display area
            juce::Path decay;
            const float w = (float)area.getWidth()  - 16.0f;
            const float h = (float)area.getHeight() - 16.0f;
            const float ox = (float)area.getX() + 8.0f;
            const float oy = (float)area.getY() + 8.0f + h;

            const float room = module.roomSize.load();
            const float damp = module.damping .load();
            const float decayRate = 4.0f * (1.0f - room * 0.9f) * (1.0f + damp * 0.5f);

            decay.startNewSubPath(ox, oy);
            for (int px = 1; px <= (int)w; ++px)
            {
                const float t = (float)px / w;
                const float amp = std::exp(-decayRate * t);
                decay.lineTo(ox + (float)px, oy - amp * h);
            }
            g.setColour(juce::Colour(0xff4488bb));
            g.strokePath(decay, juce::PathStrokeType(1.5f));
        }
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(10);
        area.removeFromTop(22 + 6);

        auto knobRow = area.removeFromTop(80);
        const int kw = knobRow.getWidth() / 4;

        auto layoutKnob = [&](juce::Label& lbl, juce::Slider& knob)
        {
            auto col = knobRow.removeFromLeft(kw);
            lbl .setBounds(col.removeFromTop(14));
            knob.setBounds(col);
        };

        layoutKnob(roomLabel,  roomKnob);
        layoutKnob(dampLabel,  dampKnob);
        layoutKnob(widthLabel, widthKnob);
        layoutKnob(mixLabel,   mixKnob);
    }

    ~ReverbEditor() override
    {
        for (auto* s : { &roomKnob, &dampKnob, &widthKnob, &mixKnob })
            s->setLookAndFeel(nullptr);
    }

private:
    KnobLookAndFeel knobLook;
    ReverbModule&   module;

    juce::Slider roomKnob, dampKnob, widthKnob, mixKnob;
    juce::Label  roomLabel, dampLabel, widthLabel, mixLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReverbEditor)
};

} // namespace

std::unique_ptr<juce::Component> ReverbModule::createEditor()
{
    return std::make_unique<ReverbEditor>(*this);
}

// ---- Persistence -----------------------------------------------------------

void ReverbModule::saveState(juce::XmlElement& xml) const
{
    xml.setAttribute("roomSize", roomSize.load());
    xml.setAttribute("damping",  damping .load());
    xml.setAttribute("width",    width   .load());
    xml.setAttribute("mix",      mix     .load());
}

void ReverbModule::loadState(const juce::XmlElement& xml)
{
    roomSize.store((float)xml.getDoubleAttribute("roomSize", roomSize.load()));
    damping .store((float)xml.getDoubleAttribute("damping",  damping .load()));
    width   .store((float)xml.getDoubleAttribute("width",    width   .load()));
    mix     .store((float)xml.getDoubleAttribute("mix",      mix     .load()));
}

REGISTER_MODULE(ReverbModule);
