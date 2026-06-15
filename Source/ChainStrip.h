#pragma once
#include <JuceHeader.h>

class ModuleWindow;

// A compact vertical list of module slots that shows signal-chain order.
// Each row has a drag handle, numbered name, and a visibility dot.
// Click a row to show/raise that module's window.
// Drag a row to reorder the effects chain.
class ChainStrip : public juce::Component, private juce::Timer
{
public:
    ChainStrip() { startTimerHz(5); }  // low-rate repaint keeps visibility dots fresh

    std::function<void(int from, int to)>        onReorder;
    std::function<void(int index)>               onShow;
    std::function<void(int index, bool enabled)> onToggle;

    void setModules(juce::StringArray names, std::vector<ModuleWindow*> windows,
                    std::vector<bool> enabledStates = {});
    int  getTotalHeight() const;

    void paint    (juce::Graphics&)          override;
    void mouseDown(const juce::MouseEvent&)  override;
    void mouseDrag(const juce::MouseEvent&)  override;
    void mouseUp  (const juce::MouseEvent&)  override;

private:
    static constexpr int rowH = 28;
    static constexpr int gap  = 3;

    struct Row { juce::String name; ModuleWindow* window = nullptr; bool enabled = true; };
    std::vector<Row> rows;

    int  mouseDownIndex = -1;
    int  mouseDownY     = 0;
    int  dragY          = 0;
    int  insertIndex    = -1;
    bool isDragging     = false;

    int rowTop         (int i) const { return i * (rowH + gap); }
    int rowAt          (int y) const;
    int insertionIndex (int y) const;

    void timerCallback() override { repaint(); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChainStrip)
};
