#include "SynthBaseModule.h"
#include "ModuleRegistry.h"

// ---- Editor ----------------------------------------------------------------

namespace
{

struct Knob
{
    juce::Slider slider;
    juce::Label  label;

    void setup(juce::Component& parent, const juce::String& name,
               double lo, double hi, double val, int decimals = 2)
    {
        slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        slider.setRange(lo, hi);
        slider.setValue(val, juce::dontSendNotification);
        slider.setNumDecimalPlacesToDisplay(decimals);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 56, 14);
        parent.addAndMakeVisible(slider);

        label.setText(name, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(juce::Font(10.0f));
        parent.addAndMakeVisible(label);
    }

    void setBounds(juce::Rectangle<int> area)
    {
        label.setBounds(area.removeFromTop(14));
        slider.setBounds(area);
    }
};

class SynthBaseEditor : public juce::Component
{
public:
    explicit SynthBaseEditor(SynthParameters& p) : params(p)
    {
        waveformBox.addItem("Saw",    1);
        waveformBox.addItem("Square", 2);
        waveformBox.addItem("Sine",   3);
        waveformBox.setSelectedId(params.waveform.load() + 1, juce::dontSendNotification);
        waveformBox.onChange = [this] {
            params.waveform.store(waveformBox.getSelectedId() - 1);
        };
        addAndMakeVisible(waveformBox);

        waveLabel.setText("Wave", juce::dontSendNotification);
        waveLabel.setJustificationType(juce::Justification::centred);
        waveLabel.setFont(juce::Font(10.0f));
        addAndMakeVisible(waveLabel);

        osc2Detune.setup(*this, "Detune",   -50.0, 50.0,    params.detune.load(),        1);
        dirt      .setup(*this, "Dirt",       0.0,  1.0,    params.dirt.load(),          2);
        cutoff    .setup(*this, "Cutoff",    80.0, 18000.0, params.filterCutoff.load(),  0);
        resonance .setup(*this, "Res",        0.1,  4.0,    params.filterRes.load(),     2);
        envAmt    .setup(*this, "Env Amt",    0.0,  1.0,    params.filterEnvAmt.load(),  2);

        ampA.setup(*this, "A",  0.001, 4.0, params.ampAttack.load(),   3);
        ampD.setup(*this, "D",  0.001, 4.0, params.ampDecay.load(),    3);
        ampS.setup(*this, "S",  0.0,   1.0, params.ampSustain.load(),  2);
        ampR.setup(*this, "R",  0.001, 8.0, params.ampRelease.load(),  3);

        filtA.setup(*this, "A",  0.001, 4.0, params.filtAttack.load(),  3);
        filtD.setup(*this, "D",  0.001, 4.0, params.filtDecay.load(),   3);
        filtS.setup(*this, "S",  0.0,   1.0, params.filtSustain.load(), 2);
        filtR.setup(*this, "R",  0.001, 8.0, params.filtRelease.load(), 3);

        cutoff.slider.setSkewFactorFromMidPoint(1000.0);

        osc2Detune.slider.onValueChange = [this] { params.detune.store       ((float)osc2Detune.slider.getValue()); };
        dirt.slider.onValueChange       = [this] { params.dirt.store         ((float)dirt.slider.getValue()); };
        cutoff.slider.onValueChange     = [this] { params.filterCutoff.store ((float)cutoff.slider.getValue()); };
        resonance.slider.onValueChange  = [this] { params.filterRes.store    ((float)resonance.slider.getValue()); };
        envAmt.slider.onValueChange     = [this] { params.filterEnvAmt.store ((float)envAmt.slider.getValue()); };

        ampA.slider.onValueChange = [this] { params.ampAttack.store  ((float)ampA.slider.getValue()); };
        ampD.slider.onValueChange = [this] { params.ampDecay.store   ((float)ampD.slider.getValue()); };
        ampS.slider.onValueChange = [this] { params.ampSustain.store ((float)ampS.slider.getValue()); };
        ampR.slider.onValueChange = [this] { params.ampRelease.store ((float)ampR.slider.getValue()); };

        filtA.slider.onValueChange = [this] { params.filtAttack.store  ((float)filtA.slider.getValue()); };
        filtD.slider.onValueChange = [this] { params.filtDecay.store   ((float)filtD.slider.getValue()); };
        filtS.slider.onValueChange = [this] { params.filtSustain.store ((float)filtS.slider.getValue()); };
        filtR.slider.onValueChange = [this] { params.filtRelease.store ((float)filtR.slider.getValue()); };

        sectionAmp .setText("AMP ENV",    juce::dontSendNotification);
        sectionFilt.setText("FILTER ENV", juce::dontSendNotification);
        sectionAmp .setFont(juce::Font(10.0f, juce::Font::bold));
        sectionFilt.setFont(juce::Font(10.0f, juce::Font::bold));
        addAndMakeVisible(sectionAmp);
        addAndMakeVisible(sectionFilt);
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(juce::Colour(0xff0d1b2a));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 6.0f);

        g.setColour(juce::Colour(0xff1e3a5f));
        g.fillRoundedRectangle(getLocalBounds().removeFromTop(22).toFloat(), 6.0f);

        g.setColour(juce::Colours::lightblue.withAlpha(0.8f));
        g.setFont(juce::Font(12.0f, juce::Font::bold));
        g.drawText("VOICE", getLocalBounds().removeFromTop(22),
                   juce::Justification::centred);

        int divY = 22 + 8 + 84 + 4;
        g.setColour(juce::Colour(0xff1e3a5f));
        g.drawHorizontalLine(divY, 8.0f, (float)getWidth() - 8.0f);
        g.drawVerticalLine(getWidth() / 2, (float)divY + 2, (float)getHeight() - 4.0f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(6);
        area.removeFromTop(22);  // title bar
        area.removeFromTop(8);

        // Top row: Wave | Detune | Dirt | Cutoff | Res | EnvAmt
        auto topRow = area.removeFromTop(84);
        int  colW   = topRow.getWidth() / 6;

        auto waveCol = topRow.removeFromLeft(colW);
        waveLabel.setBounds(waveCol.removeFromTop(14));
        waveformBox.setBounds(waveCol.withSizeKeepingCentre(colW - 8, 22));

        osc2Detune.setBounds(topRow.removeFromLeft(colW));
        dirt.setBounds(topRow.removeFromLeft(colW));
        cutoff.setBounds(topRow.removeFromLeft(colW));
        resonance.setBounds(topRow.removeFromLeft(colW));
        envAmt.setBounds(topRow);

        area.removeFromTop(12);  // gap + divider room

        // Bottom: AMP ADSR (left half) | FILTER ADSR (right half)
        auto ampArea  = area.removeFromLeft(area.getWidth() / 2).reduced(4, 0);
        auto filtArea = area.reduced(4, 0);

        sectionAmp .setBounds(ampArea.removeFromTop(14));
        sectionFilt.setBounds(filtArea.removeFromTop(14));

        int kw = ampArea.getWidth() / 4;
        ampA.setBounds(ampArea.removeFromLeft(kw));
        ampD.setBounds(ampArea.removeFromLeft(kw));
        ampS.setBounds(ampArea.removeFromLeft(kw));
        ampR.setBounds(ampArea);

        kw = filtArea.getWidth() / 4;
        filtA.setBounds(filtArea.removeFromLeft(kw));
        filtD.setBounds(filtArea.removeFromLeft(kw));
        filtS.setBounds(filtArea.removeFromLeft(kw));
        filtR.setBounds(filtArea);
    }

private:
    SynthParameters& params;

    juce::ComboBox waveformBox;
    juce::Label    waveLabel;

    Knob osc2Detune, dirt;
    Knob cutoff, resonance, envAmt;
    Knob ampA, ampD, ampS, ampR;
    Knob filtA, filtD, filtS, filtR;

    juce::Label sectionAmp, sectionFilt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SynthBaseEditor)
};

} // namespace

// ---- SynthBaseModule -------------------------------------------------------

SynthBaseModule::SynthBaseModule(SynthParameters& p) : params(p) {}

std::unique_ptr<juce::Component> SynthBaseModule::createEditor()
{
    return std::make_unique<SynthBaseEditor>(params);
}

void SynthBaseModule::saveState(juce::XmlElement& xml) const
{
    xml.setAttribute("waveform",     params.waveform.load());
    xml.setAttribute("detune",       params.detune.load());
    xml.setAttribute("dirt",         params.dirt.load());
    xml.setAttribute("filterCutoff", params.filterCutoff.load());
    xml.setAttribute("filterRes",    params.filterRes.load());
    xml.setAttribute("filterEnvAmt", params.filterEnvAmt.load());
    xml.setAttribute("ampAttack",    params.ampAttack.load());
    xml.setAttribute("ampDecay",     params.ampDecay.load());
    xml.setAttribute("ampSustain",   params.ampSustain.load());
    xml.setAttribute("ampRelease",   params.ampRelease.load());
    xml.setAttribute("filtAttack",   params.filtAttack.load());
    xml.setAttribute("filtDecay",    params.filtDecay.load());
    xml.setAttribute("filtSustain",  params.filtSustain.load());
    xml.setAttribute("filtRelease",  params.filtRelease.load());
}

void SynthBaseModule::loadState(const juce::XmlElement& xml)
{
    params.waveform.store    (xml.getIntAttribute   ("waveform",     params.waveform.load()));
    params.detune.store      ((float)xml.getDoubleAttribute("detune",       params.detune.load()));
    params.dirt.store        ((float)xml.getDoubleAttribute("dirt",         params.dirt.load()));
    params.filterCutoff.store((float)xml.getDoubleAttribute("filterCutoff", params.filterCutoff.load()));
    params.filterRes.store   ((float)xml.getDoubleAttribute("filterRes",    params.filterRes.load()));
    params.filterEnvAmt.store((float)xml.getDoubleAttribute("filterEnvAmt", params.filterEnvAmt.load()));
    params.ampAttack.store   ((float)xml.getDoubleAttribute("ampAttack",    params.ampAttack.load()));
    params.ampDecay.store    ((float)xml.getDoubleAttribute("ampDecay",     params.ampDecay.load()));
    params.ampSustain.store  ((float)xml.getDoubleAttribute("ampSustain",   params.ampSustain.load()));
    params.ampRelease.store  ((float)xml.getDoubleAttribute("ampRelease",   params.ampRelease.load()));
    params.filtAttack.store  ((float)xml.getDoubleAttribute("filtAttack",   params.filtAttack.load()));
    params.filtDecay.store   ((float)xml.getDoubleAttribute("filtDecay",    params.filtDecay.load()));
    params.filtSustain.store ((float)xml.getDoubleAttribute("filtSustain",  params.filtSustain.load()));
    params.filtRelease.store ((float)xml.getDoubleAttribute("filtRelease",  params.filtRelease.load()));
}

// Auto-register with the module registry at startup
REGISTER_MODULE(SynthBaseModule);
