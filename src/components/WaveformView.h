#pragma once

#include "theme/Animation.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

namespace stilleto::ui
{

/** Live amplitude bars driven by the recorder's envelope (real microphone input, never random).
    Repaints at 30 fps only while active; when inactive the bars ease down to a quiet baseline. */
class WaveformView : public juce::Component, private juce::Timer
{
public:
    using Source = std::function<void (float* destination, int count)>;

    explicit WaveformView (Source envelopeSource);

    void setActive (bool shouldBeActive);
    void setBarStyle (int barCount, float barWidthRatio = 0.55f) { bars = barCount; widthRatio = barWidthRatio; resized(); }
    void setGold (bool gold) { useGold = gold; repaint(); }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    Source source;
    int bars = 48;
    float widthRatio = 0.55f;
    bool active = false, useGold = true;
    std::vector<float> raw, display;
};

} // namespace stilleto::ui
