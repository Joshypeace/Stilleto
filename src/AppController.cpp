#include "AppController.h"

#include "MainWindow.h"
#include "components/RecordingWidget.h"
#include "core/TextProcessing.h"
#include "theme/Animation.h"
#include "windows/NativeWindow.h"
#include "windows/TrayIcon.h"

#include <juce_audio_formats/juce_audio_formats.h>

namespace stilleto
{
namespace
{
constexpr int completedLingerMs = 4000;
constexpr int errorLingerMs = 6500;
constexpr double minimumRecordingSeconds = 0.3;

juce::String toJuce (const std::string& utf8) { return juce::String::fromUTF8 (utf8.c_str(), static_cast<int> (utf8.size())); }
std::u16string toUtf16 (const juce::String& s) { return text::utf8ToUtf16 (s.toStdString()); }

void log (const juce::String& message) { juce::Logger::writeToLog ("[Stilleto] " + message); }
} // namespace

AppController::AppController()
    : history (SettingsManager::appDataDirectory().getChildFile ("history.json")),
      models (settings.modelDirectory())
{
    settings.addListener (this);
    models.addListener (this);

    WhisperEngine::setLogHandler ([] (const std::string& line) { log ("whisper: " + toJuce (line).trimEnd()); });

    history.setPersistenceEnabled (settings.saveHistory());
    juce::String historyError;
    if (! history.load (historyError))
        log (historyError);

    recorder.setPreferredDevice (settings.inputDevice());
    applyMotionPreference();
}

AppController::~AppController()
{
    shutdown();
}

void AppController::initialise (bool startHidden)
{
    foreground = std::make_unique<ForegroundTracker>();

    refreshMicrophones();
    models.rescan();   // may already select a model through modelsChanged()
    if (activeModel == juce::File())
        loadActiveModel();
    registerHotkeyFromSettings();

    // Keep the Run entry in sync with the setting (the user may have removed it elsewhere).
    if (settings.launchAtStartup() != native::isLaunchAtStartupEnabled (L"Stilleto"))
        native::setLaunchAtStartup (L"Stilleto", settings.launchAtStartup(),
                                    L"\"" + native::currentExecutablePath() + L"\" --minimized");

    tray = std::make_unique<TrayIcon> (
        TrayIcon::Actions {
            [this] { showMainWindow(); },
            [this] { toggleDictation (Source::Tray); },
            [this] { showMainWindow (Page::Settings); },
            [this] { toggleHotkeyPause(); },
            [this] { requestQuit(); } },
        [this]
        {
            TrayIcon::MenuState s;
            s.recording = current.state == DictationState::Recording;
            s.busy = isBusy (current.state);
            s.hotkeyPaused = hotkey.isPaused();
            s.hotkeyText = hotkeyText();
            return s;
        });

    widget = std::make_unique<RecordingWidget> (*this);
    mainWindow = std::make_unique<MainWindow> (*this);

    if (! startHidden)
        showMainWindow();

    updateTray();
    updateWidget();
    startTimerHz (10);
}

void AppController::renderSnapshots (const juce::File& directory, std::function<void()> onDone)
{
    snapshotMode = true;
    ui::Motion::setReduced (true);   // render final states, not mid-animation frames
    refreshMicrophones();
    models.rescan();
    if (activeModel == juce::File())
        loadActiveModel();

    mainWindow = std::make_unique<MainWindow> (*this);
    mainWindow->setSize (1280, 800);
    directory.createDirectory();

    // Asynchronous so the model can finish loading and pages can lay out between captures.
    auto step = std::make_shared<std::function<void (int)>>();
    *step = [this, directory, onDone, step, token = alive] (int index)
    {
        if (! token->load()) return;

        static const char* names[] = { "overview", "dictation", "history", "models", "settings", "about" };
        if (index >= 6) { onDone(); return; }

        if (index == 0 && modelState == ModelStatus::Loading)
        {
            juce::Timer::callAfterDelay (100, [step] { (*step) (0); });
            return;
        }

        mainWindow->showPage (static_cast<Page> (index));
        juce::Timer::callAfterDelay (250, [this, directory, step, index, token]
        {
            if (! token->load()) return;
            auto* content = mainWindow->getContentComponent();
            const auto image = content->createComponentSnapshot (content->getLocalBounds(), true, 1.0f);
            const auto file = directory.getChildFile (juce::String (names[index]) + ".png");
            file.deleteFile();
            if (auto stream = file.createOutputStream())
                juce::PNGImageFormat().writeImageToStream (image, *stream);
            (*step) (index + 1);
        });
    };
    (*step) (0);
}

void AppController::shutdown()
{
    if (shuttingDown)
        return;
    shuttingDown = true;
    alive->store (false);
    stopTimer();

    if (current.state == DictationState::Recording)
        recorder.cancelRecording();

    // Stop the workers first so no further callbacks are produced. Insertion checks the cancel
    // flag between keystroke batches, so draining the pool never needs a forced thread kill.
    insertionCancel->store (true);
    transcriber.shutdown();
    insertionPool.removeAllJobs (false, -1);
    recorder.shutdown();
    hotkey.unregister();
    foreground.reset();

    if (mainWindow != nullptr && ! snapshotMode)
        settings.setWindowState (mainWindow->getWindowStateAsString());

    widget.reset();
    mainWindow.reset();
    tray.reset();

    history.flushNow();
    settings.flush();

    settings.removeListener (this);
    models.removeListener (this);
    WhisperEngine::setLogHandler ({});
}

//==============================================================================================
// State machine

bool AppController::transitionTo (DictationState next)
{
    if (! isValidTransition (current.state, next))
    {
        log ("Rejected transition " + juce::String (std::string (toString (current.state)))
             + " -> " + juce::String (std::string (toString (next))));
        return false;
    }

    current.state = next;
    stateEnteredAt = juce::Time::getMillisecondCounter();
    return true;
}

void AppController::notifySession()
{
    updateTray();
    updateWidget();
    listeners.call ([] (Listener& l) { l.sessionChanged(); });
}

void AppController::setError (const juce::String& headline, const juce::String& detail)
{
    if (current.state == DictationState::Recording)
        recorder.cancelRecording();

    // Error is reachable from every state, so this transition cannot be rejected.
    transitionTo (DictationState::Error);
    current.headline = headline;
    current.detail = detail;
    log ("Error: " + headline + " - " + detail);
    notifySession();
}

void AppController::toggleDictation (Source source)
{
    if (current.state == DictationState::Recording)
        stopDictation();
    else
        startDictation (source);
}

void AppController::startDictation (Source source)
{
    if (shuttingDown)
        return;

    if (isBusy (current.state))
    {
        // A transcription or insertion is still running: never start an overlapping recording.
        current.detail = current.state == DictationState::Processing ? "Still transcribing the previous dictation..."
                                                                     : "Still inserting the previous dictation...";
        notifySession();
        return;
    }

    if (! canStartRecording (current.state))
        return;

    if (modelState == ModelStatus::NoModel || modelState == ModelStatus::Failed)
    {
        setError ("No speech model", modelState == ModelStatus::Failed && modelProblem.isNotEmpty()
                                         ? modelProblem
                                         : "Download or choose a Whisper model on the Models page before dictating.");
        return;
    }

    // Capture the destination before anything can change focus.
    switch (source)
    {
        case Source::Hotkey:
        case Source::Widget:
            pendingTarget = TextInserter::captureForeground();
            break;
        case Source::Tray:
        case Source::MainWindow:
            pendingTarget = foreground != nullptr ? TextInserter::targetFromWindow (foreground->lastExternalWindow()) : InsertionTarget {};
            break;
    }

    juce::String error;
    if (! recorder.beginRecording (error))
    {
        refreshMicrophones();
        setError (microphoneAvailable() ? "Microphone unavailable" : "No microphone found", error);
        return;
    }

    if (! transitionTo (DictationState::Recording))
    {
        recorder.cancelRecording();
        return;
    }

    cancelRequested = false;
    lastCapturedSamples = 0;
    lastCaptureProgressAt = juce::Time::getMillisecondCounter();
    current.source = source;
    current.headline = "Listening";
    current.detail = "Press " + hotkeyText() + " again to finish";
    current.targetName = pendingTarget.isValid() ? juce::String (pendingTarget.describe()) : juce::String();
    current.recordedSeconds = 0.0;
    log ("Recording started (" + recorder.currentDeviceName() + ", " + juce::String (recorder.currentSampleRate(), 0) + " Hz)");
    notifySession();
}

void AppController::stopDictation()
{
    if (current.state != DictationState::Recording)
        return;

    auto recording = recorder.endRecording();
    current.recordedSeconds = recording.seconds();
    log ("Recording stopped: " + juce::String (recording.seconds(), 2) + " s at " + juce::String (recording.sampleRate, 0)
         + " Hz, peak " + juce::String (recording.peak, 4) + ", rms " + juce::String (recording.rms, 5)
         + (recording.overflowed ? ", FIFO OVERFLOW" : ""));

    // Developer test hook for automated end-to-end runs without a human speaker: when
    // STILLETO_TEST_AUDIO names a WAV file, it replaces the captured audio after the real
    // microphone capture above. Never set in normal use.
   #if STILLETO_ENABLE_TEST_HOOKS   // compiled only with -DSTILLETO_TEST_HOOKS=ON
    if (const auto testAudio = juce::SystemStats::getEnvironmentVariable ("STILLETO_TEST_AUDIO", {}); testAudio.isNotEmpty())
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        if (std::unique_ptr<juce::AudioFormatReader> reader { formats.createReaderFor (juce::File (testAudio)) })
        {
            juce::AudioBuffer<float> buffer ((int) reader->numChannels, (int) reader->lengthInSamples);
            reader->read (&buffer, 0, (int) reader->lengthInSamples, 0, true, true);
            recording.samples.assign (buffer.getReadPointer (0), buffer.getReadPointer (0) + buffer.getNumSamples());
            recording.sampleRate = reader->sampleRate;
            current.recordedSeconds = recording.seconds();
            log ("TEST HOOK: substituted " + testAudio + " (" + juce::String (recording.seconds(), 2) + " s)");
        }
    }
   #endif

    if (recording.samples.empty() || recording.seconds() < minimumRecordingSeconds)
    {
        setError ("Recording too short", "Hold on a moment longer before stopping. Nothing was captured.");
        return;
    }

    if (! transitionTo (DictationState::Processing))
        return;

    current.headline = "Transcribing";
    current.detail = "Running " + modelName() + " on this device";
    notifySession();

    WhisperTranscriber::Job job;
    job.sampleRate = recording.sampleRate;
    job.samples = std::move (recording.samples);
    job.options.language = settings.language().toStdString();
    job.options.threads = settings.threads();

    if (settings.saveRecordings())
    {
        job.saveWavTo = true;
        job.wavFile = recordingsDirectory()
                          .getChildFile ("dictation-" + juce::Time::getCurrentTime().formatted ("%Y%m%d-%H%M%S") + ".wav");
    }

    const bool queued = transcriber.transcribe (std::move (job), [this, token = alive] (const WhisperTranscriber::Result& r)
    {
        if (token->load())
            handleTranscription (r);
    });

    if (! queued)
        setError ("Transcription busy", "The previous transcription has not finished yet.");
}

void AppController::cancelDictation()
{
    switch (current.state)
    {
        case DictationState::Recording:
            recorder.cancelRecording();
            transitionTo (DictationState::Idle);
            current.headline = "Cancelled";
            current.detail.clear();
            notifySession();
            break;

        case DictationState::Processing:
            cancelRequested = true;              // wins even if a successful result is already queued
            transcriber.cancelTranscription();   // the result callback moves to Idle
            current.detail = "Cancelling...";
            notifySession();
            break;

        case DictationState::Completed:
        case DictationState::Error:
            transitionTo (DictationState::Idle);
            notifySession();
            break;

        case DictationState::Idle:
        case DictationState::Inserting:   // insertion is short and cannot be interrupted safely
            break;
    }
}

void AppController::handleTranscription (const WhisperTranscriber::Result& r)
{
    if (current.state != DictationState::Processing)
        return;

    using Status = WhisperTranscriber::Result::Status;

    if (r.status == Status::Cancelled || cancelRequested)
    {
        transitionTo (DictationState::Idle);
        current.headline = "Cancelled";
        current.detail.clear();
        notifySession();
        return;
    }

    if (r.status == Status::NoSpeech)
    {
        setError ("No speech detected", toJuce (r.error));
        return;
    }

    if (r.status == Status::Failed)
    {
        setError ("Transcription failed", toJuce (r.error));
        return;
    }

    text::CleanupOptions cleanup;
    cleanup.tidyPunctuation = settings.tidyPunctuation();
    cleanup.paragraphPerSegment = settings.paragraphPerSegment();
    // The filler and stammer rules are English ("um" and "er" are real words in German and Dutch).
    cleanup.removeDisfluencies = settings.removeDisfluencies() && (r.language.empty() || r.language == "en");
    const auto cleaned = toJuce (text::cleanTranscript (r.rawText, cleanup));

    if (cleaned.isEmpty())
    {
        setError ("No speech detected", "Whisper did not recognise any words in the recording.");
        return;
    }

    log ("Transcribed " + juce::String (r.audioSeconds, 1) + " s in " + juce::String (r.inferenceSeconds, 2) + " s");

    current.transcript = cleaned;
    current.inserted = false;
    current.insertionNote.clear();

    HistoryEntry entry;
    entry.id = juce::Uuid().toString();
    entry.text = cleaned;
    entry.timestamp = juce::Time::getCurrentTime();
    entry.durationSeconds = r.audioSeconds;
    entry.wordCount = static_cast<int> (text::countWords (cleaned.toStdString()));
    entry.language = toJuce (r.language);
    entry.model = modelName();
    current.historyId = settings.saveHistory() ? entry.id : juce::String();
    history.add (entry);

    const bool shouldInsert = settings.autoInsert() && current.source != Source::MainWindow && pendingTarget.isValid();

    if (shouldInsert)
    {
        beginInsertion (cleaned, pendingTarget, current.historyId, false);
        return;
    }

    transitionTo (DictationState::Completed);
    current.headline = "Transcribed";
    current.insertionNote = current.source == Source::MainWindow ? "Ready to copy or insert"
                          : ! settings.autoInsert()              ? "Automatic insertion is off - copy or insert it from Stilleto"
                                                                 : "No target window was active - copy it from Stilleto";
    current.detail = current.insertionNote;
    history.updateInsertion (current.historyId, false, current.insertionNote);
    notifySession();
}

//==============================================================================================
// Insertion

InsertOptions AppController::insertOptions() const
{
    InsertOptions o;
    switch (settings.insertionMode())
    {
        case InsertionMode::Automatic:       o.method = InsertMethod::Automatic; break;
        case InsertionMode::ClipboardPaste:  o.method = InsertMethod::ClipboardPaste; break;
        case InsertionMode::SimulatedTyping: o.method = InsertMethod::UnicodeKeys; break;
    }
    o.chunkDelayMs = settings.keystrokeDelayMs();
    o.appendTrailingSpace = settings.trailingSpace();
    return o;
}

void AppController::beginInsertion (const juce::String& textToInsert, const InsertionTarget& target, const juce::String& historyId, bool activateTarget)
{
    if (! transitionTo (DictationState::Inserting))
        return;

    current.headline = "Inserting";
    current.detail = "Typing into " + juce::String (target.describe());
    current.targetName = target.describe();
    notifySession();

    // SendInput and the clipboard wait are blocking: run them off the message thread.
    auto options = insertOptions();
    options.activateTarget = activateTarget;
    // A tray click makes Stilleto foreground; the target may then be re-activated (nothing else is).
    options.activateWhenStilletoForeground = current.source == Source::Tray;
    insertionProcess = juce::String (target.processName.c_str());

    insertionPool.addJob ([this, token = alive, cancel = insertionCancel, payload = toUtf16 (textToInsert), target, options, historyId]
    {
        const auto result = TextInserter::insert (payload, target, options, cancel.get());
        juce::MessageManager::callAsync ([this, token, result, historyId]
        {
            if (token->load())
                finishInsertion (result, historyId);
        });
    });
}

void AppController::finishInsertion (const InsertResult& result, const juce::String& historyId)
{
    if (current.state != DictationState::Inserting)
        return;

    current.inserted = result.succeeded();
    current.insertionNote = toJuce (result.message);
    if (! result.clipboardNote.empty())
        current.insertionNote << " " << toJuce (result.clipboardNote);
    history.updateInsertion (historyId, current.inserted,
                             current.inserted ? juce::String ("Inserted into " + current.targetName)
                                              : "Not inserted: " + current.insertionNote);

    log (juce::String ("Insertion ") + (result.succeeded() ? "succeeded" : "failed") + " via "
         + (result.methodUsed == InsertMethod::ClipboardPaste ? "clipboard" : "keystrokes") + " into " + insertionProcess
         + " (status " + juce::String ((int) result.status) + ")");   // process name only: window titles can be private

    if (result.succeeded())
    {
        transitionTo (DictationState::Completed);
        current.headline = "Inserted";
        current.detail = current.insertionNote;
        notifySession();
    }
    else
    {
        setError (result.status == InsertResult::Status::PartiallyInserted ? "Partially inserted" : "Text not inserted",
                  current.insertionNote);
    }
}

void AppController::insertIntoLastApplication (const juce::String& textToInsert, const juce::String& historyId)
{
    if (textToInsert.isEmpty() || isBusy (current.state) || current.state == DictationState::Recording)
        return;

    const auto target = foreground != nullptr ? TextInserter::targetFromWindow (foreground->lastExternalWindow()) : InsertionTarget {};

    if (! target.isValid())
    {
        setError ("No application to insert into", "Click into the application you want to type in, then use Insert again.");
        return;
    }

    current.source = Source::MainWindow;
    beginInsertion (textToInsert, target, historyId, true);
}

bool AppController::copyToClipboard (const juce::String& textToCopy)
{
    return TextInserter::copyToClipboard (toUtf16 (textToCopy));
}

juce::File AppController::recordingsDirectory()
{
    return SettingsManager::appDataDirectory().getChildFile ("recordings");
}

int AppController::savedRecordingCount() const
{
    return recordingsDirectory().getNumberOfChildFiles (juce::File::findFiles, "*.wav");
}

bool AppController::deleteSavedRecordings()
{
    bool ok = true;
    for (const auto& f : recordingsDirectory().findChildFiles (juce::File::findFiles, false, "*.wav"))
        ok = f.deleteFile() && ok;
    return ok;
}

juce::String AppController::lastApplicationName() const
{
    if (foreground == nullptr) return {};
    const auto t = TextInserter::targetFromWindow (foreground->lastExternalWindow());
    if (! t.isValid()) return {};
    auto name = juce::String (t.processName.c_str());
    return name.upToLastOccurrenceOf (".", false, false).isNotEmpty() ? name.upToLastOccurrenceOf (".", false, false) : name;
}

//==============================================================================================
// Hotkey

juce::String AppController::hotkeyText() const
{
    return juce::String (settings.hotkey().toDisplayString());
}

void AppController::registerHotkeyFromSettings()
{
    const auto spec = settings.hotkey();
    const auto result = hotkey.registerHotkey (spec, [this] { toggleDictation (Source::Hotkey); });
    hotkeyProblem = result == GlobalHotkey::Result::Ok ? juce::String() : juce::String (GlobalHotkey::describe (result));
    if (hotkeyProblem.isNotEmpty())
        log ("Hotkey " + hotkeyText() + ": " + hotkeyProblem);
    listeners.call ([] (Listener& l) { l.hotkeyStatusChanged(); });
}

bool AppController::applyHotkey (const HotkeySpec& spec, juce::String& error)
{
    const auto result = hotkey.registerHotkey (spec, [this] { toggleDictation (Source::Hotkey); });
    if (result != GlobalHotkey::Result::Ok)
    {
        error = GlobalHotkey::describe (result);
        listeners.call ([] (Listener& l) { l.hotkeyStatusChanged(); });
        return false;
    }

    hotkeyProblem.clear();
    settings.setHotkey (spec);
    listeners.call ([] (Listener& l) { l.hotkeyStatusChanged(); });
    notifySession();
    return true;
}

void AppController::toggleHotkeyPause()
{
    if (hotkey.isPaused())
    {
        const auto r = hotkey.resume();
        hotkeyProblem = r == GlobalHotkey::Result::Ok ? juce::String() : juce::String (GlobalHotkey::describe (r));
    }
    else if (hotkey.isRegistered())
    {
        hotkey.pause();
    }
    else
    {
        registerHotkeyFromSettings();
    }

    updateTray();
    listeners.call ([] (Listener& l) { l.hotkeyStatusChanged(); });
}

void AppController::setHotkeyCaptureActive (bool active)
{
    if (active)
    {
        hotkeyHeldBeforeCapture = hotkey.isRegistered();
        hotkey.unregister();
    }
    else if (hotkeyHeldBeforeCapture && ! hotkey.isRegistered())
    {
        // applyHotkey() may already have registered the new combination.
        hotkey.resume();
    }
}

//==============================================================================================
// Models

juce::String AppController::modelName() const
{
    if (activeModel == juce::File())
        return "No model";
    if (const auto* m = models.findInstalled (activeModel.getFileName()))
        return m->displayName;
    return activeModel.getFileNameWithoutExtension();
}

void AppController::loadActiveModel()
{
    // Choosing a model never rescans: a rescan notifies modelsChanged(), which may call back
    // here. Callers that need fresh disk state rescan first; the guard stops any other cycle.
    if (choosingModel)
        return;
    const juce::ScopedValueSetter<bool> guard (choosingModel, true);

    const InstalledModel* chosen = models.findInstalled (settings.modelFile());

    // Fall back to the recommended model, then to anything valid that is installed.
    if (chosen == nullptr || chosen->status != ModelFileStatus::Valid)
    {
        chosen = models.findInstalled (juce::String (ModelCatalog::findById (ModelCatalog::defaultModelId)->fileName()));
        if (chosen == nullptr || chosen->status != ModelFileStatus::Valid)
        {
            chosen = nullptr;
            for (const auto& m : models.installed())
                if (m.status == ModelFileStatus::Valid) { chosen = &m; break; }
        }
    }

    if (chosen == nullptr)
    {
        activeModel = juce::File();
        modelState = ModelStatus::NoModel;
        modelProblem = "No Whisper model is installed yet.";
        listeners.call ([] (Listener& l) { l.modelStatusChanged(); });
        return;
    }

    activateModel (chosen->file);
}

void AppController::activateModel (juce::File file)
{
    activeModel = file;
    modelState = ModelStatus::Loading;
    modelProblem.clear();
    if (file.getFileName() != settings.modelFile())
        settings.setModelFile (file.getFileName());

    listeners.call ([] (Listener& l) { l.modelStatusChanged(); });
    log ("Loading model " + file.getFullPathName());

    WhisperEngine::LoadOptions options;
    options.useGpu = settings.useGpu();

    transcriber.loadModel (file, options, [this, token = alive, file] (const WhisperEngine::LoadResult& result)
    {
        if (! token->load() || file != activeModel)
            return;   // superseded by a newer selection

        modelState = result.ok ? ModelStatus::Ready : ModelStatus::Failed;
        modelProblem = result.ok ? juce::String() : toJuce (result.error);
        log (result.ok ? "Model ready: " + modelName() : "Model failed: " + modelProblem);
        listeners.call ([] (Listener& l) { l.modelStatusChanged(); });
        notifySession();
    });
}

void AppController::modelsChanged()
{
    // The active model may have been removed from disk.
    if (activeModel != juce::File() && ! activeModel.existsAsFile())
        loadActiveModel();
    else if (activeModel == juce::File() && ! models.installed().isEmpty())
        loadActiveModel();
}

//==============================================================================================
// Microphone

void AppController::refreshMicrophones()
{
    micNames = recorder.inputDeviceNames();
    listeners.call ([] (Listener& l) { l.microphoneStatusChanged(); });
}

juce::String AppController::microphoneName() const
{
    if (micNames.isEmpty())
        return "No microphone";
    const auto preferred = settings.inputDevice();
    return preferred.isNotEmpty() && micNames.contains (preferred) ? preferred : "System default";
}

//==============================================================================================
// Settings

void AppController::applyMotionPreference()
{
    ui::Motion::setReduced (settings.reducedMotion() || ! native::systemAnimationsEnabled());
}

void AppController::settingChanged (const juce::Identifier& key)
{
    using K = SettingsManager::Keys;

    if (key == K::reducedMotion)
        applyMotionPreference();
    else if (key == K::saveHistory)
        history.setPersistenceEnabled (settings.saveHistory());
    else if (key == K::inputDevice)
        recorder.setPreferredDevice (settings.inputDevice());
    else if (key == K::useGpu)
        loadActiveModel();
    else if (key == K::modelDirectory)
    {
        models.setUserDirectory (settings.modelDirectory());
        loadActiveModel();
    }
    else if (key == K::launchAtStartup)
        native::setLaunchAtStartup (L"Stilleto", settings.launchAtStartup(),
                                    L"\"" + native::currentExecutablePath() + L"\" --minimized");
    else if (key == K::widgetVisibility || key == K::recordingIndicator)
        updateWidget();
}

//==============================================================================================
// Windows, tray and widget

void AppController::showMainWindow (std::optional<Page> page)
{
    if (mainWindow == nullptr)
        return;

    if (page.has_value())
        mainWindow->showPage (*page);

    mainWindow->setVisible (true);
    mainWindow->setMinimised (false);
    mainWindow->toFront (true);
    native::bringToForeground (mainWindow->getWindowHandle());
}

void AppController::mainWindowCloseRequested()
{
    if (settings.closeToTray() && tray != nullptr)
        mainWindow->setVisible (false);
    else
        requestQuit();
}

void AppController::mainWindowMinimiseRequested()
{
    if (settings.minimiseToTray() && tray != nullptr)
        mainWindow->setVisible (false);
    else
        mainWindow->setMinimised (true);
}

void AppController::requestQuit()
{
    const bool working = current.state == DictationState::Recording || isBusy (current.state);

    if (! working)
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
        return;
    }

    if (quitConfirmationOpen)
        return;
    quitConfirmationOpen = true;

    showMainWindow();
    juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                      .withIconType (juce::MessageBoxIconType::QuestionIcon)
                                      .withTitle ("Quit while dictating?")
                                      .withMessage ("A dictation is still in progress. Quitting now discards it.")
                                      .withButton ("Quit")
                                      .withButton ("Keep dictating")
                                      .withAssociatedComponent (mainWindow.get()),
                                  [this, token = alive] (int choice)
                                  {
                                      if (! token->load()) return;
                                      quitConfirmationOpen = false;
                                      if (choice == 1)
                                          juce::JUCEApplication::getInstance()->systemRequestedQuit();
                                  });
}

void AppController::updateTray()
{
    if (tray == nullptr)
        return;

    juce::String tip = "Stilleto - ";
    switch (current.state)
    {
        case DictationState::Recording:  tip << "Listening"; break;
        case DictationState::Processing: tip << "Transcribing"; break;
        case DictationState::Inserting:  tip << "Inserting"; break;
        default:                         tip << (hotkey.isPaused() ? "Shortcut paused" : "Press " + hotkeyText() + " to dictate"); break;
    }

    tray->setRecording (current.state == DictationState::Recording, tip);
}

void AppController::updateWidget()
{
    if (widget == nullptr)
        return;

    bool show = false;
    switch (settings.widgetVisibility())
    {
        case WidgetVisibility::Always:          show = true; break;
        case WidgetVisibility::DuringDictation: show = current.state != DictationState::Idle; break;
        case WidgetVisibility::Never:           show = current.state == DictationState::Recording && settings.recordingIndicator(); break;
    }

    // When dictating from the dashboard the dashboard itself shows the state.
    if (current.source == Source::MainWindow && mainWindow != nullptr && mainWindow->isActiveWindow()
        && settings.widgetVisibility() != WidgetVisibility::Always)
        show = false;

    widget->setShouldBeShown (show, pendingTarget.window);
}

void AppController::timerCallback()
{
    const auto elapsedInState = juce::Time::getMillisecondCounter() - stateEnteredAt;

    if (current.state == DictationState::Recording)
    {
        current.recordedSeconds = recorder.elapsedSeconds();

        // Device lost mid-recording (unplugged, disabled, rate changed): samples stop arriving.
        const auto now = juce::Time::getMillisecondCounter();
        const auto captured = recorder.capturedSampleCount();
        if (captured != lastCapturedSamples)
        {
            lastCapturedSamples = captured;
            lastCaptureProgressAt = now;
        }
        // Allow a few seconds for the first samples (Bluetooth headsets switch profile slowly),
        // then expect a steady stream.
        const bool stalled = captured == 0 ? now - lastCaptureProgressAt > 5000 : now - lastCaptureProgressAt > 1500;
        if (recorder.deviceFailed() || stalled)
        {
            refreshMicrophones();
            setError ("Microphone disconnected", "The microphone stopped delivering audio, so the recording was discarded. "
                                                 "Check the connection, or choose another microphone in Settings.");
            return;
        }

        if (current.recordedSeconds >= settings.maxRecordingSeconds())
        {
            log ("Maximum recording length reached");
            stopDictation();
        }
    }
    else if ((current.state == DictationState::Completed && elapsedInState > completedLingerMs)
             || (current.state == DictationState::Error && elapsedInState > errorLingerMs))
    {
        transitionTo (DictationState::Idle);
        notifySession();
    }
}

} // namespace stilleto
