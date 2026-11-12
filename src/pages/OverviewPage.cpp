#include "pages/OverviewPage.h"

namespace stilleto
{
using namespace ui;

OverviewPage::OverviewPage (AppController& c) : Page (c)
{
    for (auto* comp : std::initializer_list<juce::Component*> { &hero, &readinessCard, &recentCard, &activityCard,
                                                                 &dictations, &words, &minutes })
        addAndMakeVisible (comp);

    hero.setInterceptsMouseClicks (false, true);
    hero.addAndMakeVisible (startButton);
    hero.addAndMakeVisible (modelsButton);
    recentCard.addAndMakeVisible (copyButton);
    recentCard.addAndMakeVisible (insertButton);

    startButton.onClick = [this]
    {
        if (controller.state() == DictationState::Recording)
            controller.stopDictation();
        else
        {
            navigate (AppController::Page::Dictation);
            controller.startDictation (AppController::Source::MainWindow);
        }
    };
    modelsButton.onClick = [this] { navigate (AppController::Page::Models); };

    copyButton.onClick = [this]
    {
        const auto& entries = controller.getHistory().entries();
        const auto text = ! entries.empty() ? entries.front().text : controller.session().transcript;
        const bool ok = controller.copyToClipboard (text);
        copyFeedback = ok ? "Copied to the clipboard" : "The clipboard is in use by another application";
        copyFeedbackIsError = ! ok;
        copyFeedbackUntil = juce::Time::getMillisecondCounter() + 2500;
        repaint();
        juce::Timer::callAfterDelay (2600, [safe = juce::Component::SafePointer<OverviewPage> (this)] { if (safe) safe->repaint(); });
    };
    insertButton.onClick = [this]
    {
        const auto& entries = controller.getHistory().entries();
        if (! entries.empty()) controller.insertIntoLastApplication (entries.front().text, entries.front().id);
        else controller.insertIntoLastApplication (controller.session().transcript);
    };

    controller.addListener (this);
    controller.getHistory().addListener (this);
    refresh();
}

OverviewPage::~OverviewPage()
{
    controller.removeListener (this);
    controller.getHistory().removeListener (this);
}

juce::String OverviewPage::pageSubtitle() const
{
    const int hour = juce::Time::getCurrentTime().getHours();
    return juce::String (hour < 12 ? "Good morning" : hour < 18 ? "Good afternoon" : "Good evening")
           + ". Your voice, anywhere you type.";
}

juce::Array<OverviewPage::Readiness> OverviewPage::readiness() const
{
    juce::Array<Readiness> rows;

    rows.add ({ Icons::Id::Mic, "Microphone",
                controller.microphoneAvailable() ? controller.microphoneName() : "Not found",
                controller.microphoneAvailable() ? Colours::success : Colours::error });

    juce::Colour modelTone = Colours::error;
    juce::String modelValue = "Not installed";
    switch (controller.modelStatus())
    {
        case AppController::ModelStatus::Ready:   modelTone = Colours::success; modelValue = controller.modelName(); break;
        case AppController::ModelStatus::Loading: modelTone = Colours::silver;  modelValue = "Loading " + controller.modelName(); break;
        case AppController::ModelStatus::Failed:  modelTone = Colours::error;   modelValue = "Failed to load"; break;
        case AppController::ModelStatus::NoModel: break;
    }
    rows.add ({ Icons::Id::Models, "Speech model", modelValue, modelTone });

    rows.add ({ Icons::Id::Keyboard, "Global shortcut",
                controller.isHotkeyRegistered() ? controller.hotkeyText()
                                                : controller.isHotkeyPaused() ? "Paused" : "Unavailable",
                controller.isHotkeyRegistered() ? Colours::success : Colours::warning });

    const auto& s = controller.getSettings();
    rows.add ({ Icons::Id::Insert, "Text insertion",
                ! s.autoInsert() ? "Manual (copy / insert)"
                                 : s.insertionMode() == InsertionMode::SimulatedTyping ? "Simulated typing" : "Automatic paste",
                Colours::success });
    return rows;
}

void OverviewPage::refresh()
{
    const auto& history = controller.getHistory();
    const bool recording = controller.state() == DictationState::Recording;
    const bool busy = isBusy (controller.state());
    const bool noModel = controller.modelStatus() == AppController::ModelStatus::NoModel
                      || controller.modelStatus() == AppController::ModelStatus::Failed;

    startButton.setButtonText (recording ? "Stop dictation" : busy ? "Transcribing..." : "Start dictation");
    startButton.setIcon (recording ? Icons::Id::Stop : Icons::Id::Mic);
    startButton.setEnabled (! busy && ! noModel);
    modelsButton.setVisible (noModel);

    if (history.isPersistenceEnabled())
    {
        const int count = history.totalDictations();
        dictations.setValue (count, [] (double v) { return juce::String (juce::roundToInt (v)); },
                             count == 0 ? "None yet" : "Saved on this PC");
        words.setValue ((double) history.totalWords(), [] (double v)
        {
            const int n = juce::roundToInt (v);
            return n >= 10000 ? juce::String (n / 1000.0, 1) + "k" : juce::String (n);
        }, count == 0 ? "Start dictating to see totals" : "Across all dictations");
        minutes.setValue (history.totalSeconds() / 60.0, [] (double v)
        {
            return v < 1.0 ? juce::String (juce::roundToInt (v * 60.0)) + " s" : juce::String (v, 1) + " min";
        }, controller.getSettings().saveRecordings() ? "Recordings are being saved (Settings > Privacy)" : "Audio is never stored");
    }
    else
    {
        for (auto* card : { &dictations, &words, &minutes })
            card->setValue (0, [] (double) { return juce::String ("--"); }, "History is off");
    }

    const bool hasRecent = ! history.entries().empty() || controller.session().transcript.isNotEmpty();
    copyButton.setEnabled (hasRecent);
    insertButton.setEnabled (hasRecent && ! busy && ! recording);
    resized();
    repaint();
}

void OverviewPage::resized()
{
    auto r = getLocalBounds().reduced (Metrics::pagePadding, Metrics::pagePadding - 8);
    const int gap = Metrics::gap;

    auto top = r.removeFromTop (juce::jlimit (210, 260, r.getHeight() * 38 / 100));
    readinessCard.setBounds (top.removeFromRight (juce::jmax (300, top.getWidth() * 36 / 100)));
    top.removeFromRight (gap);
    hero.setBounds (top);

    auto heroContent = hero.getLocalBounds().reduced (28);
    auto buttons = heroContent.removeFromBottom (44);
    startButton.setBounds (buttons.removeFromLeft (juce::jmax (180, startButton.getIdealWidth())));
    buttons.removeFromLeft (12);
    modelsButton.setBounds (buttons.removeFromLeft (modelsButton.getIdealWidth()));

    r.removeFromTop (gap);
    auto stats = r.removeFromTop (124);
    const int statWidth = (stats.getWidth() - gap * 2) / 3;
    dictations.setBounds (stats.removeFromLeft (statWidth));
    stats.removeFromLeft (gap);
    words.setBounds (stats.removeFromLeft (statWidth));
    stats.removeFromLeft (gap);
    minutes.setBounds (stats);

    r.removeFromTop (gap);
    activityCard.setBounds (r.removeFromRight (readinessCard.getWidth()));
    r.removeFromRight (gap);
    recentCard.setBounds (r);

    auto recentContent = recentCard.getContentBounds();
    auto header = recentCard.getLocalBounds().reduced (Metrics::cardPadding).removeFromTop (26);
    insertButton.setBounds (header.removeFromRight (32));
    header.removeFromRight (4);
    copyButton.setBounds (header.removeFromRight (32));
    recentTextArea = recentContent;
    activityArea = activityCard.getContentBounds();
}

void OverviewPage::paintOverChildren (juce::Graphics& g)
{
    // Hero copy (drawn here so the card stays a plain surface).
    {
        auto r = hero.getBounds().reduced (28);
        g.setColour (Colours::gold);
        g.setFont (Fonts::caption());
        g.drawText ("SYSTEM-WIDE VOICE TYPING", r.removeFromTop (16), juce::Justification::centredLeft);
        r.removeFromTop (8);

        g.setColour (Colours::text);
        g.setFont (Fonts::display());
        g.drawText ("Dictate into any app.", r.removeFromTop (40), juce::Justification::centredLeft, true);
        r.removeFromTop (6);

        g.setColour (Colours::textSecondary);
        g.setFont (Fonts::body());
        g.drawFittedText ("Put the cursor where you want text, press the shortcut, speak, and press it again. "
                          "Stilleto transcribes on this PC and types the result for you.",
                          r.removeFromTop (44), juce::Justification::topLeft, 2);

        // Shortcut keycaps beside the buttons.
        const auto shortcutOrigin = juce::Point<float> ((float) hero.getX() + 28.0f + juce::jmax (180, startButton.getIdealWidth()) + 24.0f
                                                            + (modelsButton.isVisible() ? modelsButton.getWidth() + 12.0f : 0.0f),
                                                        (float) hero.getBottom() - 28.0f - 35.0f);
        if (! modelsButton.isVisible())
        {
            g.setColour (Colours::textMuted);
            g.setFont (Fonts::small());
            g.drawText ("or press", juce::Rectangle<float> (shortcutOrigin.x, shortcutOrigin.y, 60.0f, 26.0f), juce::Justification::centredLeft);
            drawKeycaps (g, controller.hotkeyText(), shortcutOrigin.translated (62.0f, 0.0f), 26.0f);
        }
    }

    // Readiness rows.
    {
        auto r = readinessCard.getBounds().withTrimmedTop (Metrics::cardPadding + 50).reduced (Metrics::cardPadding, 0);
        const auto rows = readiness();
        const int rowHeight = juce::jmin (40, r.getHeight() / juce::jmax (1, rows.size()));

        for (int i = 0; i < rows.size(); ++i)
        {
            auto row = r.removeFromTop (rowHeight);
            if (i > 0)
            {
                g.setColour (Colours::border.withAlpha (0.6f));
                g.fillRect (row.getX(), row.getY(), row.getWidth(), 1);
            }

            const auto& item = rows.getReference (i);
            Icons::draw (g, item.icon, row.removeFromLeft (20).toFloat().withSizeKeepingCentre (16, 16), Colours::silver, 1.5f);
            row.removeFromLeft (10);

            g.setColour (item.tone);
            g.fillEllipse (row.removeFromRight (10).toFloat().withSizeKeepingCentre (7, 7));
            row.removeFromRight (8);

            g.setColour (Colours::textSecondary);
            g.setFont (Fonts::small());
            g.drawText (item.label, row, juce::Justification::centredLeft);
            g.setColour (Colours::text);
            g.setFont (Fonts::make (12.5f, "Semibold"));
            g.drawText (item.value, row.withTrimmedLeft (110), juce::Justification::centredRight, true);
        }
    }

    // Latest transcription.
    {
        auto r = recentTextArea + recentCard.getPosition();
        const auto& entries = controller.getHistory().entries();
        const bool hasEntry = ! entries.empty();
        const auto text = hasEntry ? entries.front().text : controller.session().transcript;

        if (text.isEmpty())
        {
            g.setColour (Colours::textMuted);
            g.setFont (Fonts::body());
            g.drawFittedText ("Your most recent dictation will appear here.", r, juce::Justification::centred, 2);
        }
        else
        {
            auto meta = r.removeFromBottom (20);
            g.setColour (Colours::text.withAlpha (0.92f));
            g.setFont (Fonts::body());
            g.drawFittedText (text, r.reduced (0, 4), juce::Justification::topLeft, juce::jmax (1, r.getHeight() / 20), 1.0f);

            if (copyFeedback.isNotEmpty() && juce::Time::getMillisecondCounter() < copyFeedbackUntil)
            {
                g.setColour (copyFeedbackIsError ? Colours::error : Colours::success);
                g.setFont (Fonts::small());
                g.drawText (copyFeedback, meta, juce::Justification::centredLeft, true);
            }
            else if (hasEntry)
            {
                const auto& e = entries.front();
                g.setColour (Colours::textMuted);
                g.setFont (Fonts::small());
                g.drawText (relativeTime (e.timestamp) + "  " + ui::middot() + "  " + juce::String (e.wordCount) + " words  " + ui::middot() + "  " + e.insertionNote,
                            meta, juce::Justification::centredLeft, true);
            }
        }
    }

    // Recent activity.
    {
        auto r = activityArea + activityCard.getPosition();
        const auto& entries = controller.getHistory().entries();

        if (! controller.getHistory().isPersistenceEnabled())
        {
            g.setColour (Colours::textMuted);
            g.setFont (Fonts::body());
            g.drawFittedText ("History is turned off in Settings > Privacy.", r, juce::Justification::centred, 2);
        }
        else if (entries.empty())
        {
            g.setColour (Colours::textMuted);
            g.setFont (Fonts::body());
            g.drawFittedText ("No dictations yet. Try it in Notepad: press " + controller.hotkeyText() + ".",
                              r, juce::Justification::centred, 3);
        }
        else
        {
            const int rowHeight = 46;
            for (size_t i = 0; i < entries.size() && r.getHeight() >= rowHeight; ++i)
            {
                auto row = r.removeFromTop (rowHeight);
                const auto& e = entries[i];

                g.setColour (e.inserted ? Colours::success : Colours::silver.withAlpha (0.5f));
                g.fillEllipse (row.removeFromLeft (12).toFloat().withSizeKeepingCentre (6, 6).translated (0, -8));
                row.removeFromLeft (6);

                g.setColour (Colours::text);
                g.setFont (Fonts::small());
                g.drawText (e.text, row.removeFromTop (22), juce::Justification::bottomLeft, true);
                g.setColour (Colours::textMuted);
                g.setFont (Fonts::make (11.0f));
                g.drawText (relativeTime (e.timestamp) + "  " + ui::middot() + "  " + juce::String (e.wordCount) + " words", row.removeFromTop (18),
                            juce::Justification::topLeft, true);
            }
        }
    }
}

} // namespace stilleto
