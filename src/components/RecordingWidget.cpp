#include "components/RecordingWidget.h"

#include "theme/DesignTokens.h"
#include "theme/Icons.h"
#include "windows/NativeWindow.h"

#include <numbers>

namespace stilleto
{
namespace
{
constexpr int widgetWidth = 420;
constexpr int widgetHeight = 68;

juce::String formatTime (double seconds)
{
    const int s = juce::jmax (0, (int) seconds);
    return juce::String (s / 60) + ":" + juce::String (s % 60).paddedLeft ('0', 2);
}

void disableFocus (juce::Component& c)
{
    c.setWantsKeyboardFocus (false);
    c.setMouseClickGrabsKeyboardFocus (false);
}
} // namespace

RecordingWidget::RecordingWidget (AppController& c)
    : controller (c),
      waveform ([this] (float* dest, int count) { controller.getRecorder().copyEnvelope (dest, count); })
{
    setOpaque (false);
    disableFocus (*this);
    setSize (widgetWidth, widgetHeight);

    waveform.setBarStyle (34, 0.5f);
    for (auto* child : std::initializer_list<juce::Component*> { &waveform, &primaryButton, &dismissButton })
    {
        addAndMakeVisible (child);
        disableFocus (*child);
    }

    primaryButton.onClick = [this]
    {
        if (controller.state() == DictationState::Recording)
            controller.stopDictation();
        else if (canStartRecording (controller.state()))
            controller.startDictation (AppController::Source::Widget);   // target = the still-focused app
    };

    dismissButton.onClick = [this] { controller.cancelDictation(); };

    // Layered (per-pixel alpha) tool window; styles are completed natively after creation.
    addToDesktop (juce::ComponentPeer::windowIsTemporary | juce::ComponentPeer::windowIsSemiTransparent);
    native::makeNonActivatingOverlay (getWindowHandle());
    setAlwaysOnTop (true);

    frames.onFrame = [this] (float dt)
    {
        bool moving = visibility.advance (dt) | stateMix.advance (dt);
        if (! ui::Motion::reduced())
            pulse = std::fmod (pulse + dt * 0.8f, 1.0f);

        applyAnimatedBounds();
        repaint();

        if (! wantsVisible && visibility.get() < 0.01f)
        {
            setVisible (false);
            waveform.setActive (false);
            return false;
        }

        return moving || shownState == DictationState::Recording || shownState == DictationState::Processing
               || shownState == DictationState::Inserting;
    };

    controller.addListener (this);
    sessionChanged();
}

RecordingWidget::~RecordingWidget()
{
    controller.removeListener (this);
}

void RecordingWidget::setShouldBeShown (bool show, void* targetWindow)
{
    if (show == wantsVisible)
        return;

    wantsVisible = show;

    if (show)
    {
        if (! isVisible())
        {
            placeOnScreen (targetWindow);
            visibility.snapTo (0.0f);
            applyAnimatedBounds();
            setVisible (true);   // SW_SHOWNA: no activation
            native::makeNonActivatingOverlay (getWindowHandle());
        }
        native::keepOnTopWithoutActivating (getWindowHandle());
        visibility.setTarget (1.0f);
    }
    else
    {
        visibility.setTarget (0.0f);
    }

    frames.kick();
}

void RecordingWidget::placeOnScreen (void* targetWindow)
{
    if (userPositioned && ! restingBounds.isEmpty())
        return;

    juce::ignoreUnused (targetWindow);
    const auto& displays = juce::Desktop::getInstance().getDisplays();
    const auto* display = displays.getDisplayForPoint (juce::Desktop::getMousePosition().toFloat());
    if (display == nullptr) display = displays.getPrimaryDisplay();
    if (display == nullptr) return;

    const auto area = display->userBounds.toNearestInt();
    restingBounds = juce::Rectangle<int> (widgetWidth, widgetHeight)
                        .withCentre ({ area.getCentreX(), area.getBottom() - 56 - widgetHeight / 2 });
}

void RecordingWidget::applyAnimatedBounds()
{
    const float v = ui::Ease::outCubic (visibility.get());
    const int offset = juce::roundToInt ((1.0f - v) * 14.0f);
    setBounds (restingBounds.translated (0, offset));
    setAlpha (juce::jlimit (0.0f, 1.0f, visibility.get()));
}

void RecordingWidget::sessionChanged()
{
    const auto& s = controller.session();

    if (s.state != shownState)
    {
        shownState = s.state;
        stateMix.snapTo (0.0f);
        stateMix.setTarget (1.0f);
    }

    waveform.setActive (s.state == DictationState::Recording);
    waveform.setVisible (s.state == DictationState::Recording);
    updateButtons();

    if (s.state == DictationState::Recording)
        startTimerHz (4);   // elapsed time label
    else
        stopTimer();

    frames.kick();
    resized();
    repaint();
}

void RecordingWidget::updateButtons()
{
    switch (shownState)
    {
        case DictationState::Recording:
            primaryButton.setIcon (ui::Icons::Id::Stop);
            primaryButton.setTooltip ("Stop and transcribe");
            primaryButton.setAccent (ui::Colours::gold);
            primaryButton.setVisible (true);
            dismissButton.setTooltip ("Discard recording");
            dismissButton.setVisible (true);
            break;

        case DictationState::Processing:
            primaryButton.setVisible (false);
            dismissButton.setTooltip ("Cancel transcription");
            dismissButton.setVisible (true);
            break;

        case DictationState::Inserting:
            primaryButton.setVisible (false);
            dismissButton.setVisible (false);
            break;

        case DictationState::Idle:
            primaryButton.setIcon (ui::Icons::Id::Mic);
            primaryButton.setTooltip ("Start dictation into the active window");
            primaryButton.setAccent (ui::Colours::silver);
            primaryButton.setVisible (true);
            dismissButton.setVisible (false);
            break;

        case DictationState::Completed:
        case DictationState::Error:
            primaryButton.setVisible (false);
            dismissButton.setTooltip ("Dismiss");
            dismissButton.setVisible (true);
            break;
    }
}

void RecordingWidget::timerCallback()
{
    repaint();
}

void RecordingWidget::resized()
{
    auto r = getLocalBounds().reduced (14, 0);
    dismissButton.setBounds (r.removeFromRight (32).withSizeKeepingCentre (32, 32));
    if (primaryButton.isVisible())
    {
        r.removeFromRight (4);
        primaryButton.setBounds (r.removeFromRight (32).withSizeKeepingCentre (32, 32));
    }

    // Waveform sits between the status text and the controls.
    waveform.setBounds (juce::Rectangle<int> (178, 0, juce::jmax (40, r.getRight() - 186), getHeight()).withSizeKeepingCentre (juce::jmax (40, r.getRight() - 186), 30));
}

void RecordingWidget::paint (juce::Graphics& g)
{
    using namespace ui;
    const auto r = getLocalBounds().toFloat().reduced (2.0f);
    const float radius = r.getHeight() * 0.5f;
    const auto& s = controller.session();

    g.setGradientFill (juce::ColourGradient (Colours::elevated, r.getX(), r.getY(), Colours::surface.darker (0.2f), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, radius);

    const bool recording = shownState == DictationState::Recording;
    g.setGradientFill (silverEdge (r, 1.6f));
    g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
    if (recording)
    {
        g.setColour (Colours::gold.withAlpha (0.35f));
        g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
    }

    const float mix = Ease::outCubic (stateMix.get());
    const auto badge = juce::Rectangle<float> (r.getX() + 12.0f, r.getCentreY() - 20.0f, 40.0f, 40.0f);
    const float pulseWave = Motion::reduced() ? 0.5f : 0.5f + 0.5f * std::sin (pulse * 2.0f * std::numbers::pi_v<float>);

    switch (shownState)
    {
        case DictationState::Recording:
        {
            const float ring = 30.0f + 8.0f * pulseWave;
            g.setColour (Colours::gold.withAlpha (0.18f + 0.12f * pulseWave));
            g.fillEllipse (juce::Rectangle<float> (ring, ring).withCentre (badge.getCentre()));
            const auto core = badge.withSizeKeepingCentre (28.0f, 28.0f);
            g.setGradientFill (goldGradient (core));
            g.fillEllipse (core);
            Icons::draw (g, Icons::Id::Mic, core.reduced (6.0f), juce::Colour (0xff1a150c), 2.0f);
            break;
        }
        case DictationState::Processing:
        case DictationState::Inserting:
        {
            Icons::drawBrandMark (g, badge.reduced (8.0f), 0.0f, false);
            juce::Path arc;
            const float start = pulse * 2.0f * std::numbers::pi_v<float> * 1.5f;
            arc.addCentredArc (badge.getCentreX(), badge.getCentreY(), 17.0f, 17.0f, 0.0f, start, start + 1.6f, true);
            g.setColour (Colours::gold);
            g.strokePath (arc, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            break;
        }
        case DictationState::Completed:
        {
            const auto core = badge.withSizeKeepingCentre (30.0f * (0.7f + 0.3f * mix), 30.0f * (0.7f + 0.3f * mix));
            g.setColour (Colours::success.withAlpha (0.16f));
            g.fillEllipse (core);
            Icons::draw (g, Icons::Id::Check, core.reduced (7.0f), Colours::success, 2.2f);
            break;
        }
        case DictationState::Error:
        {
            const auto core = badge.withSizeKeepingCentre (30.0f, 30.0f);
            g.setColour (Colours::error.withAlpha (0.14f));
            g.fillEllipse (core);
            Icons::draw (g, Icons::Id::Alert, core.reduced (5.0f), Colours::error, 1.8f);
            break;
        }
        case DictationState::Idle:
            Icons::drawBrandMark (g, badge.reduced (4.0f));
            break;
    }

    // Text block.
    const float textX = badge.getRight() + 12.0f;
    const float textRight = recording ? (float) waveform.getX() - 8.0f
                                      : (float) (primaryButton.isVisible() ? primaryButton.getX() : dismissButton.getX()) - 10.0f;
    const auto textArea = juce::Rectangle<float> (textX, r.getY() + 13.0f, juce::jmax (40.0f, textRight - textX), r.getHeight() - 26.0f);

    juce::String headline, detail;
    switch (shownState)
    {
        case DictationState::Recording:
            headline = formatTime (controller.liveRecordingSeconds());
            detail = "Listening";
            break;
        case DictationState::Processing:
            headline = "Transcribing";
            detail = s.detail.startsWith ("Cancel") ? s.detail : "On this device";
            break;
        case DictationState::Inserting:
            headline = "Inserting";
            detail = s.targetName.isNotEmpty() ? "into " + s.targetName : "into the active window";
            break;
        case DictationState::Completed:
            headline = s.transcript.isNotEmpty() ? s.transcript : s.headline;
            detail = s.inserted ? (s.clipboardNote.isNotEmpty() ? juce::String ("Inserted - clipboard only partly restored") : "Inserted into " + s.targetName)
                                : s.insertionNote;
            break;
        case DictationState::Error:
            headline = s.headline;
            detail = s.detail;
            break;
        case DictationState::Idle:
            headline = "Ready to dictate";
            detail = "Press " + controller.hotkeyText();
            break;
    }

    g.setOpacity (0.35f + 0.65f * mix);
    g.setColour (shownState == DictationState::Error ? Colours::error.brighter (0.2f) : Colours::text);
    g.setFont (recording ? Fonts::mono (17.0f) : Fonts::bodyStrong());
    g.drawText (headline, textArea.withHeight (textArea.getHeight() * 0.55f), juce::Justification::centredLeft, true);

    g.setColour (recording ? Colours::gold : Colours::textSecondary);
    g.setFont (Fonts::small());
    g.drawText (detail, textArea.withTrimmedTop (textArea.getHeight() * 0.55f), juce::Justification::centredLeft, true);
    g.setOpacity (1.0f);
}

void RecordingWidget::mouseDown (const juce::MouseEvent& e)
{
    dragOffset = e.getScreenPosition() - getScreenPosition();
}

void RecordingWidget::mouseDrag (const juce::MouseEvent& e)
{
    userPositioned = true;
    restingBounds.setPosition (e.getScreenPosition() - dragOffset);
    setTopLeftPosition (restingBounds.getPosition());
}

} // namespace stilleto
