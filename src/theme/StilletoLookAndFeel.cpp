#include "theme/StilletoLookAndFeel.h"

#include "theme/DesignTokens.h"
#include "theme/Icons.h"

namespace stilleto::ui
{

StilletoLookAndFeel::StilletoLookAndFeel()
{
    setDefaultSansSerifTypefaceName ("Segoe UI");

    auto scheme = getDarkColourScheme();
    scheme.setUIColour (ColourScheme::windowBackground, Colours::background);
    scheme.setUIColour (ColourScheme::widgetBackground, Colours::elevated);
    scheme.setUIColour (ColourScheme::menuBackground, Colours::elevated);
    scheme.setUIColour (ColourScheme::outline, Colours::border);
    scheme.setUIColour (ColourScheme::defaultText, Colours::text);
    scheme.setUIColour (ColourScheme::defaultFill, Colours::gold);
    scheme.setUIColour (ColourScheme::highlightedText, Colours::background);
    scheme.setUIColour (ColourScheme::highlightedFill, Colours::gold);
    scheme.setUIColour (ColourScheme::menuText, Colours::text);
    setColourScheme (scheme);

    setColour (juce::ResizableWindow::backgroundColourId, Colours::background);
    setColour (juce::DocumentWindow::backgroundColourId, Colours::background);
    setColour (juce::Label::textColourId, Colours::text);

    setColour (juce::TextEditor::backgroundColourId, Colours::background2);
    setColour (juce::TextEditor::textColourId, Colours::text);
    setColour (juce::TextEditor::highlightColourId, Colours::gold.withAlpha (0.28f));
    setColour (juce::TextEditor::highlightedTextColourId, Colours::text);
    setColour (juce::TextEditor::outlineColourId, Colours::border);
    setColour (juce::TextEditor::focusedOutlineColourId, Colours::gold.withAlpha (0.7f));
    setColour (juce::CaretComponent::caretColourId, Colours::gold);

    setColour (juce::ComboBox::backgroundColourId, Colours::background2);
    setColour (juce::ComboBox::textColourId, Colours::text);
    setColour (juce::ComboBox::outlineColourId, Colours::border);
    setColour (juce::ComboBox::arrowColourId, Colours::silver);
    setColour (juce::ComboBox::focusedOutlineColourId, Colours::gold.withAlpha (0.7f));

    setColour (juce::PopupMenu::backgroundColourId, Colours::elevated);
    setColour (juce::PopupMenu::textColourId, Colours::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Colours::gold.withAlpha (0.14f));
    setColour (juce::PopupMenu::highlightedTextColourId, Colours::text);

    setColour (juce::ScrollBar::thumbColourId, Colours::silver.withAlpha (0.25f));
    setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::ListBox::outlineColourId, juce::Colours::transparentBlack);

    setColour (juce::Slider::thumbColourId, Colours::gold);
    setColour (juce::Slider::trackColourId, Colours::gold);
    setColour (juce::Slider::backgroundColourId, Colours::border);
    setColour (juce::Slider::textBoxTextColourId, Colours::textSecondary);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);

    setColour (juce::AlertWindow::backgroundColourId, Colours::surface);
    setColour (juce::AlertWindow::textColourId, Colours::textSecondary);
    setColour (juce::AlertWindow::outlineColourId, Colours::border);

    setColour (juce::TextButton::buttonColourId, Colours::elevated);
    setColour (juce::TextButton::buttonOnColourId, Colours::gold);
    setColour (juce::TextButton::textColourOffId, Colours::text);
    setColour (juce::TextButton::textColourOnId, Colours::background);

    setColour (juce::TooltipWindow::backgroundColourId, Colours::elevated);
    setColour (juce::TooltipWindow::textColourId, Colours::text);
    setColour (juce::TooltipWindow::outlineColourId, Colours::border);
}

//==============================================================================================
void StilletoLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<float> (0, 0, (float) width, (float) height).reduced (0.5f);
    const bool hover = box.isMouseOver (true) && box.isEnabled();

    g.setColour (Colours::background2.brighter (hover ? 0.04f : 0.0f));
    g.fillRoundedRectangle (r, Metrics::radiusSmall);

    g.setColour (box.hasKeyboardFocus (true) ? Colours::gold.withAlpha (0.6f) : (hover ? Colours::silver.withAlpha (0.3f) : Colours::border));
    g.drawRoundedRectangle (r, Metrics::radiusSmall, 1.0f);

    // Chevron
    const float cx = width - 18.0f, cy = height * 0.5f;
    juce::Path chevron;
    chevron.startNewSubPath (cx - 4.0f, cy - 2.0f);
    chevron.lineTo (cx, cy + 2.0f);
    chevron.lineTo (cx + 4.0f, cy - 2.0f);
    g.setColour (Colours::silver.withAlpha (box.isEnabled() ? 0.85f : 0.3f));
    g.strokePath (chevron, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::Font StilletoLookAndFeel::getComboBoxFont (juce::ComboBox&) { return Fonts::body(); }

void StilletoLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (12, 1, box.getWidth() - 40, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setColour (juce::Label::textColourId, box.isEnabled() ? Colours::text : Colours::textMuted);
}

//==============================================================================================
void StilletoLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    const auto r = juce::Rectangle<float> (0, 0, (float) width, (float) height);
    g.fillAll (Colours::elevated);
    g.setColour (Colours::border);
    g.drawRect (r, 1.0f);
}

void StilletoLookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator,
                                             bool isActive, bool isHighlighted, bool isTicked, bool hasSubMenu,
                                             const juce::String& text, const juce::String& shortcutKeyText,
                                             const juce::Drawable*, const juce::Colour*)
{
    if (isSeparator)
    {
        g.setColour (Colours::border);
        g.fillRect (area.reduced (10, 0).withHeight (1).withY (area.getCentreY()));
        return;
    }

    auto r = area.reduced (4, 1).toFloat();

    if (isHighlighted && isActive)
    {
        g.setColour (Colours::gold.withAlpha (0.12f));
        g.fillRoundedRectangle (r, 6.0f);
    }

    auto textArea = r.reduced (12, 0);

    if (isTicked)
    {
        Icons::draw (g, Icons::Id::Check, textArea.removeFromLeft (16).withSizeKeepingCentre (14, 14), Colours::gold, 1.8f);
        textArea.removeFromLeft (6);
    }
    else
    {
        textArea.removeFromLeft (22);
    }

    g.setFont (getPopupMenuFont());
    g.setColour (isActive ? (isHighlighted ? Colours::text : Colours::text.withAlpha (0.9f)) : Colours::textMuted);
    g.drawFittedText (text, textArea.toNearestInt(), juce::Justification::centredLeft, 1);

    if (shortcutKeyText.isNotEmpty())
    {
        g.setFont (Fonts::mono (11.5f));
        g.setColour (Colours::textMuted);
        g.drawText (shortcutKeyText, textArea.toNearestInt(), juce::Justification::centredRight);
    }

    if (hasSubMenu)
    {
        const float x = r.getRight() - 14, y = r.getCentreY();
        juce::Path p;
        p.startNewSubPath (x - 2, y - 4); p.lineTo (x + 2, y); p.lineTo (x - 2, y + 4);
        g.setColour (Colours::silver);
        g.strokePath (p, juce::PathStrokeType (1.4f));
    }
}

juce::Font StilletoLookAndFeel::getPopupMenuFont() { return Fonts::body(); }

//==============================================================================================
void StilletoLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                            float, float, juce::Slider::SliderStyle, juce::Slider& slider)
{
    const float cy = y + height * 0.5f;
    const auto track = juce::Rectangle<float> ((float) x, cy - 2.0f, (float) width, 4.0f);

    g.setColour (Colours::border);
    g.fillRoundedRectangle (track, 2.0f);

    const auto filled = track.withRight (sliderPos);
    g.setGradientFill (goldGradient (filled));
    g.fillRoundedRectangle (filled, 2.0f);

    const float knob = slider.isMouseOverOrDragging() ? 16.0f : 14.0f;
    const auto k = juce::Rectangle<float> (knob, knob).withCentre ({ sliderPos, cy });
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillEllipse (k.translated (0, 1.5f));
    g.setGradientFill (juce::ColourGradient (Colours::text, k.getX(), k.getY(), Colours::silver.darker (0.2f), k.getX(), k.getBottom(), false));
    g.fillEllipse (k);
}

juce::Label* StilletoLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* label = LookAndFeel_V4::createSliderTextBox (slider);
    label->setFont (Fonts::mono (12.0f));
    label->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    label->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    label->setColour (juce::Label::textColourId, Colours::textSecondary);
    return label;
}

//==============================================================================================
void StilletoLookAndFeel::fillTextEditorBackground (juce::Graphics& g, int width, int height, juce::TextEditor& editor)
{
    g.setColour (editor.findColour (juce::TextEditor::backgroundColourId));
    g.fillRoundedRectangle (juce::Rectangle<float> (0, 0, (float) width, (float) height), Metrics::radiusSmall);
}

void StilletoLookAndFeel::drawTextEditorOutline (juce::Graphics& g, int width, int height, juce::TextEditor& editor)
{
    if (! editor.isEnabled()) return;
    const auto r = juce::Rectangle<float> (0, 0, (float) width, (float) height).reduced (0.5f);
    const bool focused = editor.hasKeyboardFocus (true) && ! editor.isReadOnly();
    g.setColour (focused ? editor.findColour (juce::TextEditor::focusedOutlineColourId)
                         : editor.findColour (juce::TextEditor::outlineColourId));
    g.drawRoundedRectangle (r, Metrics::radiusSmall, 1.0f);
}

//==============================================================================================
void StilletoLookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int width, int height,
                                         bool isVertical, int thumbStart, int thumbSize, bool isMouseOver, bool isMouseDown)
{
    juce::Rectangle<float> thumb;
    if (isVertical)
        thumb = juce::Rectangle<float> ((float) x + width * 0.3f, (float) thumbStart, width * 0.4f, (float) thumbSize);
    else
        thumb = juce::Rectangle<float> ((float) thumbStart, (float) y + height * 0.3f, (float) thumbSize, height * 0.4f);

    g.setColour (Colours::silver.withAlpha (isMouseDown ? 0.45f : isMouseOver ? 0.32f : 0.18f));
    g.fillRoundedRectangle (thumb.reduced (0, 2), juce::jmin (thumb.getWidth(), thumb.getHeight()) * 0.5f);
}

//==============================================================================================
void StilletoLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                                bool highlighted, bool down)
{
    const auto r = button.getLocalBounds().toFloat().reduced (1.0f);
    const bool primary = button.getToggleState() || button.getProperties().getWithDefault ("primary", false);

    if (primary)
    {
        g.setGradientFill (goldGradient (r, down ? 0.9f : highlighted ? 1.08f : 1.0f));
        g.fillRoundedRectangle (r, Metrics::radiusSmall);
    }
    else
    {
        g.setColour (Colours::elevated.brighter (down ? 0.02f : highlighted ? 0.08f : 0.0f));
        g.fillRoundedRectangle (r, Metrics::radiusSmall);
        g.setColour (highlighted ? Colours::silver.withAlpha (0.35f) : Colours::border);
        g.drawRoundedRectangle (r, Metrics::radiusSmall, 1.0f);
    }
}

juce::Font StilletoLookAndFeel::getTextButtonFont (juce::TextButton&, int) { return Fonts::bodyStrong(); }

//==============================================================================================
juce::AlertWindow* StilletoLookAndFeel::createAlertWindow (const juce::String& title, const juce::String& message,
                                                           const juce::String& button1, const juce::String& button2,
                                                           const juce::String& button3, juce::MessageBoxIconType iconType,
                                                           int numButtons, juce::Component* associatedComponent)
{
    auto* w = LookAndFeel_V4::createAlertWindow (title, message, button1, button2, button3, iconType, numButtons, associatedComponent);

    // The first button is the affirmative action; give it the gold treatment.
    for (int i = 0; i < w->getNumButtons(); ++i)
        if (auto* b = dynamic_cast<juce::TextButton*> (w->getButton (i)))
        {
            b->getProperties().set ("primary", i == 0);
            b->setColour (juce::TextButton::textColourOffId, i == 0 ? Colours::background : Colours::text);
        }

    return w;
}

void StilletoLookAndFeel::drawAlertBox (juce::Graphics& g, juce::AlertWindow& alert, const juce::Rectangle<int>& textArea,
                                        juce::TextLayout& layout)
{
    const auto r = alert.getLocalBounds().toFloat();
    g.fillAll (Colours::surface);

    g.setGradientFill (silverEdge (r, 1.4f));
    g.drawRect (r, 1.0f);

    g.setGradientFill (goldGradient (r.withHeight (2.0f)));
    g.fillRect (r.withHeight (2.0f));

    layout.draw (g, textArea.toFloat());
}

juce::Font StilletoLookAndFeel::getAlertWindowTitleFont()   { return Fonts::title(); }
juce::Font StilletoLookAndFeel::getAlertWindowMessageFont() { return Fonts::body(); }
juce::Font StilletoLookAndFeel::getAlertWindowFont()        { return Fonts::body(); }

//==============================================================================================
juce::Rectangle<int> StilletoLookAndFeel::getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos,
                                                            juce::Rectangle<int> parentArea)
{
    const auto font = Fonts::small();
    const int w = juce::jmin (320, juce::GlyphArrangement::getStringWidthInt (font, tipText) + 22);
    const int h = 28;
    return juce::Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 18,
                                 screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 8) : screenPos.y + 18, w, h)
        .constrainedWithin (parentArea);
}

void StilletoLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height)
{
    const auto r = juce::Rectangle<float> (0, 0, (float) width, (float) height);
    g.setColour (Colours::elevated);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (Colours::border);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
    g.setColour (Colours::text);
    g.setFont (Fonts::small());
    g.drawFittedText (text, r.reduced (10, 2).toNearestInt(), juce::Justification::centredLeft, 1);
}

} // namespace stilleto::ui
