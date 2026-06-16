#include "ModuleRack.h"

// ---- public API ------------------------------------------------------------

void ModuleRack::addModule(std::unique_ptr<juce::Component> editor, int h)
{
    auto wrapper            = std::make_unique<ModuleWrapper>();
    wrapper->preferredHeight = h;
    wrapper->editor          = std::move(editor);
    wrapper->addAndMakeVisible(*wrapper->editor);
    wrapper->dragZone        = std::make_unique<DragZone>(*this, wrapper.get());
    wrapper->addAndMakeVisible(*wrapper->dragZone);   // added last → highest z-order
    addAndMakeVisible(*wrapper);
    wrappers.push_back(std::move(wrapper));
}

void ModuleRack::clearModules()
{
    for (auto& w : wrappers)
        removeChildComponent(w.get());
    wrappers.clear();
    dragIndex = insertIndex = -1;
}

int ModuleRack::getTotalPreferredHeight() const
{
    int h = 0;
    for (auto& w : wrappers) h += w->preferredHeight + gap;
    return wrappers.empty() ? 0 : h - gap;
}

// ---- paint & layout --------------------------------------------------------

void ModuleRack::paint(juce::Graphics& g)
{
    if (wrappers.empty())
    {
        g.setColour(juce::Colour(0xff0f3460));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 6.0f);
        g.setColour(juce::Colour(0xff556688));
        g.setFont(13.0f);
        g.drawText("No modules loaded  \xe2\x80\x94  drop a folder into /modules/ to install",
                   getLocalBounds(), juce::Justification::centred);
        return;
    }

    if (dragIndex >= 0)
    {
        const int lineY = getInsertionLineY(insertIndex);
        g.setColour(juce::Colour(0xff00aaff));
        g.fillRoundedRectangle(4.0f, (float)(lineY - 1),
                               (float)(getWidth() - 8), 3.0f, 1.5f);
    }
}

void ModuleRack::resized()
{
    int y = 0;
    for (int i = 0; i < (int)wrappers.size(); ++i)
    {
        auto& w = wrappers[(size_t)i];
        if (i == dragIndex)
            continue;   // dragged wrapper floats freely; skip it so others close the gap
        w->setBounds(0, y, getWidth(), w->preferredHeight);
        y += w->preferredHeight + gap;
    }
}

// ---- drag ------------------------------------------------------------------

void ModuleRack::startDrag(ModuleWrapper* w, juce::Point<int> mouseInRack)
{
    for (int i = 0; i < (int)wrappers.size(); ++i)
    {
        if (wrappers[(size_t)i].get() != w) continue;
        dragIndex   = i;
        dragGrabY   = mouseInRack.y - w->getY();
        insertIndex = i;
        w->toFront(false);
        w->setAlpha(0.72f);
        repaint();
        return;
    }
}

void ModuleRack::updateDrag(juce::Point<int> mouseInRack)
{
    if (dragIndex < 0) return;

    auto* dragged = wrappers[(size_t)dragIndex].get();
    const int newY = juce::jlimit(0,
                                  juce::jmax(0, getHeight() - dragged->preferredHeight),
                                  mouseInRack.y - dragGrabY);
    dragged->setTopLeftPosition(0, newY);

    insertIndex = getInsertionIndex(mouseInRack.y);
    repaint();
}

void ModuleRack::endDrag()
{
    if (dragIndex < 0) return;

    // Map the visual insert slot to the final index in the wrappers array after
    // the dragged element is removed and re-inserted.
    const int finalIndex = (insertIndex > dragIndex) ? insertIndex - 1 : insertIndex;

    wrappers[(size_t)dragIndex]->setAlpha(1.0f);

    if (finalIndex != dragIndex)
    {
        if (onReorder) onReorder(dragIndex, finalIndex);

        auto dragged = std::move(wrappers[(size_t)dragIndex]);
        wrappers.erase(wrappers.begin() + dragIndex);
        wrappers.insert(wrappers.begin() + finalIndex, std::move(dragged));
    }

    dragIndex = insertIndex = -1;
    resized();
    repaint();
}

int ModuleRack::getInsertionIndex(int mouseY) const
{
    // Return the first wrapper whose centre is below mouseY (skipping the dragged one).
    // Wrapper bounds are already set by resized(), so getY()/getHeight() are authoritative.
    for (int i = 0; i < (int)wrappers.size(); ++i)
    {
        if (i == dragIndex) continue;
        const auto* w = wrappers[(size_t)i].get();
        if (mouseY < w->getY() + w->getHeight() / 2)
            return i;
    }
    return (int)wrappers.size();   // after last slot
}

int ModuleRack::getInsertionLineY(int insertIdx) const
{
    // Find the Y of the top edge of the wrapper at insertIdx (skipping dragged).
    for (int i = insertIdx; i < (int)wrappers.size(); ++i)
    {
        if (i == dragIndex) continue;
        return wrappers[(size_t)i]->getY();
    }
    // After all wrappers: bottom of the last non-dragged wrapper + small margin
    for (int i = (int)wrappers.size() - 1; i >= 0; --i)
    {
        if (i == dragIndex) continue;
        return wrappers[(size_t)i]->getBottom() + gap / 2;
    }
    return 0;
}

// ---- DragZone --------------------------------------------------------------

void ModuleRack::DragZone::mouseDown(const juce::MouseEvent& e)
{
    rack.startDrag(wrapper, e.getEventRelativeTo(&rack).getPosition());
}

void ModuleRack::DragZone::mouseDrag(const juce::MouseEvent& e)
{
    rack.updateDrag(e.getEventRelativeTo(&rack).getPosition());
}

void ModuleRack::DragZone::mouseUp(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);
    rack.endDrag();
}
