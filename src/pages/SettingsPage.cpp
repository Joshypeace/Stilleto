#include "pages/SettingsPage.h"

#include <thread>

namespace stilleto
{
using namespace ui;

namespace
{
struct Language { const char* code; const char* name; };
constexpr Language languages[] = {
    { "en", "English" }, { "auto", "Detect automatically" }, { "es", "Spanish" }, { "fr", "French" },
    { "de", "German" }, { "it", "Italian" }, { "pt", "Portuguese" }, { "nl", "Dutch" }, { "pl", "Polish" },
    { "ru", "Russian" }, { "uk", "Ukrainian" }, { "tr", "Turkish" }, { "ar", "Arabic" }, { "hi", "Hindi" },
    { "sw", "Swahili" }, { "zh", "Chinese" }, { "ja", "Japanese" }, { "ko", "Korean" },
};

constexpr int recordingLimits[] = { 60, 120, 300, 600, 1800 };
}

/** A titled group of rows. Each row: label, description, control on the right. */
class SettingsPage::Section : public Card
{
public:
    struct Row
    {
        juce::String title, description;
        juce::Component* control = nullptr;
        int controlWidth = 46;
        int height = 64;
    };

    Section (const juce::String& title, const juce::String& subtitle, Icons::Id icon) : Card (title, subtitle)
    {
        setIcon (icon);
    }

    void addRow (Row row)
    {
        if (row.control != nullptr)
            addAndMakeVisible (row.control);
        rows.push_back (std::move (row));
    }

    int layout (int width)
    {
        const int top = Metrics::cardPadding + 50;
        int y = top;
        for (auto& row : rows)
        {
            if (row.control != nullptr)
            {
                const int cw = juce::jmin (row.controlWidth, width / 2);
                const int ch = row.height > 90 ? row.height - 20 : juce::jmin (38, row.height - 16);
                row.control->setBounds (width - Metrics::cardPadding - cw, y + (row.height > 90 ? 10 : (row.height - ch) / 2), cw, ch);
            }
            y += row.height;
        }
        setSize (width, y + Metrics::cardPadding - 6);
        return getHeight();
    }

    void paintOverChildren (juce::Graphics& g) override
    {
        int y = Metrics::cardPadding + 50;
        for (size_t i = 0; i < rows.size(); ++i)
        {
            const auto& row = rows[i];
            const int controlLeft = row.control != nullptr ? row.control->getX() - 24 : getWidth() - Metrics::cardPadding;
            auto area = juce::Rectangle<int> (Metrics::cardPadding, y, controlLeft - Metrics::cardPadding, row.height);

            if (i > 0)
            {
                g.setColour (Colours::border.withAlpha (0.55f));
                g.fillRect (Metrics::cardPadding, y, getWidth() - Metrics::cardPadding * 2, 1);
            }

            const int textTop = row.height > 90 ? y + 12 : y + (row.height - 40) / 2;
            g.setColour (Colours::text);
            g.setFont (Fonts::body());
            g.drawText (row.title, area.getX(), textTop, area.getWidth(), 20, juce::Justification::centredLeft, true);
            g.setColour (Colours::textMuted);
            g.setFont (Fonts::small());
            g.drawFittedText (row.description, area.getX(), textTop + 21, area.getWidth(), row.height > 90 ? 36 : 20,
                              juce::Justification::topLeft, row.height > 90 ? 2 : 1);
            y += row.height;
        }
    }

private:
    std::vector<Row> rows;
};

/** Level meter shown while testing the microphone. */
class LevelMeter : public juce::Component
{
public:
    float level = 0.0f;
    bool active = false;

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat().withSizeKeepingCentre ((float) getWidth(), 8.0f);
        g.setColour (Colours::background2);
        g.fillRoundedRectangle (r, 4.0f);
        if (active)
        {
            g.setGradientFill (goldGradient (r));
            g.fillRoundedRectangle (r.withWidth (r.getWidth() * juce::jlimit (0.0f, 1.0f, level)), 4.0f);
        }
        g.setColour (Colours::border);
        g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
    }
};

class SettingsPage::Content : public juce::Component
{
public:
    Section general { "General", "Startup, window and motion", Icons::Id::Overview };
    Section shortcut { "Keyboard shortcut", "Starts and stops dictation from any application", Icons::Id::Keyboard };
    Section audio { "Audio", "Microphone selection and diagnostics", Icons::Id::Mic };
    Section transcription { "Transcription", "Local Whisper engine", Icons::Id::Chip };
    Section insertion { "Text insertion", "How transcripts reach the target application", Icons::Id::Insert };
    Section privacy { "Privacy", "What is stored on this PC", Icons::Id::Shield };

    ToggleSwitch launchAtStartup, minimiseToTray, closeToTray, reducedMotion;
    SegmentedControl widgetMode { { "While dictating", "Always", "Never" } };

    HotkeyEditor hotkeyEditor;
    StilletoButton restoreHotkey { "Restore default", StilletoButton::Style::Ghost };

    juce::ComboBox microphone, maxLength;
    StilletoButton testMic { "Test microphone", StilletoButton::Style::Secondary, Icons::Id::Mic };
    LevelMeter meter;
    juce::Label deviceStatus;

    juce::ComboBox model, language, threads;
    ToggleSwitch gpu;
    juce::Array<juce::File> modelFiles;   // combo item id - 1 -> file

    ToggleSwitch autoInsert, trailingSpace, paragraphs, tidy;
    SegmentedControl method { { "Automatic", "Paste", "Typing" } };
    juce::Slider keystrokeDelay { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };

    ToggleSwitch saveHistory, saveRecordings, recordingIndicator;
    StilletoButton clearHistory { "Clear transcripts", StilletoButton::Style::Danger, Icons::Id::Trash };

    Content()
    {
        for (auto* s : { &general, &shortcut, &audio, &transcription, &insertion, &privacy })
            addAndMakeVisible (s);

        general.addRow ({ "Launch at Windows sign-in", "Start minimised to the system tray when you sign in.", &launchAtStartup });
        general.addRow ({ "Minimise to tray", "The minimise button hides Stilleto to the notification area.", &minimiseToTray });
        general.addRow ({ "Close to tray", "Closing the window keeps the shortcut working in the background.", &closeToTray });
        general.addRow ({ "Floating widget", "The compact status pill shown while you dictate into other apps.", &widgetMode, 330 });
        general.addRow ({ "Reduce motion", "Minimise animations. Windows' own animation setting is also respected.", &reducedMotion });

        shortcut.addRow ({ "Dictation shortcut", "Press once to start recording and again to transcribe and insert.", &hotkeyEditor, 420, 110 });
        shortcut.addRow ({ "Default", "Ctrl + Alt + Space", &restoreHotkey, 150 });

        audio.addRow ({ "Microphone", "Recorded at the device's native rate, converted to 16 kHz for Whisper.", &microphone, 320 });
        audio.addRow ({ "Input level", "Speak to check the level. The microphone is only open while testing or dictating.", &testMic, 170 });
        audio.addRow ({ "", "", &meter, 320, 40 });
        audio.addRow ({ "Device status", "", &deviceStatus, 320 });
        audio.addRow ({ "Maximum recording length", "Recording stops and transcribes automatically at this length.", &maxLength, 200 });

        transcription.addRow ({ "Active model", "Manage models and downloads on the Models page.", &model, 320 });
        transcription.addRow ({ "Language", "English-only (.en) models always transcribe English.", &language, 240 });
        transcription.addRow ({ "CPU threads", "More threads transcribe faster but use more of the CPU.", &threads, 200 });
        transcription.addRow ({ "GPU acceleration", "This build uses whisper.cpp's CPU backend. Rebuild with STILLETO_GPU_VULKAN=ON to enable GPU support.", &gpu });

        insertion.addRow ({ "Insert automatically", "Type the transcript into the window that was active when you pressed the shortcut.", &autoInsert });
        insertion.addRow ({ "Method", "Automatic pastes via the clipboard and restores it; Typing simulates keystrokes for apps that block paste.", &method, 300, 76 });
        insertion.addRow ({ "Keystroke delay", "Pause between simulated keystrokes (Typing method). Increase it if characters are dropped.", &keystrokeDelay, 300, 76 });
        insertion.addRow ({ "Add a trailing space", "So consecutive dictations do not run together.", &trailingSpace });
        insertion.addRow ({ "Paragraph per segment", "Keep Whisper's segment breaks as new lines.", &paragraphs });
        insertion.addRow ({ "Tidy punctuation", "Fix spacing around punctuation and capitalise the first word.", &tidy });

        privacy.addRow ({ "Save transcription history", "Keep transcripts in %APPDATA%\\Stilleto so you can search and reuse them.", &saveHistory });
        privacy.addRow ({ "Save raw recordings", "Off by default. When on, each dictation is also saved as a WAV file in %APPDATA%\\Stilleto\\recordings.", &saveRecordings });
        privacy.addRow ({ "Show recording indicator", "Always show a recording indicator, even if the floating widget is set to Never.", &recordingIndicator });
        privacy.addRow ({ "Stored transcripts", "Permanently delete every saved transcript from this PC.", &clearHistory, 190 });

        styleLabel (deviceStatus, Fonts::small(), Colours::textSecondary, juce::Justification::centredRight);

        keystrokeDelay.setRange (5.0, 200.0, 1.0);
        keystrokeDelay.setTextValueSuffix (" ms");
        keystrokeDelay.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 24);

        for (const auto seconds : recordingLimits)
            maxLength.addItem (juce::String (seconds / 60) + (seconds == 60 ? " minute" : " minutes"), seconds);

        for (int i = 0; i < (int) std::size (languages); ++i)
            language.addItem (languages[i].name, i + 1);

        const int hw = juce::jmax (1, (int) std::thread::hardware_concurrency());
        threads.addItem ("Automatic (" + juce::String (WhisperEngine::defaultThreadCount()) + ")", 1000);
        for (int t = 1; t <= juce::jmin (hw, 16); ++t)
            threads.addItem (juce::String (t), t);

        gpu.setEnabled (false);
    }

    int layout (int width)
    {
        int y = 0;
        for (auto* s : { &general, &shortcut, &audio, &transcription, &insertion, &privacy })
        {
            s->setTopLeftPosition (0, y);
            y += s->layout (width) + Metrics::gap;
        }
        return y;
    }
};

//==============================================================================================
SettingsPage::SettingsPage (AppController& c) : Page (c), content (std::make_unique<Content>())
{
    viewport.setViewedComponent (content.get(), false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);

    auto& s = controller.getSettings();
    auto& ui = *content;

    auto bindToggle = [this] (ToggleSwitch& t, std::function<void (bool)> setter)
    {
        t.onClick = [this, &t, setter] { if (! loading) setter (t.getToggleState()); };
    };

    bindToggle (ui.launchAtStartup, [&s] (bool v) { s.setLaunchAtStartup (v); });
    bindToggle (ui.minimiseToTray,  [&s] (bool v) { s.setMinimiseToTray (v); });
    bindToggle (ui.closeToTray,     [&s] (bool v) { s.setCloseToTray (v); });
    bindToggle (ui.reducedMotion,   [&s] (bool v) { s.setReducedMotion (v); });
    bindToggle (ui.autoInsert,      [&s] (bool v) { s.setAutoInsert (v); });
    bindToggle (ui.trailingSpace,   [&s] (bool v) { s.setTrailingSpace (v); });
    bindToggle (ui.paragraphs,      [&s] (bool v) { s.setParagraphPerSegment (v); });
    bindToggle (ui.tidy,            [&s] (bool v) { s.setTidyPunctuation (v); });
    bindToggle (ui.saveRecordings,  [&s] (bool v) { s.setSaveRecordings (v); });
    bindToggle (ui.recordingIndicator, [&s] (bool v) { s.setRecordingIndicator (v); });

    ui.saveHistory.onClick = [this, &s]
    {
        if (loading) return;
        if (content->saveHistory.getToggleState())
        {
            s.setSaveHistory (true);
            return;
        }

        // Turning history off: ask whether existing transcripts should also be deleted.
        s.setSaveHistory (false);
        if (! controller.getHistory().entries().empty())
            confirmClearHistory();
    };

    ui.widgetMode.onChange = [this, &s] (int i) { if (! loading) s.setWidgetVisibility (static_cast<WidgetVisibility> (i)); };
    ui.method.onChange = [this, &s] (int i) { if (! loading) s.setInsertionMode (static_cast<InsertionMode> (i)); };
    ui.keystrokeDelay.onValueChange = [this, &s] { if (! loading) s.setKeystrokeDelayMs ((int) content->keystrokeDelay.getValue()); };

    ui.hotkeyEditor.onCaptureStateChanged = [this] (bool capturing) { controller.setHotkeyCaptureActive (capturing); };
    ui.hotkeyEditor.onShortcutChosen = [this] (const HotkeySpec& spec)
    {
        juce::String error;
        if (controller.applyHotkey (spec, error))
            content->hotkeyEditor.setMessage ("Saved. " + juce::String (spec.toDisplayString()) + " now works in every application.", false);
        else
            content->hotkeyEditor.setMessage (error, true);
        content->hotkeyEditor.setShortcut (controller.getSettings().hotkey());
    };
    ui.restoreHotkey.onClick = [this]
    {
        juce::String error;
        const bool ok = controller.applyHotkey (HotkeySpec::defaultSpec(), error);
        content->hotkeyEditor.setMessage (ok ? "Restored Ctrl + Alt + Space." : error, ! ok);
        content->hotkeyEditor.setShortcut (controller.getSettings().hotkey());
    };

    ui.microphone.onChange = [this, &s]
    {
        if (loading) return;
        const int id = content->microphone.getSelectedId();
        s.setInputDevice (id <= 1 ? juce::String() : content->microphone.getText());
        if (controller.getRecorder().isDeviceOpen() && controller.state() != DictationState::Recording)
        {
            setMonitoring (false);   // reopen on the new device next time
        }
    };
    ui.testMic.onClick = [this] { setMonitoring (! content->meter.active); };
    ui.maxLength.onChange = [this, &s] { if (! loading) s.setMaxRecordingSeconds (content->maxLength.getSelectedId()); };

    ui.model.onChange = [this]
    {
        if (loading) return;
        const int index = content->model.getSelectedId() - 1;
        if (juce::isPositiveAndBelow (index, content->modelFiles.size()))
            controller.activateModel (content->modelFiles[index]);
    };
    ui.language.onChange = [this, &s]
    {
        if (loading) return;
        const int index = content->language.getSelectedId() - 1;
        if (juce::isPositiveAndBelow (index, (int) std::size (languages)))
            s.setLanguage (languages[index].code);
    };
    ui.threads.onChange = [this, &s]
    {
        if (loading) return;
        const int id = content->threads.getSelectedId();
        s.setThreads (id == 1000 ? 0 : id);
    };

    ui.clearHistory.onClick = [this] { confirmClearHistory(); };

    controller.addListener (this);
    controller.getSettings().addListener (this);
    loadValues();
}

SettingsPage::~SettingsPage()
{
    alive->store (false);
    setMonitoring (false);
    controller.removeListener (this);
    controller.getSettings().removeListener (this);
}

void SettingsPage::pageShown()
{
    controller.refreshMicrophones();
    loadValues();
}

void SettingsPage::pageHidden()
{
    setMonitoring (false);
}

void SettingsPage::loadValues()
{
    const juce::ScopedValueSetter<bool> guard (loading, true);
    auto& s = controller.getSettings();
    auto& ui = *content;

    ui.launchAtStartup.setToggleState (s.launchAtStartup(), juce::dontSendNotification);
    ui.minimiseToTray.setToggleState (s.minimiseToTray(), juce::dontSendNotification);
    ui.closeToTray.setToggleState (s.closeToTray(), juce::dontSendNotification);
    ui.reducedMotion.setToggleState (s.reducedMotion(), juce::dontSendNotification);
    ui.widgetMode.setSelectedIndex (static_cast<int> (s.widgetVisibility()));

    ui.hotkeyEditor.setShortcut (s.hotkey());
    hotkeyStatusChanged();

    // Microphones
    ui.microphone.clear (juce::dontSendNotification);
    ui.microphone.addItem ("System default", 1);
    int id = 2, selected = 1;
    for (const auto& name : controller.microphones())
    {
        ui.microphone.addItem (name, id);
        if (name == s.inputDevice()) selected = id;
        ++id;
    }
    ui.microphone.setSelectedId (selected, juce::dontSendNotification);
    ui.microphone.setEnabled (controller.microphoneAvailable());
    ui.maxLength.setSelectedId (s.maxRecordingSeconds(), juce::dontSendNotification);
    if (ui.maxLength.getSelectedId() == 0) ui.maxLength.setSelectedId (300, juce::dontSendNotification);

    if (! controller.microphoneAvailable())
        ui.deviceStatus.setText ("No microphone detected", juce::dontSendNotification);
    else if (! controller.getRecorder().isDeviceOpen())
        ui.deviceStatus.setText (juce::String (controller.microphones().size()) + " input device(s) available", juce::dontSendNotification);

    // Models
    ui.model.clear (juce::dontSendNotification);
    ui.modelFiles.clear();
    int modelId = 1;
    for (const auto& m : controller.getModels().installed())
    {
        if (m.status != ModelFileStatus::Valid) continue;
        ui.modelFiles.add (m.file);
        ui.model.addItem (m.displayName + "  ·  " + m.file.getFileName(), modelId);
        if (m.file == controller.activeModelFile())
            ui.model.setSelectedId (modelId, juce::dontSendNotification);
        ++modelId;
    }
    if (modelId == 1)
    {
        ui.model.addItem ("No models installed", 1);
        ui.model.setSelectedId (1, juce::dontSendNotification);
        ui.model.setEnabled (false);
    }
    else
    {
        ui.model.setEnabled (true);
    }

    for (int i = 0; i < (int) std::size (languages); ++i)
        if (s.language() == languages[i].code)
            ui.language.setSelectedId (i + 1, juce::dontSendNotification);
    ui.threads.setSelectedId (s.threads() == 0 ? 1000 : s.threads(), juce::dontSendNotification);
    ui.gpu.setToggleState (false, juce::dontSendNotification);

    ui.autoInsert.setToggleState (s.autoInsert(), juce::dontSendNotification);
    ui.method.setSelectedIndex (static_cast<int> (s.insertionMode()));
    ui.keystrokeDelay.setValue (s.keystrokeDelayMs(), juce::dontSendNotification);
    ui.keystrokeDelay.setEnabled (s.insertionMode() == InsertionMode::SimulatedTyping);
    ui.trailingSpace.setToggleState (s.trailingSpace(), juce::dontSendNotification);
    ui.paragraphs.setToggleState (s.paragraphPerSegment(), juce::dontSendNotification);
    ui.tidy.setToggleState (s.tidyPunctuation(), juce::dontSendNotification);

    ui.saveHistory.setToggleState (s.saveHistory(), juce::dontSendNotification);
    ui.saveRecordings.setToggleState (s.saveRecordings(), juce::dontSendNotification);
    ui.recordingIndicator.setToggleState (s.recordingIndicator(), juce::dontSendNotification);
    ui.clearHistory.setEnabled (! controller.getHistory().entries().empty());
}

void SettingsPage::hotkeyStatusChanged()
{
    auto& editor = content->hotkeyEditor;
    if (controller.isHotkeyPaused())
        editor.setMessage ("Paused from the tray menu. Resume it there to dictate with the shortcut.", false);
    else if (controller.hotkeyError().isNotEmpty())
        editor.setMessage (controller.hotkeyError(), true);
}

void SettingsPage::setMonitoring (bool on)
{
    auto& recorder = controller.getRecorder();

    if (on && controller.state() != DictationState::Recording)
    {
        juce::String error;
        if (recorder.startMonitoring (error))
        {
            content->testMic.setButtonText ("Stop test");
            content->meter.active = true;
            content->deviceStatus.setText (recorder.currentDeviceName() + "  ·  " + juce::String (recorder.currentSampleRate(), 0) + " Hz",
                                           juce::dontSendNotification);
            startTimerHz (30);
        }
        else
        {
            content->deviceStatus.setText (error, juce::dontSendNotification);
            content->deviceStatus.setColour (juce::Label::textColourId, Colours::error);
        }
        return;
    }

    recorder.stopMonitoring();
    stopTimer();
    content->testMic.setButtonText ("Test microphone");
    content->meter.active = false;
    content->meter.level = 0.0f;
    content->meter.repaint();
    content->deviceStatus.setColour (juce::Label::textColourId, Colours::textSecondary);
}

void SettingsPage::timerCallback()
{
    content->meter.level = controller.getRecorder().currentLevel();
    content->meter.repaint();
}

void SettingsPage::confirmClearHistory()
{
    const int count = controller.getHistory().totalDictations();
    if (count == 0) return;

    juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                      .withIconType (juce::MessageBoxIconType::WarningIcon)
                                      .withTitle ("Delete stored transcripts?")
                                      .withMessage ("Delete all " + juce::String (count) + " saved transcripts from this PC? This cannot be undone.")
                                      .withButton ("Delete")
                                      .withButton ("Keep them")
                                      .withAssociatedComponent (this),
                                  [this, token = alive] (int r)
                                  {
                                      if (token->load() && r == 1)
                                      {
                                          controller.getHistory().clear();
                                          loadValues();
                                      }
                                  });
}

void SettingsPage::resized()
{
    viewport.setBounds (getLocalBounds().reduced (Metrics::pagePadding - 4, Metrics::pagePadding - 8));
    const int width = juce::jmin (920, viewport.getWidth() - viewport.getScrollBarThickness() - 8);
    content->setSize (width, content->layout (width));
}

} // namespace stilleto
