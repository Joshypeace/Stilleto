#pragma once

#include "audio/AudioRecorder.h"
#include "core/DictationState.h"
#include "history/HistoryManager.h"
#include "settings/SettingsManager.h"
#include "transcription/ModelManager.h"
#include "transcription/WhisperTranscriber.h"
#include "windows/ForegroundTracker.h"
#include "windows/GlobalHotkey.h"
#include "windows/TextInserter.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>

namespace stilleto
{

class MainWindow;
class RecordingWidget;
class TrayIcon;

/** Owns every subsystem and runs the dictation state machine.

    All public functions run on the JUCE message thread. Worker threads (audio drain, whisper,
    insertion, history writer) report back only through messages posted to that thread, so UI
    components never see a state change from another thread. */
class AppController : private juce::Timer,
                      private SettingsManager::Listener,
                      private ModelManager::Listener
{
public:
    enum class Source { Hotkey, Tray, MainWindow, Widget };

    enum class Page { Overview = 0, Dictation, History, Models, Settings, About };

    /** Everything the UI needs to render the dictation state. */
    struct Session
    {
        DictationState state = DictationState::Idle;
        Source source = Source::Hotkey;
        juce::String headline;          // e.g. "Listening", "Transcribing"
        juce::String detail;            // explanatory line / error message
        juce::String transcript;        // last successful transcript
        juce::String insertionNote;     // "Inserted into Notepad", "Not inserted: ..."
        juce::String clipboardNote;     // set when the user's previous clipboard was not fully restored
        bool inserted = false;
        juce::String targetName;        // window the text goes to
        double recordedSeconds = 0.0;
        juce::String historyId;         // entry for the last transcript, if saved
    };

    enum class ModelStatus { NoModel, Loading, Ready, Failed };

    struct Listener
    {
        virtual ~Listener() = default;
        virtual void sessionChanged() {}
        virtual void modelStatusChanged() {}
        virtual void hotkeyStatusChanged() {}
        virtual void microphoneStatusChanged() {}
    };

    AppController();
    ~AppController() override;

    /** Creates windows, tray and hotkey. startHidden = launched at sign-in (tray only). */
    void initialise (bool startHidden);

    /** Stops recording, cancels inference, waits for workers and releases every OS resource. */
    void shutdown();

    /** Developer aid (--snapshot <dir>): renders every page off-screen to PNG files without
        registering the hotkey, creating the tray icon or showing a window. */
    void renderSnapshots (const juce::File& directory, std::function<void()> onDone);

    // Dictation -----------------------------------------------------------------------------
    void toggleDictation (Source source);
    void startDictation (Source source);
    void stopDictation();
    void cancelDictation();

    /** Inserts text into the last external application (Insert / Insert again buttons). */
    void insertIntoLastApplication (const juce::String& text, const juce::String& historyId = {});
    bool copyToClipboard (const juce::String& text);

    // Opt-in raw recordings (Settings > Privacy > Save raw recordings).
    [[nodiscard]] static juce::File recordingsDirectory();
    [[nodiscard]] int savedRecordingCount() const;
    bool deleteSavedRecordings();   // returns false if any file could not be deleted

    [[nodiscard]] const Session& session() const noexcept { return current; }
    [[nodiscard]] DictationState state() const noexcept { return current.state; }
    [[nodiscard]] double liveRecordingSeconds() const noexcept { return recorder.elapsedSeconds(); }
    [[nodiscard]] juce::String lastApplicationName() const;

    // Hotkey ----------------------------------------------------------------------------------
    bool applyHotkey (const HotkeySpec& spec, juce::String& error);
    [[nodiscard]] bool isHotkeyAvailable (const HotkeySpec& spec) const { return hotkey.isAvailable (spec); }
    [[nodiscard]] bool isHotkeyRegistered() const noexcept { return hotkey.isRegistered(); }
    [[nodiscard]] bool isHotkeyPaused() const noexcept { return hotkey.isPaused(); }
    [[nodiscard]] const juce::String& hotkeyError() const noexcept { return hotkeyProblem; }
    [[nodiscard]] juce::String hotkeyText() const;
    void toggleHotkeyPause();

    /** Releases the live shortcut while the user records a new one in Settings, so pressing the
        current combination does not start a dictation. */
    void setHotkeyCaptureActive (bool active);

    // Models ----------------------------------------------------------------------------------
    void activateModel (juce::File file);   // by value: callers may pass a reference into UI state
    [[nodiscard]] ModelStatus modelStatus() const noexcept { return modelState; }
    [[nodiscard]] juce::String modelName() const;
    [[nodiscard]] const juce::String& modelMessage() const noexcept { return modelProblem; }
    [[nodiscard]] juce::File activeModelFile() const { return activeModel; }

    // Microphone -------------------------------------------------------------------------------
    void refreshMicrophones();
    [[nodiscard]] const juce::StringArray& microphones() const noexcept { return micNames; }
    [[nodiscard]] juce::String microphoneName() const;
    [[nodiscard]] bool microphoneAvailable() const noexcept { return ! micNames.isEmpty(); }

    // Windows ----------------------------------------------------------------------------------
    void showMainWindow (std::optional<Page> page = std::nullopt);
    void mainWindowCloseRequested();
    void mainWindowMinimiseRequested();
    void requestQuit();

    // Subsystems --------------------------------------------------------------------------------
    SettingsManager& getSettings() noexcept         { return settings; }
    HistoryManager& getHistory() noexcept           { return history; }
    ModelManager& getModels() noexcept              { return models; }
    AudioRecorder& getRecorder() noexcept           { return recorder; }
    WhisperTranscriber& getTranscriber() noexcept   { return transcriber; }

    void addListener (Listener* l)    { listeners.add (l); }
    void removeListener (Listener* l) { listeners.remove (l); }

private:
    bool transitionTo (DictationState next);
    void setError (const juce::String& headline, const juce::String& detail);
    void notifySession();
    void handleTranscription (const WhisperTranscriber::Result& result);
    void beginInsertion (const juce::String& text, const InsertionTarget& target, const juce::String& historyId, bool activateTarget);
    void finishInsertion (const InsertResult& result, const juce::String& historyId);
    void loadActiveModel();
    void registerHotkeyFromSettings();
    void applyMotionPreference();
    void updateTray();
    void updateWidget();
    [[nodiscard]] InsertOptions insertOptions() const;

    void timerCallback() override;
    void settingChanged (const juce::Identifier& key) override;
    void modelsChanged() override;

    // Order matters: members are destroyed in reverse, and shutdown() runs first regardless.
    SettingsManager settings;
    HistoryManager history;
    ModelManager models;
    AudioRecorder recorder;
    WhisperTranscriber transcriber;
    GlobalHotkey hotkey;
    std::unique_ptr<ForegroundTracker> foreground;
    juce::ThreadPool insertionPool { juce::ThreadPoolOptions().withThreadName ("Stilleto insertion").withNumberOfThreads (1) };

    std::unique_ptr<MainWindow> mainWindow;
    std::unique_ptr<RecordingWidget> widget;
    std::unique_ptr<TrayIcon> tray;
    juce::ListenerList<Listener> listeners;

    Session current;
    InsertionTarget pendingTarget;
    juce::uint32 stateEnteredAt = 0;

    juce::File activeModel;
    ModelStatus modelState = ModelStatus::NoModel;
    juce::String modelProblem;
    juce::String hotkeyProblem;
    juce::StringArray micNames;
    bool shuttingDown = false;
    bool quitConfirmationOpen = false;
    bool hotkeyHeldBeforeCapture = false;
    bool choosingModel = false;
    bool cancelRequested = false;
    juce::int64 lastCapturedSamples = 0;
    juce::uint32 lastCaptureProgressAt = 0;
    juce::String insertionProcess;
    std::shared_ptr<std::atomic<bool>> insertionCancel = std::make_shared<std::atomic<bool>> (false);
    bool snapshotMode = false;

    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);
};

} // namespace stilleto
