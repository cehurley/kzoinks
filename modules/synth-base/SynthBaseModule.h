#pragma once
#include <JuceHeader.h>
#include "IModule.h"
#include "SynthParameters.h"

class SynthBaseModule : public IModule
{
public:
    explicit SynthBaseModule(SynthParameters& params);

    juce::String getName() const override { return "Voice"; }
    int getPreferredHeight() const override { return 195; }

    std::unique_ptr<juce::Component> createEditor() override;
    void saveState(juce::XmlElement&)        const override;
    void loadState(const juce::XmlElement&)        override;

private:
    SynthParameters& params;
};
