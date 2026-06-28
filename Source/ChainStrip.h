#pragma once
#include <JuceHeader.h>

class ModuleWindow;

// A fixed-size column of FX insert slots (Logic-Pro-channel-strip style): each
// slot is either empty ("click to add") or holds one module instance. Click the
// name area to pick/replace the module type from a dropdown; drag the grip to
// reorder slots; click the power dot to bypass; click the visibility dot to
// show/raise that slot's window.
class ChainStrip : public juce::Component, private juce::Timer
{
public:
    ChainStrip() { startTimerHz(5); }  // low-rate repaint keeps visibility dots fresh

    std::function<void(int from, int to)>                      onReorder;
    std::function<void(int index)>                              onShow;
    std::function<void(int index, bool enabled)>                onToggle;
    // newType.isEmpty() means the slot was cleared back to empty.
    std::function<void(int index, const juce::String& newType)> onTypeChange;

    // The list of module type names offered in each slot's dropdown.
    void setCatalog(juce::StringArray names) { catalog = std::move(names); }

    struct SlotData { juce::String typeName; ModuleWindow* window = nullptr; bool enabled = true; };
    void setSlots(std::vector<SlotData> data);
    int  getTotalHeight() const;

    void paint    (juce::Graphics&)          override;
    void mouseDown(const juce::MouseEvent&)  override;
    void mouseDrag(const juce::MouseEvent&)  override;
    void mouseUp  (const juce::MouseEvent&)  override;

private:
    static constexpr int rowH = 28;
    static constexpr int gap  = 3;

    std::vector<SlotData> rows;
    juce::StringArray      catalog;

    int  mouseDownIndex = -1;
    int  mouseDownY     = 0;
    int  dragY          = 0;
    int  insertIndex    = -1;
    bool isDragging     = false;

    int rowTop         (int i) const { return i * (rowH + gap); }
    int rowAt          (int y) const;
    int insertionIndex (int y) const;

    void showTypeMenu(int index);

    void timerCallback() override { repaint(); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChainStrip)
};
