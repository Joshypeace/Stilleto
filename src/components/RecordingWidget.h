#pragma once

#include "AppController.h"
#include "components/Buttons.h"
#include "components/WaveformView.h"
#include "theme/Animation.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace stilleto
{

/** Compact always-on-top overlay shown while dictating into another application.

    It is a separate desktop window with WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, shown with
    SW_SHOWNA, and none of its components take keyboard focus, so appearing, updating, being
    clicked or disappearing never steals focus from the target application. */
class RecordingWidget : public juce::Component,
                        private AppController::Listener,
                        private juce::Timer
{
public:
    explicit RecordingWidget (AppController& controller);
    ~RecordingWidget() override;

    /** Shows/hides with an entrance/exit transition, near the display holding targetWindow. */
    void setShouldBeShown (bool show, void* targetWindow);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

private:
    void sessionChanged() override;
    void timerCallback() override;
    void updateButtons();
    void placeOnScreen (void* targetWindow);
    void applyAnimatedBounds();

    AppController& controller;
    ui::WaveformView waveform;
    ui::IconButton primaryButton { ui::Icons::Id::Stop, "Stop and transcribe" };
    ui::IconButton dismissButton { ui::Icons::Id::Close, "Cancel" };

    ui::AnimatedValue visibility { 0.0f, 0.085f }, stateMix { 1.0f, 0.10f };
    ui::FrameDriver frames;
    DictationState shownState = DictationState::Idle;
    juce::Rectangle<int> restingBounds;
    juce::Point<int> dragOffset;
    bool userPositioned = false, wantsVisible = false;
    float pulse = 0.0f;
};

} // namespace stilleto
