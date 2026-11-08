#include "components/TopBar.h"

#include "theme/DesignTokens.h"

namespace stilleto::ui
{

TopBar::TopBar()
{
    for (auto* c : std::initializer_list<juce::Component*> { &microphone, &engine, &offline, &settingsButton, &minimiseButton, &closeButton })
        addAndMakeVisible (c);

    offline.setStatus ("Local", StatusIndicator::Tone::Good);
    offline.setTooltip ("Speech recognition runs entirely on this computer.");

    settingsButton.onClick = [this] { if (onSettings) onSettings(); };
    minimiseButton.onClick = [this] { if (onMinimise) onMinimise(); };
    closeButton.onClick = [this] { if (onClose) onClose(); };

    frames.onFrame = [this] (float dt) { const bool m = titleFade.advance (dt); repaint(); return m; };
}

void TopBar::setTitle (const juce::String& newTitle, const juce::String& newSubtitle)
{
    if (newTitle == title && newSubtitle == subtitle)
        return;

    previousTitle = title;
    title = newTitle;
    subtitle = newSubtitle;
    titleFade.snapTo (0.0f);
    titleFade.setTarget (1.0f);
    frames.kick();
    repaint();
}

void TopBar::resized()
{
    auto r = getLocalBounds().reduced (Metrics::pagePadding - 8, 0).withTrimmedRight (-12);
    const int controlsY = 14;

    closeButton.setBounds (r.getRight() - 40, controlsY, 34, 30);
    minimiseButton.setBounds (closeButton.getX() - 36, controlsY, 34, 30);
    settingsButton.setBounds (minimiseButton.getX() - 46, controlsY, 34, 30);

    int x = settingsButton.getX() - 14;
    for (auto* pill : { &offline, &engine, &microphone })
    {
        const int w = juce::jmin (220, pill->getIdealWidth());
        x -= w;
        pill->setBounds (x, controlsY + 2, w, 26);
        x -= 8;
    }
}

void TopBar::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    g.setColour (Colours::border.withAlpha (0.6f));
    g.fillRect (r.getX() + Metrics::pagePadding, r.getBottom() - 1.0f, r.getWidth() - Metrics::pagePadding * 2, 1.0f);

    const float t = Ease::outCubic (titleFade.get());
    auto textArea = juce::Rectangle<float> ((float) Metrics::pagePadding, 12.0f, (float) microphone.getX() - Metrics::pagePadding - 12.0f, 26.0f);

    if (t < 1.0f && previousTitle.isNotEmpty())
    {
        g.setColour (Colours::text.withAlpha (1.0f - t));
        g.setFont (Fonts::title());
        g.drawText (previousTitle, textArea.translated (0, -6.0f * t), juce::Justification::centredLeft, true);
    }

    g.setColour (Colours::text.withAlpha (t));
    g.setFont (Fonts::title());
    g.drawText (title, textArea.translated (0, 6.0f * (1.0f - t)), juce::Justification::centredLeft, true);

    g.setColour (Colours::textMuted.withAlpha (t));
    g.setFont (Fonts::small());
    g.drawText (subtitle, textArea.translated (0, 26.0f).withHeight (18.0f), juce::Justification::centredLeft, true);
}

void TopBar::mouseDown (const juce::MouseEvent& e)      { if (onDragStart) onDragStart (e); }
void TopBar::mouseDrag (const juce::MouseEvent& e)      { if (onDrag) onDrag (e); }
void TopBar::mouseDoubleClick (const juce::MouseEvent&) { if (onToggleMaximise) onToggleMaximise(); }

} // namespace stilleto::ui
