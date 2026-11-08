#pragma once

#include "theme/DesignTokens.h"
#include "theme/Icons.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <optional>

namespace stilleto::ui
{

/** A layered surface with an optional heading row. Children are laid out by the owner inside
    getContentBounds(). */
class Card : public juce::Component
{
public:
    explicit Card (juce::String titleText = {}, juce::String subtitleText = {})
        : title (std::move (titleText)), subtitle (std::move (subtitleText))
    {
        setOpaque (false);
    }

    void setTitle (const juce::String& t, const juce::String& s = {}) { title = t; subtitle = s; repaint(); }
    void setIcon (std::optional<Icons::Id> i) { icon = i; repaint(); }
    void setEmphasis (float e) { emphasis = e; repaint(); }

    [[nodiscard]] juce::Rectangle<int> getContentBounds() const
    {
        auto r = getLocalBounds().reduced (Metrics::cardPadding);
        if (title.isNotEmpty())
            r.removeFromTop (subtitle.isNotEmpty() ? 50 : 32);
        return r;
    }

    void paint (juce::Graphics& g) override
    {
        paintCard (g, getLocalBounds().toFloat().reduced (2.0f), Metrics::radiusLarge, Colours::surface, emphasis);

        if (title.isEmpty())
            return;

        auto r = getLocalBounds().reduced (Metrics::cardPadding);
        auto header = r.removeFromTop (22);

        if (icon)
        {
            Icons::draw (g, *icon, header.removeFromLeft (18).toFloat().withSizeKeepingCentre (17, 17), Colours::silver, 1.6f);
            header.removeFromLeft (10);
        }

        g.setColour (Colours::text);
        g.setFont (Fonts::heading());
        g.drawText (title, header, juce::Justification::centredLeft);

        if (subtitle.isNotEmpty())
        {
            g.setColour (Colours::textMuted);
            g.setFont (Fonts::small());
            g.drawText (subtitle, r.removeFromTop (22).withTrimmedLeft (icon ? 28 : 0), juce::Justification::centredLeft);
        }
    }

private:
    juce::String title, subtitle;
    std::optional<Icons::Id> icon;
    float emphasis = 0.0f;
};

/** Small uppercase label used above groups of controls. */
inline void drawSectionLabel (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area)
{
    g.setColour (Colours::textMuted);
    g.setFont (Fonts::caption());
    g.drawText (text.toUpperCase(), area, juce::Justification::centredLeft);
}

/** Keycap rendering for shortcuts ("Ctrl" "Alt" "Space"). Returns the width used. */
inline float drawKeycaps (juce::Graphics& g, const juce::String& shortcut, juce::Point<float> origin, float height = 26.0f,
                          bool measureOnly = false)
{
    const auto parts = juce::StringArray::fromTokens (shortcut, "+", {});
    const auto font = Fonts::mono (height * 0.46f);
    float x = origin.x;

    for (int i = 0; i < parts.size(); ++i)
    {
        const auto label = parts[i].trim();
        const float w = juce::jmax (height, (float) juce::GlyphArrangement::getStringWidthInt (font, label) + 18.0f);
        const auto cap = juce::Rectangle<float> (x, origin.y, w, height);

        if (! measureOnly)
        {
            g.setColour (Colours::background);
            g.fillRoundedRectangle (cap.translated (0, 2.0f), 6.0f);
            g.setGradientFill (juce::ColourGradient (Colours::elevated.brighter (0.12f), cap.getX(), cap.getY(),
                                                     Colours::elevated, cap.getX(), cap.getBottom(), false));
            g.fillRoundedRectangle (cap, 6.0f);
            g.setColour (Colours::silver.withAlpha (0.28f));
            g.drawRoundedRectangle (cap.reduced (0.5f), 6.0f, 1.0f);
            g.setColour (Colours::text);
            g.setFont (font);
            g.drawText (label, cap, juce::Justification::centred);
        }

        x += w;
        if (i < parts.size() - 1)
        {
            if (! measureOnly)
            {
                g.setColour (Colours::textMuted);
                g.setFont (Fonts::small());
                g.drawText ("+", juce::Rectangle<float> (x, origin.y, 16.0f, height), juce::Justification::centred);
            }
            x += 16.0f;
        }
    }

    return x - origin.x;
}

} // namespace stilleto::ui
