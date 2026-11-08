#pragma once

#include "components/Buttons.h"
#include "core/HotkeySpec.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace stilleto::ui
{

/** Shows the current shortcut as keycaps; "Change" captures the next key combination.

    Keys are read from the physical keyboard state (GetAsyncKeyState) so punctuation and function
    keys map to the right virtual-key codes regardless of layout. onCaptureStateChanged lets the
    owner pause the live global hotkey while recording a new one. */
class HotkeyEditor : public juce::Component, private juce::Timer
{
public:
    HotkeyEditor();

    std::function<void (const HotkeySpec&)> onShortcutChosen;
    std::function<void (bool capturing)> onCaptureStateChanged;

    void setShortcut (const HotkeySpec& spec);
    void setMessage (const juce::String& text, bool isError);

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    void focusLost (FocusChangeType) override;

private:
    void beginCapture();
    void endCapture();
    void timerCallback() override;

    HotkeySpec spec;
    bool capturing = false;
    juce::String message;
    bool messageIsError = false;
    float phase = 0.0f;
    StilletoButton changeButton { "Change", StilletoButton::Style::Secondary, Icons::Id::Keyboard };
};

} // namespace stilleto::ui
