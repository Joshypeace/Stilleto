#pragma once

#include "components/Buttons.h"
#include "components/StatusIndicator.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace stilleto::ui
{

/** Section title, compact status pills and window controls. Also the window's drag handle. */
class TopBar : public juce::Component
{
public:
    TopBar();

    std::function<void()> onSettings, onMinimise, onClose, onToggleMaximise;
    std::function<void (const juce::MouseEvent&)> onDragStart, onDrag;

    void setTitle (const juce::String& title, const juce::String& subtitle);

    StatusIndicator microphone, engine, offline;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    juce::String title, subtitle, previousTitle;
    AnimatedValue titleFade { 1.0f, 0.09f };
    FrameDriver frames;
    IconButton settingsButton { Icons::Id::Settings, "Settings" };
    IconButton minimiseButton { Icons::Id::Minimise, "Minimise" };
    IconButton closeButton { Icons::Id::Close, "Close", true };
};

} // namespace stilleto::ui
