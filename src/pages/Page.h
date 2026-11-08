#pragma once

#include "AppController.h"
#include "components/Buttons.h"
#include "components/Card.h"
#include "theme/DesignTokens.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace stilleto
{

/** A top-level section of the dashboard. Pages observe the controller and render real state. */
class Page : public juce::Component
{
public:
    explicit Page (AppController& c) : controller (c) {}

    [[nodiscard]] virtual juce::String pageTitle() const = 0;
    [[nodiscard]] virtual juce::String pageSubtitle() const { return {}; }

    /** Called when the page becomes / stops being the visible one. */
    virtual void pageShown() {}
    virtual void pageHidden() {}

    /** Set by the main window so pages can link to each other. */
    std::function<void (AppController::Page)> onNavigate;

protected:
    void navigate (AppController::Page p) { if (onNavigate) onNavigate (p); }

    AppController& controller;
};

namespace ui
{
    /** A static text label styled with the design system. */
    inline void styleLabel (juce::Label& label, const juce::Font& font, juce::Colour colour,
                            juce::Justification j = juce::Justification::centredLeft)
    {
        label.setFont (font);
        label.setColour (juce::Label::textColourId, colour);
        label.setJustificationType (j);
        label.setInterceptsMouseClicks (false, false);
        label.setBorderSize ({});
    }

    /** Relative time like "Just now", "5 min ago", "Yesterday 14:02". */
    inline juce::String relativeTime (juce::Time t)
    {
        const auto now = juce::Time::getCurrentTime();
        const auto diff = now - t;
        if (diff.inSeconds() < 60) return "Just now";
        if (diff.inMinutes() < 60) return juce::String ((int) diff.inMinutes()) + " min ago";
        if (diff.inHours() < 24 && now.getDayOfMonth() == t.getDayOfMonth()) return t.formatted ("Today %H:%M");
        if (diff.inDays() < 2) return t.formatted ("Yesterday %H:%M");
        return t.formatted ("%d %b %Y, %H:%M");
    }

    inline juce::String formatDuration (double seconds)
    {
        if (seconds < 60.0) return juce::String (seconds, 1) + " s";
        const int s = (int) seconds;
        return juce::String (s / 60) + " min " + juce::String (s % 60) + " s";
    }
}

} // namespace stilleto
