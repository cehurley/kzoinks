#include "EQModule.h"
#include "ModuleRegistry.h"

// ---- DSP -------------------------------------------------------------------

EQModule::EQModule(SynthParameters&)
{
    for (int i = 0; i < numBands; ++i)
        gains[i].store(0.0f);
}

void EQModule::prepareToPlay(double sr, int)
{
    sampleRate = sr;
    for (size_t ch = 0; ch < 2; ++ch)
        for (size_t b = 0; b < (size_t)numBands; ++b)
            states[ch][b] = {};
}

void EQModule::peakCoeffs(float freq, float gainDB, float Q, double sr,
                           float& b0n, float& b1n, float& b2n, float& a2n) noexcept
{
    // RBJ Audio EQ Cookbook — peaking EQ filter
    float A     = std::pow(10.0f, gainDB / 40.0f);
    float w0    = 2.0f * juce::MathConstants<float>::pi * freq / (float)sr;
    float alpha = std::sin(w0) / (2.0f * Q);
    float cosw0 = std::cos(w0);
    float a0inv = 1.0f / (1.0f + alpha / A);

    b0n = (1.0f + alpha * A) * a0inv;
    b1n = (-2.0f * cosw0)    * a0inv;   // a1n/a0 == b1n/a0 for peak filters
    b2n = (1.0f - alpha * A) * a0inv;
    a2n = (1.0f - alpha / A) * a0inv;
}

void EQModule::processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer&,
                             int startSample, int numSamples)
{
    const int numChannels = juce::jmin(audio.getNumChannels(), 2);

    for (int band = 0; band < numBands; ++band)
    {
        const float gainDB = gains[band].load();
        if (std::abs(gainDB) < 0.01f) continue;  // unity — skip

        float b0n, b1n, b2n, a2n;
        peakCoeffs(frequencies[(size_t)band], gainDB, bandQ, sampleRate, b0n, b1n, b2n, a2n);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float*       data = audio.getWritePointer(ch, startSample);
            BiquadState& s    = states[ch][band];

            for (int i = 0; i < numSamples; ++i)
            {
                const float x = data[i];
                const float y = b0n*x + b1n*s.x1 + b2n*s.x2 - b1n*s.y1 - a2n*s.y2;
                s.x2 = s.x1;  s.x1 = x;
                s.y2 = s.y1;  s.y1 = y;
                data[i] = y;
            }
        }
    }
}

// ---- Editor ----------------------------------------------------------------

namespace
{

class EQEditor : public juce::Component
{
public:
    explicit EQEditor(EQModule& eq) : module(eq)
    {
        for (int i = 0; i < EQModule::numBands; ++i)
        {
            auto& s = sliders[i];
            s.setSliderStyle(juce::Slider::LinearVertical);
            s.setRange(-EQModule::maxGainDB, EQModule::maxGainDB);
            s.setValue(module.gains[i].load(), juce::dontSendNotification);
            s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 48, 14);
            s.setNumDecimalPlacesToDisplay(1);
            s.onValueChange = [this, i] {
                module.gains[(size_t)i].store((float)sliders[(size_t)i].getValue());
            };
            addAndMakeVisible(s);

            auto& l = freqLabels[i];
            l.setText(EQModule::labels[(size_t)i], juce::dontSendNotification);
            l.setJustificationType(juce::Justification::centred);
            l.setFont(juce::Font(10.0f));
            addAndMakeVisible(l);
        }
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds();

        g.setColour(juce::Colour(0xff0d1b2a));
        g.fillRoundedRectangle(bounds.toFloat(), 6.0f);

        // Title bar
        auto titleBar = bounds.removeFromTop(22);
        g.setColour(juce::Colour(0xff1a3a1a));
        g.fillRoundedRectangle(titleBar.toFloat(), 6.0f);
        g.setColour(juce::Colours::lightgreen.withAlpha(0.8f));
        g.setFont(juce::Font(12.0f, juce::Font::bold));
        g.drawText("GRAPHIC EQ", titleBar, juce::Justification::centred);

        // 0 dB centre line across the slider track area
        if (!sliders[0].getBounds().isEmpty())
        {
            // The slider track sits above the textbox (14px) and the freq label (16px)
            auto sliderBounds = sliders[0].getBounds();
            int trackHeight   = sliderBounds.getHeight() - 14;  // approx textbox height
            int zeroDby = sliderBounds.getY() + trackHeight / 2;

            g.setColour(juce::Colour(0xff2a4a2a));
            g.drawHorizontalLine(zeroDby,
                                 (float)sliders[0].getX(),
                                 (float)sliders[EQModule::numBands - 1].getRight());

            // ±6 dB marks
            int sixDB = trackHeight / 4;
            g.setColour(juce::Colour(0xff1a3a1a));
            g.drawHorizontalLine(zeroDby - sixDB,
                                 (float)sliders[0].getX(),
                                 (float)sliders[EQModule::numBands - 1].getRight());
            g.drawHorizontalLine(zeroDby + sixDB,
                                 (float)sliders[0].getX(),
                                 (float)sliders[EQModule::numBands - 1].getRight());
        }
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(6);
        area.removeFromTop(22);  // title bar
        area.removeFromTop(4);

        auto freqRow   = area.removeFromBottom(16);
        int  colWidth  = area.getWidth() / EQModule::numBands;

        for (int i = 0; i < EQModule::numBands; ++i)
        {
            auto col = area.removeFromLeft(colWidth);
            sliders[i].setBounds(col.reduced(2, 0));

            auto labelCol = freqRow.removeFromLeft(colWidth);
            freqLabels[i].setBounds(labelCol);
        }
    }

private:
    EQModule& module;
    juce::Slider sliders[EQModule::numBands];
    juce::Label  freqLabels[EQModule::numBands];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EQEditor)
};

} // namespace

std::unique_ptr<juce::Component> EQModule::createEditor()
{
    return std::make_unique<EQEditor>(*this);
}

void EQModule::saveState(juce::XmlElement& xml) const
{
    for (int i = 0; i < numBands; ++i)
        xml.setAttribute("gain" + juce::String(i), gains[(size_t)i].load());
}

void EQModule::loadState(const juce::XmlElement& xml)
{
    for (int i = 0; i < numBands; ++i)
        gains[(size_t)i].store((float)xml.getDoubleAttribute("gain" + juce::String(i), 0.0));
}

REGISTER_MODULE(EQModule);
