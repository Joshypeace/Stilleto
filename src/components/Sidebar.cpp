#include "components/Sidebar.h"

#include "components/Card.h"
#include "theme/DesignTokens.h"

namespace stilleto::ui
{
namespace
{
constexpr float itemHeight = 42.0f;
constexpr float itemsTop = 104.0f;
}

Sidebar::Sidebar (std::vector<Item> i) : items (std::move (i))
{
    setWantsKeyboardFocus (true);
    setTitle ("Navigation");
    for (int k = 0; k < (int) items.size(); ++k)
        hoverAmounts.add (0.0f);

    indicatorY.snapTo (itemBounds (0).getY());

    frames.onFrame = [this] (float dt)
    {
        bool moving = indicatorY.advance (dt);

        for (int k = 0; k < hoverAmounts.size(); ++k)
        {
            const float target = (k == hovered) ? 1.0f : 0.0f;
            const float cur = hoverAmounts[k];
            const float next = Motion::reduced() ? target : cur + (target - cur) * juce::jmin (1.0f, dt * 14.0f);
            hoverAmounts.set (k, next);
            moving = moving || std::abs (next - target) > 0.002f;
        }

        if (shimmer < 1.0f)
        {
            shimmer = juce::jmin (1.0f, shimmer + dt * 0.9f);
            moving = true;
        }

        repaint();
        return moving;
    };
}

juce::Rectangle<float> Sidebar::itemBounds (int index) const
{
    return { 14.0f, itemsTop + index * (itemHeight + 4.0f), getWidth() - 28.0f, itemHeight };
}

int Sidebar::itemAt (juce::Point<float> p) const
{
    for (int k = 0; k < (int) items.size(); ++k)
        if (itemBounds (k).contains (p))
            return k;
    return -1;
}

void Sidebar::setSelected (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) items.size()))
        return;
    selected = index;
    indicatorY.setTarget (itemBounds (index).getY());
    frames.kick();
    repaint();
}

void Sidebar::setShortcutText (const juce::String& text, bool paused)
{
    shortcut = text;
    shortcutPaused = paused;
    repaint();
}

void Sidebar::playIntro()
{
    if (Motion::reduced()) return;
    shimmer = 0.0f;
    frames.kick();
}

void Sidebar::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();

    g.setGradientFill (juce::ColourGradient (Colours::background2, 0, 0, Colours::background, 0, r.getBottom(), false));
    g.fillAll();
    g.setColour (Colours::border.withAlpha (0.8f));
    g.fillRect (r.getRight() - 1.0f, 0.0f, 1.0f, r.getHeight());

    // Brand
    const auto mark = juce::Rectangle<float> (22.0f, 26.0f, 38.0f, 38.0f);
    Icons::drawBrandMark (g, mark, shimmer);
    g.setColour (Colours::text);
    g.setFont (Fonts::make (19.0f, "Semibold"));
    g.drawText ("Stilleto", juce::Rectangle<float> (72.0f, 26.0f, r.getWidth() - 80.0f, 22.0f), juce::Justification::centredLeft);
    g.setColour (Colours::textMuted);
    g.setFont (Fonts::make (11.5f));
    g.drawText ("Your voice. Anywhere you type.", juce::Rectangle<float> (72.0f, 47.0f, r.getWidth() - 80.0f, 16.0f),
                juce::Justification::centredLeft, true);

    drawSectionLabel (g, "Workspace", juce::Rectangle<int> (26, (int) itemsTop - 24, 200, 16));

    // Sliding selection indicator.
    const auto sel = itemBounds (0).withY (indicatorY.get());
    g.setGradientFill (juce::ColourGradient (Colours::gold.withAlpha (0.13f), sel.getX(), sel.getY(),
                                             Colours::gold.withAlpha (0.02f), sel.getRight(), sel.getY(), false));
    g.fillRoundedRectangle (sel, Metrics::radiusSmall + 2.0f);
    g.setGradientFill (goldGradient (sel.withWidth (3.0f)));
    g.fillRoundedRectangle (sel.withWidth (3.0f).reduced (0.0f, 10.0f), 1.5f);

    for (int k = 0; k < (int) items.size(); ++k)
    {
        const auto b = itemBounds (k);
        const float h = hoverAmounts[k];

        if (k != selected && h > 0.01f)
        {
            g.setColour (Colours::text.withAlpha (0.045f * h));
            g.fillRoundedRectangle (b, Metrics::radiusSmall + 2.0f);
        }

        // Selection proximity drives a smooth colour interpolation as the indicator slides.
        const float proximity = juce::jlimit (0.0f, 1.0f, 1.0f - std::abs (indicatorY.get() - b.getY()) / (itemHeight + 4.0f));
        const auto iconColour = Colours::textSecondary.interpolatedWith (Colours::text, h * 0.6f).interpolatedWith (Colours::gold, proximity);
        const auto textColour = Colours::textSecondary.interpolatedWith (Colours::text, juce::jmax (h * 0.7f, proximity));

        Icons::draw (g, items[(size_t) k].icon, juce::Rectangle<float> (b.getX() + 14.0f, b.getCentreY() - 9.0f, 18.0f, 18.0f), iconColour, 1.6f);
        g.setColour (textColour);
        g.setFont (proximity > 0.5f ? Fonts::bodyStrong() : Fonts::body());
        g.drawText (items[(size_t) k].label, b.withTrimmedLeft (44.0f), juce::Justification::centredLeft);

        if (hasKeyboardFocus (false) && k == selected)
        {
            g.setColour (Colours::gold.withAlpha (0.6f));
            g.drawRoundedRectangle (b.expanded (1.0f), Metrics::radiusSmall + 3.0f, 1.0f);
        }
    }

    // Footer: shortcut and offline promise.
    auto footer = juce::Rectangle<float> (14.0f, r.getBottom() - 132.0f, r.getWidth() - 28.0f, 116.0f);
    g.setColour (Colours::surface);
    g.fillRoundedRectangle (footer, Metrics::radiusMedium);
    g.setGradientFill (silverEdge (footer));
    g.drawRoundedRectangle (footer.reduced (0.5f), Metrics::radiusMedium, 1.0f);

    auto inner = footer.reduced (14.0f, 12.0f);
    drawSectionLabel (g, shortcutPaused ? "Shortcut (paused)" : "Dictation shortcut", inner.removeFromTop (16.0f).toNearestInt());
    inner.removeFromTop (8.0f);
    const auto caps = inner.removeFromTop (26.0f);
    drawKeycaps (g, shortcut, caps.getPosition(), 24.0f);
    inner.removeFromTop (12.0f);

    const auto line = inner.removeFromTop (18.0f);
    Icons::draw (g, Icons::Id::Shield, line.withWidth (16.0f).withSizeKeepingCentre (15, 15), Colours::success, 1.5f);
    g.setColour (Colours::textSecondary);
    g.setFont (Fonts::small());
    g.drawText ("Offline - audio stays on this PC", line.withTrimmedLeft (24.0f), juce::Justification::centredLeft, true);
}

void Sidebar::mouseMove (const juce::MouseEvent& e)
{
    const int h = itemAt (e.position);
    if (h != hovered)
    {
        hovered = h;
        setMouseCursor (h >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
        frames.kick();
    }
}

void Sidebar::mouseExit (const juce::MouseEvent&)
{
    hovered = -1;
    frames.kick();
}

void Sidebar::mouseDown (const juce::MouseEvent& e)
{
    const int k = itemAt (e.position);
    if (k >= 0 && onSelect)
        onSelect (k);
}

bool Sidebar::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::upKey || key == juce::KeyPress::downKey)
    {
        const int next = juce::jlimit (0, (int) items.size() - 1, selected + (key == juce::KeyPress::downKey ? 1 : -1));
        if (next != selected && onSelect) onSelect (next);
        return true;
    }
    return false;
}

} // namespace stilleto::ui
