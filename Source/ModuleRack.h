#pragma once
#include <JuceHeader.h>

class ModuleRack : public juce::Component
{
public:
    ModuleRack() = default;

    // Fired on the message thread when the user completes a drag.
    // Caller must swap modules[from] and modules[to] to keep signal chain in sync.
    std::function<void(int from, int to)> onReorder;

    void addModule(std::unique_ptr<juce::Component> editor, int preferredHeight);
    void clearModules();
    int  getTotalPreferredHeight() const;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    static constexpr int titleH = 22;
    static constexpr int gap    = 4;

    struct ModuleWrapper;

    // Transparent overlay covering the title bar — the only draggable surface.
    // Sits in front of the editor so sliders/knobs below it still get events.
    struct DragZone : public juce::Component
    {
        ModuleRack&    rack;
        ModuleWrapper* wrapper;

        DragZone(ModuleRack& r, ModuleWrapper* w) : rack(r), wrapper(w) {}

        void paint(juce::Graphics& g) override
        {
            // Subtle 2×3 grip dots on the left edge of the title bar
            g.setColour(juce::Colours::white.withAlpha(0.15f));
            for (int row = 0; row < 2; ++row)
                for (int col = 0; col < 3; ++col)
                    g.fillEllipse(6.0f + col * 5.0f, 7.0f + row * 5.0f, 3.0f, 3.0f);
        }

        void mouseDown  (const juce::MouseEvent& e) override;
        void mouseDrag  (const juce::MouseEvent& e) override;
        void mouseUp    (const juce::MouseEvent& e) override;
        void mouseEnter (const juce::MouseEvent& e) override { setMouseCursor(juce::MouseCursor::DraggingHandCursor); juce::ignoreUnused(e); }
        void mouseExit  (const juce::MouseEvent& e) override { setMouseCursor(juce::MouseCursor::NormalCursor);       juce::ignoreUnused(e); }
    };

    struct ModuleWrapper : public juce::Component
    {
        std::unique_ptr<juce::Component> editor;
        std::unique_ptr<DragZone>        dragZone;
        int preferredHeight = 0;

        void resized() override
        {
            editor  ->setBounds(getLocalBounds());
            dragZone->setBounds(0, 0, getWidth(), titleH);
        }
    };

    // ---- drag state --------------------------------------------------------
    void startDrag  (ModuleWrapper* w, juce::Point<int> mouseInRack);
    void updateDrag (juce::Point<int> mouseInRack);
    void endDrag    ();

    int getInsertionIndex (int mouseY)    const;
    int getInsertionLineY (int insertIdx) const;

    std::vector<std::unique_ptr<ModuleWrapper>> wrappers;

    int dragIndex   = -1;
    int insertIndex = -1;
    int dragGrabY   = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModuleRack)
};
