#include "RingModModule.h"
#include "ModuleRegistry.h"
#include "RingModData.h"

// ---- DSP -------------------------------------------------------------------

RingModModule::RingModModule(SynthParameters&) {}

void RingModModule::prepareToPlay(double sr, int)
{
    sampleRate = sr;
    carrierPhase[0] = carrierPhase[1] = 0.0;
}

float RingModModule::carrierSample(double phase, int waveform) noexcept
{
    switch (waveform)
    {
        case Triangle: return (float)(4.0 * std::abs(phase - 0.5) - 1.0);
        case Square:   return phase < 0.5 ? 1.0f : -1.0f;
        default:       return (float)std::sin(juce::MathConstants<double>::twoPi * phase);
    }
}

void RingModModule::processBlock(juce::AudioBuffer<float>& buffer,
                                 juce::MidiBuffer&,
                                 int startSample, int numSamples)
{
    const float freqVal   = freq.load();
    const int   waveVal   = wave.load();
    const float depthVal  = juce::jlimit(0.0f, 1.0f, depth.load());
    const float spreadVal = juce::jlimit(0.0f, 180.0f, spread.load());
    const float mixVal    = juce::jlimit(0.0f, 1.0f, mix.load());

    const double phaseInc    = freqVal / sampleRate;
    const double spreadFrac  = spreadVal / 360.0; // degrees -> fraction of a cycle

    const int numCh = buffer.getNumChannels();
    float* dataL = buffer.getWritePointer(0, startSample);
    float* dataR = numCh >= 2 ? buffer.getWritePointer(1, startSample) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        const float dryL = dataL[i];
        const float dryR = dataR ? dataR[i] : dryL;

        const float carL = carrierSample(carrierPhase[0], waveVal);
        const float carR = carrierSample(std::fmod(carrierPhase[1] + spreadFrac, 1.0), waveVal);

        // depth blends the carrier between unipolar (0..1, tremolo-style AM) and
        // bipolar (-1..1, full ring mod) — bipolar is what produces the classic
        // inharmonic ring-mod sidebands; unipolar just amplitude-modulates.
        const float carUniL = 0.5f + 0.5f * carL;
        const float carUniR = 0.5f + 0.5f * carR;
        const float modL = carUniL + depthVal * (carL - carUniL);
        const float modR = carUniR + depthVal * (carR - carUniR);

        const float wetL = dryL * modL;
        const float wetR = dryR * modR;

        dataL[i] = dryL * (1.0f - mixVal) + wetL * mixVal;
        if (dataR) dataR[i] = dryR * (1.0f - mixVal) + wetR * mixVal;

        carrierPhase[0] += phaseInc;
        if (carrierPhase[0] >= 1.0) carrierPhase[0] -= 1.0;
        carrierPhase[1] += phaseInc;
        if (carrierPhase[1] >= 1.0) carrierPhase[1] -= 1.0;
    }

    carrierPhaseOut.store((float)carrierPhase[0]);
}

// ---- Editor ----------------------------------------------------------------

namespace
{

class KnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    KnobLookAndFeel()
    {
        knobImage = juce::ImageCache::getFromMemory(RingModAssets::lightblue_png,
                                                    RingModAssets::lightblue_pngSize);
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

class CarrierDisplay : public juce::Component, private juce::Timer
{
public:
    explicit CarrierDisplay(RingModModule& m) : module(m) { startTimerHz(30); }

    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().reduced(2);
        g.setColour(juce::Colour(0xff0a0e14));
        g.fillRoundedRectangle(b.toFloat(), 4.0f);

        const int   waveVal = module.wave.load();
        const float depthVal = module.depth.load();

        auto plot = b.reduced(6).toFloat();
        const float midY = plot.getCentreY();

        juce::Path path;
        const int   numPts = 128;
        for (int i = 0; i <= numPts; ++i)
        {
            const double ph = (double)i / numPts;
            const float  car = RingModModule::carrierSample(ph, waveVal);
            const float  uni = 0.5f + 0.5f * car;
            const float  v   = uni + depthVal * (car - uni);
            const float  x   = plot.getX() + plot.getWidth() * (float)i / numPts;
            const float  y   = midY - v * plot.getHeight() * 0.45f;
            if (i == 0) path.startNewSubPath(x, y);
            else        path.lineTo(x, y);
        }

        g.setColour(juce::Colour(0xffff66cc).withAlpha(0.8f));
        g.strokePath(path, juce::PathStrokeType(1.5f));

        // Moving phase marker
        const float phase = module.carrierPhaseOut.load();
        const float markerX = plot.getX() + plot.getWidth() * phase;
        g.setColour(juce::Colours::white.withAlpha(0.5f));
        g.drawVerticalLine((int)markerX, plot.getY(), plot.getBottom());

        g.setColour(juce::Colours::white.withAlpha(0.3f));
        g.setFont(juce::Font(9.0f));
        g.drawText(juce::String((int)module.freq.load()) + " Hz",
                   b.removeFromBottom(12), juce::Justification::centredRight);
    }

private:
    void timerCallback() override { repaint(); }

    RingModModule& module;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CarrierDisplay)
};

// ---------------------------------------------------------------------------

class RingModEditor : public juce::Component
{
public:
    explicit RingModEditor(RingModModule& m) : module(m), display(m)
    {
        for (auto* s : { &freqKnob, &depthKnob, &spreadKnob, &mixKnob })
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

        setupKnob(freqKnob,   freqLabel,   "Freq",   1.0,  5000.0, module.freq.load(), " Hz");
        freqKnob.setSkewFactorFromMidPoint(440.0);
        setupKnob(depthKnob,  depthLabel,  "Depth",  0.0,  1.0,    module.depth.load());
        setupKnob(spreadKnob, spreadLabel, "Spread", 0.0,  180.0,  module.spread.load(), " deg");
        setupKnob(mixKnob,    mixLabel,    "Mix",    0.0,  1.0,    module.mix.load());

        freqKnob.onValueChange   = [this] { module.freq  .store((float)freqKnob  .getValue()); };
        depthKnob.onValueChange  = [this] { module.depth .store((float)depthKnob .getValue()); };
        spreadKnob.onValueChange = [this] { module.spread.store((float)spreadKnob.getValue()); };
        mixKnob.onValueChange    = [this] { module.mix   .store((float)mixKnob   .getValue()); };

        waveBox.addItem("Sine",     RingModModule::Sine + 1);
        waveBox.addItem("Triangle", RingModModule::Triangle + 1);
        waveBox.addItem("Square",   RingModModule::Square + 1);
        waveBox.setSelectedId(module.wave.load() + 1, juce::dontSendNotification);
        waveBox.onChange = [this] { module.wave.store(waveBox.getSelectedId() - 1); };
        addAndMakeVisible(waveBox);

        waveLabel.setText("Wave", juce::dontSendNotification);
        waveLabel.setJustificationType(juce::Justification::centred);
        waveLabel.setFont(juce::Font(10.0f));
        addAndMakeVisible(waveLabel);

        addAndMakeVisible(display);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0x3c5438d9));

        auto titleBar = getLocalBounds().removeFromTop(22);
        g.setColour(juce::Colour(0xff1a1a2a));
        g.fillRoundedRectangle(titleBar.toFloat(), 6.0f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(8);
        area.removeFromTop(22 + 4);

        display.setBounds(area.removeFromTop(60));
        area.removeFromTop(6);

        auto waveRow = area.removeFromTop(40);
        auto waveCol = waveRow.removeFromLeft(100);
        waveLabel.setBounds(waveCol.removeFromTop(14));
        waveBox.setBounds(waveCol.removeFromTop(22));

        area.removeFromTop(6);
        auto knobRow = area.removeFromTop(80);
        const int kw = knobRow.getWidth() / 4;

        auto layoutKnob = [&](juce::Label& lbl, juce::Slider& knob)
        {
            auto col = knobRow.removeFromLeft(kw);
            lbl .setBounds(col.removeFromTop(14));
            knob.setBounds(col);
        };

        layoutKnob(freqLabel,   freqKnob);
        layoutKnob(depthLabel,  depthKnob);
        layoutKnob(spreadLabel, spreadKnob);
        layoutKnob(mixLabel,    mixKnob);
    }

    ~RingModEditor() override
    {
        for (auto* s : { &freqKnob, &depthKnob, &spreadKnob, &mixKnob })
            s->setLookAndFeel(nullptr);
    }

private:
    KnobLookAndFeel knobLook;   // must outlive knobs
    RingModModule&  module;
    CarrierDisplay  display;

    juce::Slider   freqKnob, depthKnob, spreadKnob, mixKnob;
    juce::Label    freqLabel, depthLabel, spreadLabel, mixLabel;
    juce::ComboBox waveBox;
    juce::Label    waveLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RingModEditor)
};

} // namespace

std::unique_ptr<juce::Component> RingModModule::createEditor()
{
    return std::make_unique<RingModEditor>(*this);
}

juce::Image RingModModule::getLogo() const
{
    return juce::ImageCache::getFromMemory(
        RingModAssets::faceplaterm_png, RingModAssets::faceplaterm_pngSize);
}

// ---- Persistence -----------------------------------------------------------

void RingModModule::saveState(juce::XmlElement& xml) const
{
    xml.setAttribute("freq",   freq  .load());
    xml.setAttribute("wave",   wave  .load());
    xml.setAttribute("depth",  depth .load());
    xml.setAttribute("spread", spread.load());
    xml.setAttribute("mix",    mix   .load());
}

void RingModModule::loadState(const juce::XmlElement& xml)
{
    freq  .store((float)xml.getDoubleAttribute("freq",   freq  .load()));
    wave  .store(        xml.getIntAttribute   ("wave",   wave  .load()));
    depth .store((float)xml.getDoubleAttribute("depth",  depth .load()));
    spread.store((float)xml.getDoubleAttribute("spread", spread.load()));
    mix   .store((float)xml.getDoubleAttribute("mix",    mix   .load()));
}

REGISTER_MODULE(RingModModule);
