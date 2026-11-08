#include "components/Buttons.h"

#include "theme/DesignTokens.h"

namespace stilleto::ui
{

//==============================================================================================
StilletoButton::StilletoButton (const juce::String& text, Style s, std::optional<Icons::Id> i)
    : juce::Button (text), style (s), icon (i)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    frames.onFrame = [this] (float dt)
    {
        const bool moving = hover.advance (dt) | press.advance (dt);
        repaint();
        return moving;
    };
}

int StilletoButton::getIdealWidth() const
{
    const auto font = Fonts::bodyStrong();
    return juce::GlyphArrangement::getStringWidthInt (font, getButtonText()) + (icon ? 52 : 36);
}

void StilletoButton::buttonStateChanged()
{
    hover.setTarget (isOver() || isDown() ? 1.0f : 0.0f);
    press.setTarget (isDown() ? 1.0f : 0.0f);
    frames.kick();
}

void StilletoButton::paintButton (juce::Graphics& g, bool, bool)
{
    const auto r = getLocalBounds().toFloat().reduced (1.0f).translated (0.0f, press.get() * 0.8f);
    const float h = hover.get();
    const bool enabled = isEnabled();
    const float radius = Metrics::radiusSmall + 2.0f;

    juce::Colour textColour = Colours::text;

    switch (style)
    {
        case Style::Primary:
        {
            if (! enabled)
            {
                g.setColour (Colours::elevated);
                g.fillRoundedRectangle (r, radius);
                textColour = Colours::textMuted;
                break;
            }

            juce::DropShadow (Colours::gold.withAlpha (0.10f + 0.16f * h), 14, { 0, 4 })
                .drawForRectangle (g, r.toNearestInt().reduced (4, 2));
            g.setGradientFill (goldGradient (r, 1.0f + 0.07f * h - 0.08f * press.get()));
            g.fillRoundedRectangle (r, radius);

            // Metallic sheen along the top edge.
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.38f), r.getX(), r.getY(),
                                                     juce::Colours::white.withAlpha (0.0f), r.getX(), r.getCentreY(), false));
            g.fillRoundedRectangle (r.withHeight (r.getHeight() * 0.5f).reduced (1.0f, 0.5f), radius - 1.0f);
            textColour = juce::Colour (0xff17130b);
            break;
        }

        case Style::Secondary:
            g.setColour (Colours::elevated.brighter (0.06f * h));
            g.fillRoundedRectangle (r, radius);
            g.setColour (Colours::border.interpolatedWith (Colours::silver.withAlpha (0.45f), h));
            g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
            textColour = enabled ? Colours::text : Colours::textMuted;
            break;

        case Style::Ghost:
            g.setColour (Colours::text.withAlpha (0.06f * h));
            g.fillRoundedRectangle (r, radius);
            textColour = enabled ? Colours::textSecondary.interpolatedWith (Colours::text, h) : Colours::textMuted;
            break;

        case Style::Danger:
            g.setColour (Colours::error.withAlpha (0.08f + 0.10f * h));
            g.fillRoundedRectangle (r, radius);
            g.setColour (Colours::error.withAlpha (0.35f + 0.3f * h));
            g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
            textColour = enabled ? Colours::error.brighter (0.2f) : Colours::textMuted;
            break;
    }

    if (hasKeyboardFocus (false))
    {
        g.setColour (Colours::gold.withAlpha (0.8f));
        g.drawRoundedRectangle (r.expanded (2.0f), radius + 2.0f, 1.2f);
    }

    const auto font = Fonts::bodyStrong();
    const int textWidth = juce::GlyphArrangement::getStringWidthInt (font, getButtonText());
    const float iconSize = 16.0f;
    const float contentWidth = textWidth + (icon ? iconSize + 8.0f : 0.0f);
    float x = r.getCentreX() - contentWidth * 0.5f;

    if (icon)
    {
        Icons::draw (g, *icon, { x, r.getCentreY() - iconSize * 0.5f, iconSize, iconSize }, textColour, 1.8f);
        x += iconSize + 8.0f;
    }

    g.setColour (textColour);
    g.setFont (font);
    g.drawText (getButtonText(), juce::Rectangle<float> (x, r.getY(), (float) textWidth + 2.0f, r.getHeight()),
                juce::Justification::centredLeft, false);
}

//==============================================================================================
IconButton::IconButton (Icons::Id i, const juce::String& tooltip, bool d)
    : juce::Button (tooltip), icon (i), danger (d)
{
    setTooltip (tooltip);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    frames.onFrame = [this] (float dt) { const bool m = hover.advance (dt); repaint(); return m; };
}

void IconButton::buttonStateChanged()
{
    hover.setTarget (isOver() || isDown() ? 1.0f : 0.0f);
    frames.kick();
}

void IconButton::paintButton (juce::Graphics& g, bool, bool down)
{
    const auto r = getLocalBounds().toFloat().reduced (1.0f);
    const float h = hover.get();
    const auto hoverColour = danger ? Colours::error : (accent ? *accent : Colours::text);

    g.setColour (hoverColour.withAlpha ((danger ? 0.12f : 0.07f) * h + (down ? 0.04f : 0.0f)));
    g.fillRoundedRectangle (r, Metrics::radiusSmall);

    const auto base = accent ? *accent : Colours::textSecondary;
    const auto colour = ! isEnabled() ? Colours::textMuted.withAlpha (0.5f)
                                      : base.interpolatedWith (danger ? Colours::error : (accent ? accent->brighter (0.3f) : Colours::text), h);
    const float size = juce::jmin (18.0f, r.getHeight() * 0.56f);
    Icons::draw (g, icon, r.withSizeKeepingCentre (size, size), colour, 1.7f);
}

//==============================================================================================
ToggleSwitch::ToggleSwitch() : juce::Button ("switch")
{
    setClickingTogglesState (true);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    frames.onFrame = [this] (float dt)
    {
        const bool m = position.advance (dt) | hover.advance (dt);
        repaint();
        return m;
    };
    onStateChange = [this]
    {
        position.setTarget (getToggleState() ? 1.0f : 0.0f);
        frames.kick();
    };
}

void ToggleSwitch::clicked()
{
    position.setTarget (getToggleState() ? 1.0f : 0.0f);
    frames.kick();
}

void ToggleSwitch::buttonStateChanged()
{
    hover.setTarget (isOver() ? 1.0f : 0.0f);
    if (std::abs (position.getTarget() - (getToggleState() ? 1.0f : 0.0f)) > 0.01f)
        position.setTarget (getToggleState() ? 1.0f : 0.0f);
    frames.kick();
}

void ToggleSwitch::paintButton (juce::Graphics& g, bool, bool)
{
    const auto r = getLocalBounds().toFloat().withSizeKeepingCentre (40.0f, 22.0f);
    const float p = position.get();
    const bool enabled = isEnabled();

    g.setColour (Colours::background2.interpolatedWith (Colours::goldDeep, p).withAlpha (enabled ? 1.0f : 0.4f));
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);

    if (p > 0.01f)
    {
        g.setGradientFill (goldGradient (r));
        g.setOpacity (p * (enabled ? 1.0f : 0.4f));
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        g.setOpacity (1.0f);
    }

    g.setColour (Colours::border.interpolatedWith (Colours::silver.withAlpha (0.5f), hover.get() * (1.0f - p)));
    g.drawRoundedRectangle (r.reduced (0.5f), r.getHeight() * 0.5f, 1.0f);

    const float knob = r.getHeight() - 6.0f;
    const float x = r.getX() + 3.0f + (r.getWidth() - knob - 6.0f) * p;
    const auto k = juce::Rectangle<float> (x, r.getY() + 3.0f, knob, knob);
    g.setColour (juce::Colours::black.withAlpha (0.3f));
    g.fillEllipse (k.translated (0, 1.0f));
    g.setColour (enabled ? Colours::text.interpolatedWith (juce::Colour (0xfffffaf0), p) : Colours::textMuted);
    g.fillEllipse (k);

    if (hasKeyboardFocus (false))
    {
        g.setColour (Colours::gold.withAlpha (0.8f));
        g.drawRoundedRectangle (r.expanded (2.5f), r.getHeight() * 0.5f + 2.5f, 1.2f);
    }
}

//==============================================================================================
SegmentedControl::SegmentedControl (juce::StringArray o) : options (std::move (o))
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    frames.onFrame = [this] (float dt) { const bool m = indicator.advance (dt); repaint(); return m; };
}

void SegmentedControl::setSelectedIndex (int index, bool notify)
{
    index = juce::jlimit (0, juce::jmax (0, options.size() - 1), index);
    if (index == selected && ! notify)
    {
        indicator.snapTo ((float) index);
        repaint();
        return;
    }

    selected = index;
    indicator.setTarget ((float) index);
    frames.kick();
    if (notify && onChange) onChange (index);
}

juce::Rectangle<float> SegmentedControl::segmentBounds (int index) const
{
    const auto r = getLocalBounds().toFloat().reduced (3.0f);
    const float w = r.getWidth() / (float) juce::jmax (1, options.size());
    return { r.getX() + w * index, r.getY(), w, r.getHeight() };
}

void SegmentedControl::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    g.setColour (Colours::background2);
    g.fillRoundedRectangle (r, Metrics::radiusSmall + 2.0f);
    g.setColour (Colours::border);
    g.drawRoundedRectangle (r.reduced (0.5f), Metrics::radiusSmall + 2.0f, 1.0f);

    const auto first = segmentBounds (0);
    const auto pill = first.withX (first.getX() + first.getWidth() * indicator.get());
    g.setColour (Colours::elevated.brighter (0.08f));
    g.fillRoundedRectangle (pill, Metrics::radiusSmall);
    g.setColour (Colours::gold.withAlpha (0.55f));
    g.drawRoundedRectangle (pill.reduced (0.5f), Metrics::radiusSmall, 1.0f);

    g.setFont (Fonts::make (12.5f, "Semibold"));
    for (int i = 0; i < options.size(); ++i)
    {
        g.setColour (i == selected ? Colours::text : (i == hovered ? Colours::textSecondary.brighter (0.2f) : Colours::textSecondary));
        g.drawFittedText (options[i], segmentBounds (i).toNearestInt(), juce::Justification::centred, 1);
    }
}

void SegmentedControl::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < options.size(); ++i)
        if (segmentBounds (i).contains (e.position))
            setSelectedIndex (i, true);
}

void SegmentedControl::mouseMove (const juce::MouseEvent& e)
{
    int h = -1;
    for (int i = 0; i < options.size(); ++i)
        if (segmentBounds (i).contains (e.position)) h = i;
    if (h != hovered) { hovered = h; repaint(); }
}

void SegmentedControl::mouseExit (const juce::MouseEvent&)
{
    hovered = -1;
    repaint();
}

} // namespace stilleto::ui
