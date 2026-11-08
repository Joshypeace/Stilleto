#include "components/MicOrb.h"

#include "theme/DesignTokens.h"
#include "theme/Icons.h"

#include <numbers>

namespace stilleto::ui
{

MicOrb::MicOrb (std::function<float()> levelSource) : juce::Button ("Microphone"), level (std::move (levelSource))
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTitle ("Start or stop dictation");
}

void MicOrb::setMode (Mode m)
{
    if (m == mode) return;
    mode = m;
    recordMix.setTarget (m == Mode::Recording ? 1.0f : 0.0f);
    setEnabled (m != Mode::Disabled);
    setTooltip (m == Mode::Recording ? "Stop and transcribe" : m == Mode::Processing ? "Transcribing..." : "Start dictation");
    lastTick = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (60);
}

void MicOrb::buttonStateChanged()
{
    hover.setTarget (isOver() && isEnabled() ? 1.0f : 0.0f);
    lastTick = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (60);
}

void MicOrb::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const float dt = (float) juce::jlimit (0.0, 0.1, (now - lastTick) / 1000.0);
    lastTick = now;

    bool moving = hover.advance (dt) | recordMix.advance (dt);

    if (mode == Mode::Recording)
    {
        levelSmoothed.setTarget (level ? level() : 0.0f);
        moving = true;
    }
    else
    {
        levelSmoothed.setTarget (0.0f);
    }
    moving = levelSmoothed.advance (dt) || moving;

    if (! Motion::reduced())
    {
        pulsePhase = std::fmod (pulsePhase + dt * 0.7f, 1.0f);
        spinPhase = std::fmod (spinPhase + dt * 0.8f, 1.0f);
    }

    if (mode == Mode::Processing)
        moving = true;

    repaint();
    if (! moving)
        stopTimer();
}

void MicOrb::paintButton (juce::Graphics& g, bool, bool down)
{
    const auto bounds = getLocalBounds().toFloat();
    const float size = juce::jmin (bounds.getWidth(), bounds.getHeight());
    const auto centre = bounds.getCentre();
    const float rec = recordMix.get();
    const float h = hover.get();
    const float lvl = levelSmoothed.get();

    const float coreRadius = size * 0.30f * (down ? 0.97f : 1.0f);
    const auto core = juce::Rectangle<float> (coreRadius * 2, coreRadius * 2).withCentre (centre);

    // Level-reactive halo rings while recording.
    if (rec > 0.01f)
    {
        const float breathe = Motion::reduced() ? 0.5f : 0.5f + 0.5f * std::sin (pulsePhase * 2.0f * std::numbers::pi_v<float>);
        for (int i = 0; i < 3; ++i)
        {
            const float spread = coreRadius * (1.12f + 0.16f * i + lvl * (0.35f + 0.12f * i) + breathe * 0.04f);
            g.setColour (Colours::gold.withAlpha (rec * (0.20f - 0.055f * i) * (0.55f + lvl)));
            g.drawEllipse (juce::Rectangle<float> (spread * 2, spread * 2).withCentre (centre), 1.4f);
        }

        juce::ColourGradient glow (Colours::gold.withAlpha (0.20f * rec * (0.4f + lvl)), centre.x, centre.y,
                                   Colours::gold.withAlpha (0.0f), centre.x + coreRadius * 1.9f, centre.y, true);
        g.setGradientFill (glow);
        g.fillEllipse (juce::Rectangle<float> (coreRadius * 3.8f, coreRadius * 3.8f).withCentre (centre));
    }

    // Outer brushed-silver ring.
    const auto ring = core.expanded (size * 0.06f);
    g.setGradientFill (juce::ColourGradient (Colours::silver.withAlpha (0.35f + 0.2f * h), ring.getX(), ring.getY(),
                                             Colours::silver.withAlpha (0.05f), ring.getRight(), ring.getBottom(), false));
    g.drawEllipse (ring, 1.2f);

    // Core: charcoal when idle, gold when recording.
    juce::DropShadow (juce::Colours::black.withAlpha (0.45f), 24, { 0, 8 }).drawForPath (g, [&] { juce::Path p; p.addEllipse (core); return p; }());
    g.setGradientFill (juce::ColourGradient (Colours::elevated.brighter (0.10f + 0.05f * h), core.getX(), core.getY(),
                                             Colours::background2, core.getX(), core.getBottom(), false));
    g.fillEllipse (core);

    if (rec > 0.01f)
    {
        g.setGradientFill (goldGradient (core, 1.0f + 0.05f * h));
        g.setOpacity (rec);
        g.fillEllipse (core);
        g.setOpacity (1.0f);
    }

    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.18f), core.getX(), core.getY(),
                                             juce::Colours::white.withAlpha (0.0f), core.getX(), core.getCentreY(), false));
    g.drawEllipse (core.reduced (0.5f), 1.0f);

    // Processing: an orbiting arc segment.
    if (mode == Mode::Processing)
    {
        juce::Path arc;
        const float start = spinPhase * 2.0f * std::numbers::pi_v<float>;
        arc.addCentredArc (centre.x, centre.y, ring.getWidth() * 0.5f, ring.getHeight() * 0.5f, 0.0f, start, start + 1.4f, true);
        g.setColour (Colours::gold);
        g.strokePath (arc, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Glyph: mic when idle, stop square when recording.
    const float glyph = coreRadius * 0.9f;
    const auto glyphArea = juce::Rectangle<float> (glyph, glyph).withCentre (centre);
    if (rec > 0.5f)
    {
        Icons::draw (g, Icons::Id::Stop, glyphArea.reduced (glyph * 0.08f), juce::Colour (0xff1a150c), 2.4f);
    }
    else
    {
        const auto colour = ! isEnabled() ? Colours::textMuted
                                          : Colours::silver.interpolatedWith (Colours::gold, juce::jmax (h, mode == Mode::Processing ? 0.6f : 0.0f));
        Icons::draw (g, Icons::Id::Mic, glyphArea, colour, 1.9f);
    }

    if (hasKeyboardFocus (false))
    {
        g.setColour (Colours::gold.withAlpha (0.8f));
        g.drawEllipse (ring.expanded (4.0f), 1.2f);
    }
}

} // namespace stilleto::ui
