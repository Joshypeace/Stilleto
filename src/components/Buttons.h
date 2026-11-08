#pragma once

#include "theme/Animation.h"
#include "theme/Icons.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <optional>

namespace stilleto::ui
{

/** Text button with optional icon. Primary = champagne-gold gradient (one per view). */
class StilletoButton : public juce::Button
{
public:
    enum class Style { Primary, Secondary, Ghost, Danger };

    StilletoButton (const juce::String& text, Style style = Style::Secondary, std::optional<Icons::Id> icon = std::nullopt);

    void setStyle (Style s)                     { style = s; repaint(); }
    void setIcon (std::optional<Icons::Id> i)   { icon = i; repaint(); }
    [[nodiscard]] int getIdealWidth() const;

private:
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    void buttonStateChanged() override;
    void enablementChanged() override { repaint(); }

    Style style;
    std::optional<Icons::Id> icon;
    AnimatedValue hover { 0.0f, 0.08f }, press { 0.0f, 0.05f };
    FrameDriver frames;
};

/** Compact square icon button (copy, delete, window controls). */
class IconButton : public juce::Button
{
public:
    IconButton (Icons::Id icon, const juce::String& tooltip, bool danger = false);

    void setIcon (Icons::Id i) { icon = i; repaint(); }
    void setAccent (juce::Colour c) { accent = c; repaint(); }

private:
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    void buttonStateChanged() override;

    Icons::Id icon;
    bool danger;
    std::optional<juce::Colour> accent;
    AnimatedValue hover { 0.0f, 0.07f };
    FrameDriver frames;
};

/** On/off switch with an easing knob. */
class ToggleSwitch : public juce::Button
{
public:
    ToggleSwitch();

private:
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    void buttonStateChanged() override;
    void clicked() override;

    AnimatedValue position { 0.0f, 0.07f }, hover { 0.0f, 0.08f };
    FrameDriver frames;
};

/** Segmented control for 2-4 mutually exclusive options. */
class SegmentedControl : public juce::Component
{
public:
    explicit SegmentedControl (juce::StringArray options);

    std::function<void (int)> onChange;
    void setSelectedIndex (int index, bool notify = false);
    [[nodiscard]] int getSelectedIndex() const noexcept { return selected; }

private:
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    [[nodiscard]] juce::Rectangle<float> segmentBounds (int index) const;

    juce::StringArray options;
    int selected = 0, hovered = -1;
    AnimatedValue indicator { 0.0f, 0.08f };
    FrameDriver frames;
};

} // namespace stilleto::ui
