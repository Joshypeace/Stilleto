#pragma once

#include "theme/Animation.h"
#include "theme/Icons.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace stilleto::ui
{

/** Metric tile: icon, label, large value and a caption. The value counts up when it changes. */
class StatCard : public juce::Component
{
public:
    StatCard (Icons::Id icon, juce::String label);

    /** numeric drives the count-up animation; display formats it. Pass an empty caption to hide it. */
    void setValue (double numeric, std::function<juce::String (double)> display, const juce::String& caption);
    void paint (juce::Graphics&) override;

private:
    Icons::Id icon;
    juce::String label, caption;
    std::function<juce::String (double)> format;
    AnimatedValue value { 0.0f, 0.18f };
    FrameDriver frames;
};

} // namespace stilleto::ui
