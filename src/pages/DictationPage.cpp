#include "pages/DictationPage.h"

namespace stilleto
{
using namespace ui;

namespace
{
juce::String clock (double seconds)
{
    const int s = juce::jmax (0, (int) seconds);
    return juce::String (s / 60).paddedLeft ('0', 2) + ":" + juce::String (s % 60).paddedLeft ('0', 2);
}
}

DictationPage::DictationPage (AppController& c)
    : Page (c),
      orb ([this] { return controller.getRecorder().currentLevel(); }),
      waveform ([this] (float* d, int n) { controller.getRecorder().copyEnvelope (d, n); })
{
    addAndMakeVisible (stage);
    addAndMakeVisible (transcriptCard);
    stage.setInterceptsMouseClicks (false, true);
    stage.addAndMakeVisible (orb);
    stage.addAndMakeVisible (waveform);
    stage.addAndMakeVisible (stateBadge);
    stage.addAndMakeVisible (cancelButton);

    waveform.setBarStyle (72, 0.5f);

    transcriptCard.addAndMakeVisible (transcript);
    transcriptCard.addAndMakeVisible (copyButton);
    transcriptCard.addAndMakeVisible (insertButton);
    transcriptCard.addAndMakeVisible (autoInsert);
    transcriptCard.addAndMakeVisible (autoInsertLabel);

    transcript.setMultiLine (true, true);
    transcript.setReadOnly (true);
    transcript.setScrollbarsShown (true);
    transcript.setCaretVisible (false);
    transcript.setFont (Fonts::make (15.5f));
    transcript.setColour (juce::TextEditor::backgroundColourId, Colours::background2);
    transcript.setBorder ({ 14, 16, 14, 16 });
    transcript.setTextToShowWhenEmpty ("Your transcript appears here after you stop recording.", Colours::textMuted);

    styleLabel (autoInsertLabel, Fonts::small(), Colours::textSecondary);
    autoInsertLabel.setText ("Insert automatically after hotkey dictation", juce::dontSendNotification);
    autoInsert.setToggleState (controller.getSettings().autoInsert(), juce::dontSendNotification);
    autoInsert.onClick = [this] { controller.getSettings().setAutoInsert (autoInsert.getToggleState()); };
    autoInsert.setTitle ("Insert automatically");

    orb.onClick = [this]
    {
        if (controller.state() == DictationState::Recording) controller.stopDictation();
        else controller.startDictation (AppController::Source::MainWindow);
    };
    cancelButton.onClick = [this] { controller.cancelDictation(); };

    copyButton.onClick = [this]
    {
        const bool ok = controller.copyToClipboard (transcript.getText());
        feedback = ok ? "Copied to the clipboard" : "The clipboard is in use by another application";
        feedbackUntil = juce::Time::getMillisecondCounter() + 2500;
        repaint();
    };
    insertButton.onClick = [this] { controller.insertIntoLastApplication (transcript.getText(), controller.session().historyId); };

    controller.addListener (this);
    sessionChanged();
}

DictationPage::~DictationPage()
{
    controller.removeListener (this);
}

void DictationPage::sessionChanged()
{
    const auto& s = controller.session();
    const bool recording = s.state == DictationState::Recording;
    const bool busy = isBusy (s.state);
    const bool modelUsable = controller.modelStatus() == AppController::ModelStatus::Ready
                          || controller.modelStatus() == AppController::ModelStatus::Loading;

    orb.setMode (recording ? MicOrb::Mode::Recording
                 : busy ? MicOrb::Mode::Processing
                 : modelUsable ? MicOrb::Mode::Idle : MicOrb::Mode::Disabled);
    waveform.setActive (recording);
    cancelButton.setVisible (recording || s.state == DictationState::Processing);

    switch (s.state)
    {
        case DictationState::Idle:       stateBadge.setStatus (modelUsable ? "Ready" : "No model", modelUsable ? StatusIndicator::Tone::Good : StatusIndicator::Tone::Warning); break;
        case DictationState::Recording:  stateBadge.setStatus ("Recording", StatusIndicator::Tone::Gold, true); break;
        case DictationState::Processing: stateBadge.setStatus ("Transcribing", StatusIndicator::Tone::Busy, true); break;
        case DictationState::Inserting:  stateBadge.setStatus ("Inserting", StatusIndicator::Tone::Busy, true); break;
        case DictationState::Completed:  stateBadge.setStatus (s.inserted ? "Inserted" : "Done", StatusIndicator::Tone::Good); break;
        case DictationState::Error:      stateBadge.setStatus ("Needs attention", StatusIndicator::Tone::Bad); break;
    }

    if (s.transcript != transcript.getText())
        transcript.setText (s.transcript, false);

    const auto app = controller.lastApplicationName();
    insertButton.setButtonText (app.isNotEmpty() ? "Insert into " + app : "Insert");
    insertButton.setEnabled (s.transcript.isNotEmpty() && app.isNotEmpty() && ! busy && ! recording);
    copyButton.setEnabled (s.transcript.isNotEmpty());
    autoInsert.setToggleState (controller.getSettings().autoInsert(), juce::dontSendNotification);

    if (recording) startTimerHz (10);
    else stopTimer();

    resized();
    repaint();
}

void DictationPage::timerCallback()
{
    repaint (stage.getBounds());
}

void DictationPage::resized()
{
    auto r = getLocalBounds().reduced (Metrics::pagePadding, Metrics::pagePadding - 8);

    stage.setBounds (r.removeFromTop (juce::jlimit (300, 380, r.getHeight() * 56 / 100)));
    r.removeFromTop (Metrics::gap);
    transcriptCard.setBounds (r);

    // Stage: orb on the left, state text to the right, waveform along the bottom.
    auto s = stage.getLocalBounds().reduced (28, 24);
    auto wave = s.removeFromBottom (64);
    waveform.setBounds (wave);
    s.removeFromBottom (12);

    const int orbSize = juce::jmin (s.getHeight(), 230);
    orb.setBounds (s.removeFromLeft (orbSize).withSizeKeepingCentre (orbSize, orbSize));
    s.removeFromLeft (32);

    stateBadge.setBounds (s.getX(), s.getY() + 18, juce::jmax (110, stateBadge.getIdealWidth()), 26);
    cancelButton.setBounds (s.getRight() - 110, s.getY() + 14, 110, 34);
    stageTextArea = s.withTrimmedTop (60);

    // Transcript card.
    auto t = transcriptCard.getContentBounds();
    auto actions = t.removeFromBottom (40);
    t.removeFromBottom (12);
    transcript.setBounds (t);

    insertButton.setBounds (actions.removeFromRight (juce::jmax (120, insertButton.getIdealWidth())));
    actions.removeFromRight (10);
    copyButton.setBounds (actions.removeFromRight (copyButton.getIdealWidth()));
    autoInsert.setBounds (actions.removeFromLeft (46).withSizeKeepingCentre (46, 28));
    actions.removeFromLeft (8);
    autoInsertLabel.setBounds (actions.removeFromLeft (320));
}

void DictationPage::paintOverChildren (juce::Graphics& g)
{
    const auto& s = controller.session();
    auto r = stageTextArea + stage.getPosition();

    juce::String big, line1, line2;
    juce::Colour bigColour = Colours::text;

    switch (s.state)
    {
        case DictationState::Recording:
            big = clock (controller.liveRecordingSeconds());
            line1 = "Listening. Speak naturally.";
            line2 = s.targetName.isNotEmpty() ? "Text will go to " + s.targetName : "Press " + controller.hotkeyText() + " or the button to finish.";
            bigColour = Colours::gold;
            break;
        case DictationState::Processing:
            big = "Transcribing";
            line1 = s.detail;
            line2 = "Recorded " + formatDuration (s.recordedSeconds) + ". Nothing leaves this computer.";
            break;
        case DictationState::Inserting:
            big = "Inserting";
            line1 = s.detail;
            break;
        case DictationState::Completed:
            big = s.inserted ? "Inserted" : "Transcribed";
            line1 = s.inserted ? "Typed into " + s.targetName : s.insertionNote;
            line2 = juce::String (s.transcript.isEmpty() ? 0 : juce::StringArray::fromTokens (s.transcript, true).size()) + " words from "
                    + formatDuration (s.recordedSeconds) + " of audio";
            bigColour = Colours::success;
            break;
        case DictationState::Error:
            big = s.headline;
            line1 = s.detail;
            bigColour = Colours::error;
            break;
        case DictationState::Idle:
            big = controller.modelStatus() == AppController::ModelStatus::NoModel ? "Install a model first" : "Ready when you are";
            line1 = controller.modelStatus() == AppController::ModelStatus::NoModel
                        ? "Open Models to download base.en (142 MB), then come back."
                        : "Click the microphone, or press " + controller.hotkeyText() + " from any application.";
            line2 = "Using " + controller.modelName() + "  ·  " + controller.microphoneName();
            break;
    }

    g.setColour (bigColour);
    g.setFont (s.state == DictationState::Recording ? Fonts::mono (40.0f) : Fonts::make (30.0f, "Light"));
    g.drawText (big, r.removeFromTop (52), juce::Justification::centredLeft, true);

    g.setColour (Colours::textSecondary);
    g.setFont (Fonts::body());
    g.drawFittedText (line1, r.removeFromTop (44), juce::Justification::topLeft, 2);

    g.setColour (Colours::textMuted);
    g.setFont (Fonts::small());
    g.drawFittedText (line2, r.removeFromTop (36), juce::Justification::topLeft, 2);

    if (feedback.isNotEmpty() && juce::Time::getMillisecondCounter() < feedbackUntil)
    {
        g.setColour (Colours::success);
        g.setFont (Fonts::small());
        g.drawText (feedback, (copyButton.getBounds() + transcriptCard.getPosition()).translated (-260, 0).withWidth (250),
                    juce::Justification::centredRight);
    }
}

} // namespace stilleto
