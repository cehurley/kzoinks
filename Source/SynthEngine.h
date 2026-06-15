#pragma once
#include <JuceHeader.h>
#include "SynthParameters.h"
#include "SynthVoice.h"

class SynthEngine : public juce::MPESynthesiser
{
public:
    static constexpr int maxVoices = 16;

    SynthParameters params;

    SynthEngine()
    {
        for (int i = 0; i < maxVoices; ++i)
            addVoice(new SynthVoice(params));

        juce::MPEZoneLayout layout;
        layout.setLowerZone(15);
        setZoneLayout(layout);
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SynthEngine)
};
