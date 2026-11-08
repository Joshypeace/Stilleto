#pragma once

#include "core/HotkeySpec.h"

#include <juce_data_structures/juce_data_structures.h>
#include <juce_events/juce_events.h>

namespace stilleto
{

enum class InsertionMode { Automatic = 0, ClipboardPaste = 1, SimulatedTyping = 2 };
enum class WidgetVisibility { DuringDictation = 0, Always = 1, Never = 2 };

/** Every persisted preference, behind typed accessors. Values are stored in
    %APPDATA%\Stilleto\settings.xml. All calls are made on the message thread; listeners are
    notified synchronously after a value actually changes. */
class SettingsManager
{
public:
    // Keys (also used by listeners to tell what changed).
    struct Keys
    {
        static inline const juce::Identifier launchAtStartup     { "launchAtStartup" };
        static inline const juce::Identifier minimiseToTray      { "minimiseToTray" };
        static inline const juce::Identifier closeToTray         { "closeToTray" };
        static inline const juce::Identifier widgetVisibility    { "widgetVisibility" };
        static inline const juce::Identifier reducedMotion       { "reducedMotion" };
        static inline const juce::Identifier hotkey              { "hotkey" };
        static inline const juce::Identifier inputDevice         { "inputDevice" };
        static inline const juce::Identifier maxRecordingSeconds { "maxRecordingSeconds" };
        static inline const juce::Identifier modelFile           { "modelFile" };
        static inline const juce::Identifier modelDirectory      { "modelDirectory" };
        static inline const juce::Identifier language            { "language" };
        static inline const juce::Identifier threads             { "threads" };
        static inline const juce::Identifier useGpu              { "useGpu" };
        static inline const juce::Identifier autoInsert          { "autoInsert" };
        static inline const juce::Identifier insertionMode       { "insertionMode" };
        static inline const juce::Identifier keystrokeDelayMs    { "keystrokeDelayMs" };
        static inline const juce::Identifier trailingSpace       { "trailingSpace" };
        static inline const juce::Identifier paragraphPerSegment { "paragraphPerSegment" };
        static inline const juce::Identifier tidyPunctuation     { "tidyPunctuation" };
        static inline const juce::Identifier removeDisfluencies  { "removeDisfluencies" };
        static inline const juce::Identifier saveHistory         { "saveHistory" };
        static inline const juce::Identifier saveRecordings      { "saveRecordings" };
        static inline const juce::Identifier recordingIndicator  { "recordingIndicator" };
        static inline const juce::Identifier windowState         { "windowState" };
        static inline const juce::Identifier firstRunComplete    { "firstRunComplete" };
    };

    struct Listener
    {
        virtual ~Listener() = default;
        virtual void settingChanged (const juce::Identifier& key) = 0;
    };

    SettingsManager();
    ~SettingsManager();

    void addListener (Listener* l)    { listeners.add (l); }
    void removeListener (Listener* l) { listeners.remove (l); }

    /** Writes pending changes now (also happens automatically shortly after each change). */
    void flush();

    [[nodiscard]] static juce::File appDataDirectory();      // %APPDATA%\Stilleto
    [[nodiscard]] static juce::File defaultModelDirectory(); // %LOCALAPPDATA%\Stilleto\models

    // General
    bool launchAtStartup() const;            void setLaunchAtStartup (bool);
    bool minimiseToTray() const;             void setMinimiseToTray (bool);
    bool closeToTray() const;                void setCloseToTray (bool);
    WidgetVisibility widgetVisibility() const; void setWidgetVisibility (WidgetVisibility);
    bool reducedMotion() const;              void setReducedMotion (bool);

    // Shortcut
    HotkeySpec hotkey() const;               void setHotkey (const HotkeySpec&);

    // Audio
    juce::String inputDevice() const;        void setInputDevice (const juce::String&);   // empty = system default
    int maxRecordingSeconds() const;         void setMaxRecordingSeconds (int);

    // Transcription
    juce::String modelFile() const;          void setModelFile (const juce::String&);     // file name in a model directory
    juce::File modelDirectory() const;       void setModelDirectory (const juce::File&);
    juce::String language() const;           void setLanguage (const juce::String&);      // ISO code or "auto"
    int threads() const;                     void setThreads (int);                       // 0 = automatic
    bool useGpu() const;                     void setUseGpu (bool);

    // Insertion
    bool autoInsert() const;                 void setAutoInsert (bool);
    InsertionMode insertionMode() const;     void setInsertionMode (InsertionMode);
    int keystrokeDelayMs() const;            void setKeystrokeDelayMs (int);
    bool trailingSpace() const;              void setTrailingSpace (bool);
    bool paragraphPerSegment() const;        void setParagraphPerSegment (bool);
    bool tidyPunctuation() const;            void setTidyPunctuation (bool);
    bool removeDisfluencies() const;         void setRemoveDisfluencies (bool);

    // Privacy
    bool saveHistory() const;                void setSaveHistory (bool);
    bool saveRecordings() const;             void setSaveRecordings (bool);
    bool recordingIndicator() const;         void setRecordingIndicator (bool);

    // Window
    juce::String windowState() const;        void setWindowState (const juce::String&);
    bool firstRunComplete() const;           void setFirstRunComplete (bool);

private:
    void setValue (const juce::Identifier& key, const juce::var& value);
    bool getBool (const juce::Identifier& key, bool fallback) const;
    int getInt (const juce::Identifier& key, int fallback) const;
    juce::String getString (const juce::Identifier& key, const juce::String& fallback) const;

    std::unique_ptr<juce::PropertiesFile> properties;
    juce::ListenerList<Listener> listeners;
};

} // namespace stilleto
