#include "components/HotkeyEditor.h"

#include "components/Card.h"
#include "theme/DesignTokens.h"

#ifndef NOMINMAX
 #define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
 #define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#undef small   // rpcndr.h macro that would clash with Fonts::small()

namespace stilleto::ui
{
namespace
{
bool isModifierKey (int vk)
{
    switch (vk)
    {
        case VK_SHIFT: case VK_CONTROL: case VK_MENU: case VK_LSHIFT: case VK_RSHIFT:
        case VK_LCONTROL: case VK_RCONTROL: case VK_LMENU: case VK_RMENU: case VK_LWIN: case VK_RWIN:
            return true;
        default:
            return false;
    }
}

/** Reads the physical keyboard: held modifiers plus the first non-modifier key that is down. */
std::optional<HotkeySpec> readPhysicalShortcut()
{
    HotkeySpec spec { 0, 0 };
    auto down = [] (int vk) { return (GetAsyncKeyState (vk) & 0x8000) != 0; };

    if (down (VK_CONTROL)) spec.modifiers |= HotkeySpec::modControl;
    if (down (VK_MENU))    spec.modifiers |= HotkeySpec::modAlt;
    if (down (VK_SHIFT))   spec.modifiers |= HotkeySpec::modShift;
    if (down (VK_LWIN) || down (VK_RWIN)) spec.modifiers |= HotkeySpec::modWin;

    for (int vk = 0x08; vk <= 0xFE; ++vk)
    {
        if (isModifierKey (vk) || vk == VK_LBUTTON || vk == VK_RBUTTON || vk == VK_MBUTTON || vk == VK_ESCAPE)
            continue;
        if (down (vk))
        {
            spec.virtualKey = static_cast<std::uint32_t> (vk);
            return spec;
        }
    }
    return std::nullopt;
}
} // namespace

HotkeyEditor::HotkeyEditor()
{
    setWantsKeyboardFocus (true);
    addAndMakeVisible (changeButton);
    changeButton.onClick = [this] { capturing ? endCapture() : beginCapture(); };
}

void HotkeyEditor::setShortcut (const HotkeySpec& s)
{
    spec = s;
    repaint();
}

void HotkeyEditor::setMessage (const juce::String& text, bool isError)
{
    message = text;
    messageIsError = isError;
    repaint();
}

void HotkeyEditor::beginCapture()
{
    capturing = true;
    changeButton.setButtonText ("Cancel");
    setMessage ("Press the new combination, e.g. Ctrl + Alt + D. Esc cancels.", false);
    if (onCaptureStateChanged) onCaptureStateChanged (true);
    grabKeyboardFocus();
    startTimerHz (30);
}

void HotkeyEditor::endCapture()
{
    if (! capturing) return;
    capturing = false;
    changeButton.setButtonText ("Change");
    stopTimer();
    if (onCaptureStateChanged) onCaptureStateChanged (false);
    repaint();
}

void HotkeyEditor::focusLost (FocusChangeType)
{
    if (capturing)
    {
        endCapture();
        setMessage ({}, false);
    }
}

bool HotkeyEditor::keyPressed (const juce::KeyPress& key)
{
    if (! capturing)
        return false;

    if (key == juce::KeyPress::escapeKey)
    {
        endCapture();
        setMessage ({}, false);
        return true;
    }

    const auto chosen = readPhysicalShortcut();
    if (! chosen)
        return true;   // only modifiers so far; keep waiting

    if (! chosen->isUsable())
    {
        setMessage ("Include Ctrl, Alt or Win so the shortcut does not interfere with typing.", true);
        return true;
    }

    endCapture();
    if (onShortcutChosen)
        onShortcutChosen (*chosen);
    return true;
}

void HotkeyEditor::timerCallback()
{
    phase = std::fmod (phase + 0.033f, 1.0f);
    repaint();
}

void HotkeyEditor::resized()
{
    changeButton.setBounds (getLocalBounds().removeFromRight (112).removeFromTop (36));
}

void HotkeyEditor::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto top = r.removeFromTop (36.0f).withTrimmedRight (124.0f);

    const auto field = top;
    g.setColour (Colours::background2);
    g.fillRoundedRectangle (field, Metrics::radiusSmall + 2.0f);
    const float glow = capturing ? 0.45f + 0.35f * std::sin (phase * juce::MathConstants<float>::twoPi) : 0.0f;
    g.setColour (capturing ? Colours::gold.withAlpha (0.5f + glow * 0.4f) : Colours::border);
    g.drawRoundedRectangle (field.reduced (0.5f), Metrics::radiusSmall + 2.0f, 1.0f);

    if (capturing)
    {
        g.setColour (Colours::gold);
        g.setFont (Fonts::body());
        g.drawText ("Listening for keys...", field.reduced (14.0f, 0.0f), juce::Justification::centredLeft);
    }
    else
    {
        drawKeycaps (g, juce::String (spec.toDisplayString()), { field.getX() + 6.0f, field.getY() + 5.0f }, 26.0f);
    }

    if (message.isNotEmpty())
    {
        g.setColour (messageIsError ? Colours::error : Colours::textMuted);
        g.setFont (Fonts::small());
        g.drawFittedText (message, r.withTrimmedTop (6.0f).toNearestInt(), juce::Justification::topLeft, 2);
    }
}

} // namespace stilleto::ui
