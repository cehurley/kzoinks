#pragma once
#include <JuceHeader.h>

class ModuleWindow : public juce::DocumentWindow
{
public:
    ModuleWindow(const juce::String& name,
                 std::unique_ptr<juce::Component> editor,
                 int staggerIndex = 0)
        : juce::DocumentWindow(name,
                               juce::Colour(0xff1a1a2e),
                               juce::DocumentWindow::closeButton)
    {
        setUsingNativeTitleBar(true);
        setResizable(true, false);
        setContentOwned(editor.release(), false);
        centreWithSize(320, 320);
        setTopLeftPosition(getX() + staggerIndex * 28, getY() + staggerIndex * 22);
        setVisible(true);
    }

    // Hide rather than destroy — module stays in the signal chain
    void closeButtonPressed() override { setVisible(false); }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModuleWindow)
};
