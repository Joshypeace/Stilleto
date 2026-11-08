#include "components/StatCard.h"

#include "theme/DesignTokens.h"

namespace stilleto::ui
{

StatCard::StatCard (Icons::Id i, juce::String l) : icon (i), label (std::move (l))
{
    frames.onFrame = [this] (float dt) { const bool m = value.advance (dt); repaint(); return m; };
}

void StatCard::setValue (double numeric, std::function<juce::String (double)> display, const juce::String& newCaption)
{
    format = std::move (display);
    caption = newCaption;
    value.setTarget (static_cast<float> (numeric));
    frames.kick();
    repaint();
}

void StatCard::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (2.0f);
    paintCard (g, bounds);

    auto r = bounds.reduced ((float) Metrics::cardPadding, 18.0f);
    auto top = r.removeFromTop (20.0f);

    const auto badge = top.removeFromLeft (20.0f);
    Icons::draw (g, icon, badge.withSizeKeepingCentre (17, 17), Colours::gold, 1.6f);
    top.removeFromLeft (10.0f);

    g.setColour (Colours::textSecondary);
    g.setFont (Fonts::small());
    g.drawText (label, top, juce::Justification::centredLeft);

    r.removeFromTop (8.0f);
    const double shown = value.isSettled() ? value.getTarget() : value.get();
    g.setColour (Colours::text);
    g.setFont (Fonts::metric());
    g.drawText (format ? format (shown) : juce::String (juce::roundToInt (shown)), r.removeFromTop (42.0f), juce::Justification::centredLeft);

    if (caption.isNotEmpty())
    {
        g.setColour (Colours::textMuted);
        g.setFont (Fonts::small());
        g.drawText (caption, r.removeFromTop (18.0f), juce::Justification::centredLeft, true);
    }
}

} // namespace stilleto::ui
