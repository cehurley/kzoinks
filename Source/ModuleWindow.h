#pragma once
#include <JuceHeader.h>

class ModuleWindow : public juce::DocumentWindow
{
public:
    ModuleWindow(const juce::String& name,
                 std::unique_ptr<juce::Component> editor,
                 int staggerIndex = 0)
        : juce::DocumentWindow("",
                               juce::Colour(0xff111118),
                               juce::DocumentWindow::closeButton)
    {
        juce::ignoreUnused(name);
        setLookAndFeel(&titleLF);
        setUsingNativeTitleBar(false);
        setTitleBarHeight(28);
        setColour(juce::DocumentWindow::textColourId,        juce::Colour(0xffaaaacc));
        setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(0xff111118));
        setResizable(true, false);
        setContentOwned(editor.release(), false);
        centreWithSize(320, 320);
        setTopLeftPosition(getX() + staggerIndex * 28, getY() + staggerIndex * 22);
        setVisible(true);
    }

    ~ModuleWindow() override { setLookAndFeel(nullptr); }

    void setTitleLogo(juce::Image img)
    {
        titleLF.logo = img;
        repaint();
    }

    void closeButtonPressed() override { setVisible(false); }

private:
    struct TitleLookAndFeel : public juce::LookAndFeel_V4
    {
        juce::Image logo;

        void drawDocumentWindowTitleBar(juce::DocumentWindow&, juce::Graphics& g,
                                        int w, int h,
                                        int titleSpaceX, int titleSpaceW,
                                        const juce::Image*, bool) override
        {
            g.fillAll(juce::Colour(0xff111118));
            if (logo.isValid())
                g.drawImageWithin(logo,
                                  titleSpaceX, 2, titleSpaceW, h - 4,
                                  juce::RectanglePlacement::centred
                                  | juce::RectanglePlacement::onlyReduceInSize);
            juce::ignoreUnused(w);
        }
    };

    TitleLookAndFeel titleLF;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModuleWindow)
};
