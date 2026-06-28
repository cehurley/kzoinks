#include "ChainStrip.h"
#include "ModuleWindow.h"

void ChainStrip::setSlots(std::vector<SlotData> data)
{
    rows = std::move(data);
    repaint();
}

int ChainStrip::getTotalHeight() const
{
    if (rows.empty()) return 0;
    return (int)rows.size() * (rowH + gap) - gap;
}

void ChainStrip::paint(juce::Graphics& g)
{
    g.setColour(juce::Colour(0xff0d0d1e));
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 6.0f);

    g.setColour(juce::Colour(0xff222244));
    g.setFont(juce::Font(10.0f, juce::Font::bold));
    g.drawText("FX CHAIN", getLocalBounds().removeFromTop(16),
               juce::Justification::centred);

    const int w = getWidth();

    for (int i = 0; i < (int)rows.size(); ++i)
    {
        const bool dragged = isDragging && (i == mouseDownIndex);

        // Dragged row floats at cursor; other rows stay put
        const int y = dragged ? (dragY - (mouseDownY - rowTop(i)))
                               : rowTop(i) + 18;  // +18 for the header label

        auto rowRect = juce::Rectangle<int>(4, y, w - 8, rowH);

        const auto& row        = rows[(size_t)i];
        const bool  filled      = row.typeName.isNotEmpty();
        const bool  rowEnabled  = row.enabled;
        const float contentAlpha = filled ? (rowEnabled ? 1.0f : 0.38f) : 0.55f;

        g.setColour((dragged ? juce::Colour(0xff2e2e60)
                              : juce::Colour(0xff1a1a38)).withAlpha(filled ? contentAlpha : 0.4f));
        g.fillRoundedRectangle(rowRect.toFloat(), 4.0f);

        if (!filled)
        {
            g.setColour(juce::Colours::white.withAlpha(0.14f));
            g.drawRoundedRectangle(rowRect.toFloat().reduced(1.0f), 4.0f, 1.0f);
        }

        // Grip dots
        g.setColour(juce::Colours::white.withAlpha(0.18f * contentAlpha));
        for (int d = 0; d < 3; ++d)
            g.fillEllipse((float)(rowRect.getX() + 7),
                          (float)(rowRect.getY() + 7 + d * 5),
                          3.0f, 3.0f);

        if (filled)
        {
            // Power button — filled circle = on, outline only = off
            const float btnCx = (float)(rowRect.getX() + 34);
            const float btnCy = (float)rowRect.getCentreY();
            const float btnR  = 5.0f;
            if (rowEnabled)
            {
                g.setColour(juce::Colour(0xff00cc66));
                g.fillEllipse(btnCx - btnR, btnCy - btnR, btnR * 2, btnR * 2);
            }
            else
            {
                g.setColour(juce::Colour(0xff556677));
                g.drawEllipse(btnCx - btnR, btnCy - btnR, btnR * 2, btnR * 2, 1.5f);
            }

            // Number + name + dropdown affordance
            g.setColour(juce::Colours::white.withAlpha((dragged ? 1.0f : 0.82f) * contentAlpha));
            g.setFont(12.5f);
            g.drawText(juce::String(i + 1) + ".  " + row.typeName + "  \xe2\x96\xbe",
                       rowRect.withLeft(rowRect.getX() + 48).withRight(rowRect.getRight() - 24),
                       juce::Justification::centredLeft);

            // Visibility dot
            const bool visible = row.window && row.window->isVisible();
            g.setColour((visible ? juce::Colour(0xff00aaff)
                                  : juce::Colour(0xff333355)).withAlpha(contentAlpha));
            g.fillEllipse((float)(rowRect.getRight() - 20),
                          (float)(rowRect.getCentreY() - 4),
                          8.0f, 8.0f);
        }
        else
        {
            g.setColour(juce::Colours::white.withAlpha(0.32f));
            g.setFont(12.0f);
            g.drawText(juce::String(i + 1) + ".  + Add module  \xe2\x96\xbe",
                       rowRect.withLeft(rowRect.getX() + 48),
                       juce::Justification::centredLeft);
        }
    }

    // Insertion line
    if (isDragging && insertIndex >= 0)
    {
        const int lineY = (insertIndex < (int)rows.size())
                          ? rowTop(insertIndex) + 18
                          : rowTop((int)rows.size() - 1) + 18 + rowH + gap / 2;

        g.setColour(juce::Colour(0xff00aaff));
        g.fillRoundedRectangle(6.0f, (float)(lineY - 1),
                               (float)(w - 12), 3.0f, 1.5f);
    }
}

void ChainStrip::mouseDown(const juce::MouseEvent& e)
{
    // Offset y by header height to map into row coordinates
    const int y = e.y - 18;
    mouseDownIndex = rowAt(y);
    mouseDownY     = y;
    dragY          = y;
    isDragging     = false;
    insertIndex    = -1;
}

void ChainStrip::mouseDrag(const juce::MouseEvent& e)
{
    if (mouseDownIndex < 0) return;
    dragY = e.y - 18;

    if (!isDragging && std::abs(dragY - mouseDownY) > 4)
        isDragging = true;

    if (isDragging)
    {
        insertIndex = insertionIndex(dragY);
        repaint();
    }
}

void ChainStrip::mouseUp(const juce::MouseEvent& e)
{
    if (mouseDownIndex < 0) return;

    if (!isDragging)
    {
        // Zones along the row: grip (drag only) | power | name/dropdown | visibility
        const auto& row    = rows[(size_t)mouseDownIndex];
        const bool  filled = row.typeName.isNotEmpty();
        const int   w      = getWidth();

        const int gripEnd  = 4 + 24;
        const int powerEnd = gripEnd + 20;
        const int visStart = w - 4 - 24;

        if (e.x < gripEnd)
        {
            // grip zone — drag only, no click action
        }
        else if (!filled)
        {
            // empty slot — any non-grip click opens the picker
            showTypeMenu(mouseDownIndex);
        }
        else if (e.x < powerEnd)
        {
            const bool newEnabled = !row.enabled;
            rows[(size_t)mouseDownIndex].enabled = newEnabled;
            if (onToggle) onToggle(mouseDownIndex, newEnabled);
        }
        else if (e.x >= visStart)
        {
            if (onShow) onShow(mouseDownIndex);
        }
        else
        {
            showTypeMenu(mouseDownIndex);
        }

        repaint();
    }
    else if (insertIndex >= 0)
    {
        const int finalIndex = (insertIndex > mouseDownIndex)
                               ? insertIndex - 1
                               : insertIndex;

        if (finalIndex != mouseDownIndex)
        {
            // onReorder (MainComponent) swaps the two fxSlots entries and then
            // calls refreshChainStripDisplay(), which calls setSlots() and
            // replaces `rows` wholesale with the true post-swap order — don't
            // also mutate `rows` here, that double-applies a (different,
            // shift-based) reorder on top of the swap and desyncs the display
            // from what actually gets saved.
            if (onReorder) onReorder(mouseDownIndex, finalIndex);
        }
    }

    mouseDownIndex = -1;
    insertIndex    = -1;
    isDragging     = false;
    repaint();
}

void ChainStrip::showTypeMenu(int index)
{
    juce::PopupMenu menu;
    menu.addItem(1, "(Empty)");
    for (int i = 0; i < catalog.size(); ++i)
        menu.addItem(100 + i, catalog[i]);

    auto rowLocal  = juce::Rectangle<int>(4, rowTop(index) + 18, getWidth() - 8, rowH);
    auto rowScreen = localAreaToGlobal(rowLocal);

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(rowScreen),
        [this, index](int result)
        {
            if (result == 0) return; // dismissed without a choice
            const juce::String newType = (result == 1) ? juce::String() : catalog[result - 100];
            if (onTypeChange) onTypeChange(index, newType);
        });
}

int ChainStrip::rowAt(int y) const
{
    if (rows.empty()) return -1;
    return juce::jlimit(0, (int)rows.size() - 1, y / (rowH + gap));
}

int ChainStrip::insertionIndex(int y) const
{
    for (int i = 0; i < (int)rows.size(); ++i)
    {
        if (i == mouseDownIndex) continue;
        if (y < rowTop(i) + rowH / 2) return i;
    }
    return (int)rows.size();
}
