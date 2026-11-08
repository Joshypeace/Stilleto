#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include <functional>

namespace stilleto
{

/** Notification-area icon (Shell_NotifyIcon via JUCE) with Stilleto's context menu.

    It holds no pointers to application objects: every action is a std::function supplied by the
    owner (AppController), which destroys the tray before anything those functions reference. */
class TrayIcon : public juce::SystemTrayIconComponent
{
public:
    struct Actions
    {
        std::function<void()> openWindow;
        std::function<void()> toggleDictation;
        std::function<void()> openSettings;
        std::function<void()> toggleHotkeyPause;
        std::function<void()> quit;
    };

    struct MenuState
    {
        bool recording = false;
        bool busy = false;            // processing/inserting: dictation toggle disabled
        bool hotkeyPaused = false;
        juce::String hotkeyText;
    };

    TrayIcon (Actions actions, std::function<MenuState()> stateProvider);

    /** Switches between the idle and recording icons and updates the tooltip. */
    void setRecording (bool recording, const juce::String& tooltip);

private:
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void showMenu();

    Actions actions;
    std::function<MenuState()> state;
    juce::Image idleIcon, recordingIcon;
    bool showingRecording = false;
};

} // namespace stilleto
