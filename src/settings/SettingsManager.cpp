#include "settings/SettingsManager.h"

namespace stilleto
{

SettingsManager::SettingsManager()
{
    juce::PropertiesFile::Options options;
    options.applicationName = "settings";
    options.filenameSuffix = ".xml";
    options.folderName = "Stilleto";
    options.osxLibrarySubFolder = "Application Support";
    options.storageFormat = juce::PropertiesFile::storeAsXML;
    options.millisecondsBeforeSaving = 800;
    options.commonToAllUsers = false;

    appDataDirectory().createDirectory();
    properties = std::make_unique<juce::PropertiesFile> (appDataDirectory().getChildFile ("settings.xml"), options);
}

SettingsManager::~SettingsManager()
{
    flush();
}

void SettingsManager::flush()
{
    if (properties != nullptr)
        properties->saveIfNeeded();
}

juce::File SettingsManager::appDataDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Stilleto");
}

juce::File SettingsManager::defaultModelDirectory()
{
    // %LOCALAPPDATA% (not roaming): models are large and machine-specific.
    const auto local = juce::File::getSpecialLocation (juce::File::windowsLocalAppData);
    return (local.exists() ? local : appDataDirectory().getParentDirectory()).getChildFile ("Stilleto").getChildFile ("models");
}

void SettingsManager::setValue (const juce::Identifier& key, const juce::var& value)
{
    const auto name = key.toString();
    if (properties->containsKey (name) && properties->getValue (name) == value.toString())
        return;

    properties->setValue (name, value);
    listeners.call ([&] (Listener& l) { l.settingChanged (key); });
}

bool SettingsManager::getBool (const juce::Identifier& key, bool fallback) const
{
    return properties->getBoolValue (key.toString(), fallback);
}

int SettingsManager::getInt (const juce::Identifier& key, int fallback) const
{
    return properties->getIntValue (key.toString(), fallback);
}

juce::String SettingsManager::getString (const juce::Identifier& key, const juce::String& fallback) const
{
    return properties->getValue (key.toString(), fallback);
}

// General
bool SettingsManager::launchAtStartup() const               { return getBool (Keys::launchAtStartup, false); }
void SettingsManager::setLaunchAtStartup (bool v)           { setValue (Keys::launchAtStartup, v); }
bool SettingsManager::minimiseToTray() const                { return getBool (Keys::minimiseToTray, true); }
void SettingsManager::setMinimiseToTray (bool v)            { setValue (Keys::minimiseToTray, v); }
bool SettingsManager::closeToTray() const                   { return getBool (Keys::closeToTray, true); }
void SettingsManager::setCloseToTray (bool v)               { setValue (Keys::closeToTray, v); }
bool SettingsManager::reducedMotion() const                 { return getBool (Keys::reducedMotion, false); }
void SettingsManager::setReducedMotion (bool v)             { setValue (Keys::reducedMotion, v); }

WidgetVisibility SettingsManager::widgetVisibility() const
{
    return static_cast<WidgetVisibility> (juce::jlimit (0, 2, getInt (Keys::widgetVisibility, 0)));
}
void SettingsManager::setWidgetVisibility (WidgetVisibility v) { setValue (Keys::widgetVisibility, static_cast<int> (v)); }

// Shortcut
HotkeySpec SettingsManager::hotkey() const
{
    const auto stored = getString (Keys::hotkey, {});
    if (const auto parsed = HotkeySpec::parse (stored.toStdString()))
        return *parsed;
    return HotkeySpec::defaultSpec();
}
void SettingsManager::setHotkey (const HotkeySpec& spec)    { setValue (Keys::hotkey, juce::String (spec.toStorageString())); }

// Audio
juce::String SettingsManager::inputDevice() const           { return getString (Keys::inputDevice, {}); }
void SettingsManager::setInputDevice (const juce::String& v){ setValue (Keys::inputDevice, v); }
int SettingsManager::maxRecordingSeconds() const            { return juce::jlimit (10, 1800, getInt (Keys::maxRecordingSeconds, 300)); }
void SettingsManager::setMaxRecordingSeconds (int v)        { setValue (Keys::maxRecordingSeconds, juce::jlimit (10, 1800, v)); }

// Transcription
juce::String SettingsManager::modelFile() const             { return getString (Keys::modelFile, "ggml-base.en.bin"); }
void SettingsManager::setModelFile (const juce::String& v)  { setValue (Keys::modelFile, v); }

juce::File SettingsManager::modelDirectory() const
{
    const auto stored = getString (Keys::modelDirectory, {});
    return stored.isNotEmpty() && juce::File::isAbsolutePath (stored) ? juce::File (stored) : defaultModelDirectory();
}
void SettingsManager::setModelDirectory (const juce::File& v) { setValue (Keys::modelDirectory, v.getFullPathName()); }

juce::String SettingsManager::language() const              { return getString (Keys::language, "en"); }
void SettingsManager::setLanguage (const juce::String& v)   { setValue (Keys::language, v); }
int SettingsManager::threads() const                        { return juce::jlimit (0, 64, getInt (Keys::threads, 0)); }
void SettingsManager::setThreads (int v)                    { setValue (Keys::threads, juce::jlimit (0, 64, v)); }
bool SettingsManager::useGpu() const                        { return getBool (Keys::useGpu, false); }
void SettingsManager::setUseGpu (bool v)                    { setValue (Keys::useGpu, v); }

// Insertion
bool SettingsManager::autoInsert() const                    { return getBool (Keys::autoInsert, true); }
void SettingsManager::setAutoInsert (bool v)                { setValue (Keys::autoInsert, v); }
InsertionMode SettingsManager::insertionMode() const
{
    return static_cast<InsertionMode> (juce::jlimit (0, 2, getInt (Keys::insertionMode, 0)));
}
void SettingsManager::setInsertionMode (InsertionMode v)    { setValue (Keys::insertionMode, static_cast<int> (v)); }
int SettingsManager::keystrokeDelayMs() const               { return juce::jlimit (5, 200, getInt (Keys::keystrokeDelayMs, 35)); }
void SettingsManager::setKeystrokeDelayMs (int v)           { setValue (Keys::keystrokeDelayMs, juce::jlimit (5, 200, v)); }
bool SettingsManager::trailingSpace() const                 { return getBool (Keys::trailingSpace, true); }
void SettingsManager::setTrailingSpace (bool v)             { setValue (Keys::trailingSpace, v); }
bool SettingsManager::paragraphPerSegment() const           { return getBool (Keys::paragraphPerSegment, false); }
void SettingsManager::setParagraphPerSegment (bool v)       { setValue (Keys::paragraphPerSegment, v); }
bool SettingsManager::tidyPunctuation() const               { return getBool (Keys::tidyPunctuation, true); }
void SettingsManager::setTidyPunctuation (bool v)           { setValue (Keys::tidyPunctuation, v); }

// Privacy
bool SettingsManager::saveHistory() const                   { return getBool (Keys::saveHistory, true); }
void SettingsManager::setSaveHistory (bool v)               { setValue (Keys::saveHistory, v); }
bool SettingsManager::saveRecordings() const                { return getBool (Keys::saveRecordings, false); }
void SettingsManager::setSaveRecordings (bool v)            { setValue (Keys::saveRecordings, v); }
bool SettingsManager::recordingIndicator() const            { return getBool (Keys::recordingIndicator, true); }
void SettingsManager::setRecordingIndicator (bool v)        { setValue (Keys::recordingIndicator, v); }

// Window
juce::String SettingsManager::windowState() const           { return getString (Keys::windowState, {}); }
void SettingsManager::setWindowState (const juce::String& v){ setValue (Keys::windowState, v); }
bool SettingsManager::firstRunComplete() const              { return getBool (Keys::firstRunComplete, false); }
void SettingsManager::setFirstRunComplete (bool v)          { setValue (Keys::firstRunComplete, v); }

} // namespace stilleto
