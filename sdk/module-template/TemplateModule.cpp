#include "TemplateModule.h"
#include "ModuleRegistry.h"

// ---- DSP -------------------------------------------------------------------

TemplateModule::TemplateModule(SynthParameters& /*params*/) {}

void TemplateModule::prepareToPlay(double /*sampleRate*/, int /*samplesPerBlock*/) {}

void TemplateModule::processBlock(juce::AudioBuffer<float>& buffer,
                                  juce::MidiBuffer& /*midi*/,
                                  int startSample, int numSamples)
{
    buffer.applyGain(startSample, numSamples, gain.load());
}

// ---- Editor ----------------------------------------------------------------

namespace
{

class TemplateEditor : public juce::Component
{
public:
    explicit TemplateEditor(TemplateModule& m) : module(m)
    {
        gainSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        gainSlider.setRange(0.0, 2.0);
        gainSlider.setValue(1.0, juce::dontSendNotification);
        gainSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
        gainSlider.onValueChange = [this] {
            module.gain.store((float)gainSlider.getValue());
        };
        addAndMakeVisible(gainSlider);

        gainLabel.setText("Gain", juce::dontSendNotification);
        gainLabel.setJustificationType(juce::Justification::centred);
        gainLabel.setFont(juce::Font(10.0f));
        addAndMakeVisible(gainLabel);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff0d1b2a));

        auto titleBar = getLocalBounds().removeFromTop(22);
        g.setColour(juce::Colour(0xff1e3a5f));
        g.fillRoundedRectangle(titleBar.toFloat(), 6.0f);
        g.setColour(juce::Colours::lightblue.withAlpha(0.8f));
        g.setFont(juce::Font(12.0f, juce::Font::bold));
        g.drawText("TEMPLATE", titleBar, juce::Justification::centred);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(8);
        area.removeFromTop(30);

        auto col = area.removeFromLeft(80);
        gainLabel.setBounds(col.removeFromTop(14));
        gainSlider.setBounds(col);
    }

private:
    TemplateModule& module;
    juce::Slider    gainSlider;
    juce::Label     gainLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TemplateEditor)
};

} // namespace

std::unique_ptr<juce::Component> TemplateModule::createEditor()
{
    return std::make_unique<TemplateEditor>(*this);
}

// Self-registers with ModuleRegistry at static-init time.
// The string in REGISTER_MODULE must match the class name exactly.
REGISTER_MODULE(TemplateModule);
