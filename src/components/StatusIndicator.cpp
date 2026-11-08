#include "components/StatusIndicator.h"

#include "theme/DesignTokens.h"

namespace stilleto::ui
{

StatusIndicator::StatusIndicator()
{
    setInterceptsMouseClicks (false, false);
    previousColour = targetColour = colourFor (tone);

    frames.onFrame = [this] (float dt)
    {
        const bool mixing = colourMix.advance (dt);
        if (pulsing && ! Motion::reduced())
            phase = std::fmod (phase + dt * 1.1f, 1.0f);
        repaint();
        return mixing || (pulsing && ! Motion::reduced());
    };
}

juce::Colour StatusIndicator::colourFor (Tone t)
{
    switch (t)
    {
        case Tone::Good:    return Colours::success;
        case Tone::Busy:    return Colours::silver;
        case Tone::Warning: return Colours::warning;
        case Tone::Bad:     return Colours::error;
        case Tone::Gold:    return Colours::gold;
        case Tone::Neutral: break;
    }
    return Colours::textMuted;
}

void StatusIndicator::setStatus (const juce::String& newLabel, Tone newTone, bool pulse)
{
    if (newLabel == label && newTone == tone && pulse == pulsing)
        return;

    if (newTone != tone)
    {
        previousColour = previousColour.interpolatedWith (targetColour, colourMix.get());
        targetColour = colourFor (newTone);
        colourMix.snapTo (0.0f);
        colourMix.setTarget (1.0f);
    }

    label = newLabel;
    tone = newTone;
    pulsing = pulse;
    setTitle (label);
    frames.kick();
    repaint();
}

int StatusIndicator::getIdealWidth() const
{
    return juce::GlyphArrangement::getStringWidthInt (Fonts::small(), label) + 46;
}

void StatusIndicator::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    const auto colour = previousColour.interpolatedWith (targetColour, colourMix.get());

    g.setColour (Colours::background2.withAlpha (0.85f));
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    g.setColour (Colours::border);
    g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);

    const auto dot = juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ r.getX() + 14.0f, r.getCentreY() });

    if (pulsing && ! Motion::reduced())
    {
        const float ring = 7.0f + 9.0f * phase;
        g.setColour (colour.withAlpha (0.45f * (1.0f - phase)));
        g.drawEllipse (juce::Rectangle<float> (ring, ring).withCentre (dot.getCentre()), 1.2f);
    }

    g.setColour (colour);
    g.fillEllipse (dot);

    g.setColour (Colours::textSecondary);
    g.setFont (Fonts::small());
    g.drawText (label, r.withTrimmedLeft (24.0f).withTrimmedRight (10.0f), juce::Justification::centredLeft, true);
}

} // namespace stilleto::ui
