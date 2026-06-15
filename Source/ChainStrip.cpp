#include "ChainStrip.h"
#include "ModuleWindow.h"

void ChainStrip::setModules(juce::StringArray names, std::vector<ModuleWindow*> windows,
                            std::vector<bool> enabledStates)
{
    rows.clear();
    for (int i = 0; i < names.size(); ++i)
    {
        bool en = (i < (int)enabledStates.size()) ? enabledStates[(size_t)i] : true;
        rows.push_back({ names[i], windows[(size_t)i], en });
    }
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
    g.drawText("SIGNAL CHAIN", getLocalBounds().removeFromTop(16),
               juce::Justification::centred);

    const int w = getWidth();

    for (int i = 0; i < (int)rows.size(); ++i)
    {
        const bool dragged = isDragging && (i == mouseDownIndex);

        // Dragged row floats at cursor; other rows stay put
        const int y = dragged ? (dragY - (mouseDownY - rowTop(i)))
                               : rowTop(i) + 18;  // +18 for the header label

        auto rowRect = juce::Rectangle<int>(4, y, w - 8, rowH);

        const bool rowEnabled = rows[(size_t)i].enabled;
        const float contentAlpha = rowEnabled ? 1.0f : 0.38f;

        g.setColour((dragged ? juce::Colour(0xff2e2e60)
                              : juce::Colour(0xff1a1a38)).withAlpha(contentAlpha));
        g.fillRoundedRectangle(rowRect.toFloat(), 4.0f);

        // Grip dots
        g.setColour(juce::Colours::white.withAlpha(0.18f * contentAlpha));
        for (int d = 0; d < 3; ++d)
            g.fillEllipse((float)(rowRect.getX() + 7),
                          (float)(rowRect.getY() + 7 + d * 5),
                          3.0f, 3.0f);

        // Power button — filled circle = on, outline only = off
        const float btnCx = (float)(rowRect.getX() + 20);
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

        // Number + name
        g.setColour(juce::Colours::white.withAlpha((dragged ? 1.0f : 0.82f) * contentAlpha));
        g.setFont(12.5f);
        g.drawText(juce::String(i + 1) + ".  " + rows[(size_t)i].name,
                   rowRect.withLeft(rowRect.getX() + 32).withRight(rowRect.getRight() - 20),
                   juce::Justification::centredLeft);

        // Visibility dot
        const bool visible = rows[(size_t)i].window
                             && rows[(size_t)i].window->isVisible();
        g.setColour((visible ? juce::Colour(0xff00aaff)
                              : juce::Colour(0xff333355)).withAlpha(contentAlpha));
        g.fillEllipse((float)(rowRect.getRight() - 15),
                      (float)(rowRect.getCentreY() - 4),
                      8.0f, 8.0f);
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
    juce::ignoreUnused(e);
    if (mouseDownIndex < 0) return;

    if (!isDragging)
    {
        // Power button zone: small circle at a fixed x offset in the row
        const int rowY   = rowTop(mouseDownIndex) + 18;
        const int btnCx  = 4 + 20;          // matches paint layout
        const int btnCy  = rowY + rowH / 2;
        const bool hitPower = (std::abs(e.x - btnCx) <= 8 &&
                               std::abs(e.y - btnCy) <= 8);

        if (hitPower)
        {
            rows[(size_t)mouseDownIndex].enabled = !rows[(size_t)mouseDownIndex].enabled;
            if (onToggle) onToggle(mouseDownIndex, rows[(size_t)mouseDownIndex].enabled);
            repaint();
        }
        else
        {
            if (onShow) onShow(mouseDownIndex);
        }
    }
    else if (insertIndex >= 0)
    {
        const int finalIndex = (insertIndex > mouseDownIndex)
                               ? insertIndex - 1
                               : insertIndex;

        if (finalIndex != mouseDownIndex)
        {
            if (onReorder) onReorder(mouseDownIndex, finalIndex);

            // Keep local rows in sync
            auto row = rows[(size_t)mouseDownIndex];
            rows.erase(rows.begin() + mouseDownIndex);
            rows.insert(rows.begin() + finalIndex, row);
        }
    }

    mouseDownIndex = -1;
    insertIndex    = -1;
    isDragging     = false;
    repaint();
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
