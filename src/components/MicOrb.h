#pragma once

#include "theme/Animation.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace stilleto::ui
{

/** The large microphone control on the Dictation page.

    Idle: brushed-silver ring around a mic. Recording: champagne-gold core whose halo follows the
    live input level and a gentle breathing pulse. Processing: a restrained orbiting arc. */
class MicOrb : public juce::Button, private juce::Timer
{
public:
    enum class Mode { Idle, Recording, Processing, Disabled };

    explicit MicOrb (std::function<float()> levelSource);

    void setMode (Mode m);
    [[nodiscard]] Mode getMode() const noexcept { return mode; }

private:
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    void buttonStateChanged() override;
    void timerCallback() override;

    std::function<float()> level;
    Mode mode = Mode::Idle;
    AnimatedValue hover { 0.0f, 0.1f }, recordMix { 0.0f, 0.18f }, levelSmoothed { 0.0f, 0.06f };
    float pulsePhase = 0.0f, spinPhase = 0.0f;
    double lastTick = 0.0;
};

} // namespace stilleto::ui
