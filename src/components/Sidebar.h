#pragma once

#include "theme/Animation.h"
#include "theme/Icons.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

namespace stilleto::ui
{

/** Navigation rail: brand, sections with a sliding gold selection indicator, and a footer showing
    the active shortcut and the local/offline promise. */
class Sidebar : public juce::Component
{
public:
    struct Item
    {
        Icons::Id icon;
        juce::String label;
    };

    explicit Sidebar (std::vector<Item> items);

    std::function<void (int)> onSelect;

    void setSelected (int index);
    void setShortcutText (const juce::String& text, bool paused);
    void playIntro();   // brand shimmer on startup

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    [[nodiscard]] juce::Rectangle<float> itemBounds (int index) const;
    [[nodiscard]] int itemAt (juce::Point<float>) const;

    std::vector<Item> items;
    int selected = 0, hovered = -1;
    AnimatedValue indicatorY { 0.0f, 0.085f };
    juce::Array<float> hoverAmounts;
    float shimmer = 1.0f;
    juce::String shortcut;
    bool shortcutPaused = false;
    FrameDriver frames;
};

} // namespace stilleto::ui
