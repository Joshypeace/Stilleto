#pragma once

#include "theme/Animation.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace stilleto::ui
{

/** A pill with a coloured status dot and label, e.g. "● Microphone ready". The dot can pulse
    (used while recording or loading). */
class StatusIndicator : public juce::Component, public juce::SettableTooltipClient
{
public:
    enum class Tone { Neutral, Good, Busy, Warning, Bad, Gold };

    StatusIndicator();

    void setStatus (const juce::String& label, Tone tone, bool pulse = false);
    [[nodiscard]] int getIdealWidth() const;

    void paint (juce::Graphics&) override;

    [[nodiscard]] static juce::Colour colourFor (Tone tone);

private:
    juce::String label;
    Tone tone = Tone::Neutral;
    bool pulsing = false;
    float phase = 0.0f;
    AnimatedValue colourMix { 1.0f, 0.12f };
    juce::Colour previousColour, targetColour;
    FrameDriver frames;
};

} // namespace stilleto::ui
