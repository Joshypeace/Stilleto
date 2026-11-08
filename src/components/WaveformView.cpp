#include "components/WaveformView.h"

#include "theme/DesignTokens.h"

#include <cmath>

namespace stilleto::ui
{

WaveformView::WaveformView (Source s) : source (std::move (s))
{
    setInterceptsMouseClicks (false, false);
    display.assign (static_cast<size_t> (bars), 0.0f);
}

void WaveformView::resized()
{
    display.resize (static_cast<size_t> (bars), 0.0f);
    raw.resize (static_cast<size_t> (bars), 0.0f);
}

void WaveformView::setActive (bool shouldBeActive)
{
    active = shouldBeActive;
    if (! isTimerRunning())
        startTimerHz (30);
}

void WaveformView::timerCallback()
{
    if (active && source)
        source (raw.data(), bars);
    else
        std::fill (raw.begin(), raw.end(), 0.0f);

    bool moving = false;
    for (size_t i = 0; i < display.size(); ++i)
    {
        // Perceptual scaling: speech RMS is small, so map through a soft log curve.
        const float db = raw[i] > 0.001f ? 20.0f * std::log10 (raw[i]) : -60.0f;
        const float target = juce::jlimit (0.0f, 1.0f, (db + 56.0f) / 46.0f);
        const float k = Motion::reduced() ? 1.0f : (target > display[i] ? 0.6f : 0.25f);
        display[i] += (target - display[i]) * k;
        moving = moving || std::abs (display[i] - target) > 0.002f || target > 0.0f;
    }

    repaint();

    if (! active && ! moving)
        stopTimer();
}

void WaveformView::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    const float slot = r.getWidth() / (float) juce::jmax (1, bars);
    const float barWidth = juce::jmax (1.5f, slot * widthRatio);
    const float midY = r.getCentreY();
    const float maxHalf = r.getHeight() * 0.5f;

    for (int i = 0; i < bars; ++i)
    {
        const float v = display[static_cast<size_t> (i)];
        // Fade the oldest bars at the left edge so the waveform appears to flow in.
        const float age = (float) i / (float) juce::jmax (1, bars - 1);
        const float half = juce::jmax (1.5f, v * maxHalf);
        const auto bar = juce::Rectangle<float> (r.getX() + i * slot + (slot - barWidth) * 0.5f, midY - half, barWidth, half * 2.0f);

        const float alpha = 0.25f + 0.75f * age;
        if (useGold && v > 0.02f)
        {
            g.setGradientFill (juce::ColourGradient (Colours::goldLight.withAlpha (alpha), bar.getX(), bar.getY(),
                                                     Colours::goldDeep.withAlpha (alpha), bar.getX(), bar.getBottom(), false));
        }
        else
        {
            g.setColour (Colours::silver.withAlpha (0.16f + 0.3f * v * alpha));
        }

        g.fillRoundedRectangle (bar, barWidth * 0.5f);
    }
}

} // namespace stilleto::ui
