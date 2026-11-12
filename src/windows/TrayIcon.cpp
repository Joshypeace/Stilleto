#include "windows/TrayIcon.h"

#include "theme/Icons.h"

namespace stilleto
{

TrayIcon::TrayIcon (Actions a, std::function<MenuState()> stateProvider)
    : actions (std::move (a)), state (std::move (stateProvider))
{
    idleIcon = ui::Icons::renderBrandImage (64, false);
    recordingIcon = ui::Icons::renderBrandImage (64, true);
    setIconImage (idleIcon, idleIcon);
    setIconTooltip ("Stilleto");
}

void TrayIcon::setRecording (bool recording, const juce::String& tooltip)
{
    if (recording != showingRecording)
    {
        showingRecording = recording;
        const auto& img = recording ? recordingIcon : idleIcon;
        setIconImage (img, img);
    }
    setIconTooltip (tooltip);
}

void TrayIcon::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu() || e.mods.isLeftButtonDown())
    {
        // Left click opens the window directly; right click shows the menu.
        if (e.mods.isPopupMenu())
            showMenu();
        else if (actions.openWindow)
            actions.openWindow();
    }
}

void TrayIcon::mouseDoubleClick (const juce::MouseEvent&)
{
    if (actions.openWindow)
        actions.openWindow();
}

void TrayIcon::showMenu()
{
    const auto s = state ? state() : MenuState {};

    juce::PopupMenu menu;
    menu.addSectionHeader ("Stilleto");
    menu.addItem ("Open Stilleto", [fn = actions.openWindow] { if (fn) fn(); });
    menu.addSeparator();
    menu.addItem (juce::PopupMenu::Item (s.recording ? "Stop dictation" : "Start dictation")
                      .setEnabled (! s.busy)
                      .setAction ([fn = actions.toggleDictation] { if (fn) fn(); }));
    menu.addItem (juce::PopupMenu::Item (s.hotkeyPaused ? "Resume shortcut (" + s.hotkeyText + ")"
                                                        : "Pause shortcut (" + s.hotkeyText + ")")
                      .setAction ([fn = actions.toggleHotkeyPause] { if (fn) fn(); }));
    menu.addItem ("Settings", [fn = actions.openSettings] { if (fn) fn(); });
    menu.addSeparator();
    menu.addItem ("Quit Stilleto", [fn = actions.quit] { if (fn) fn(); });

    // JUCE's tray window takes the foreground on click, so the menu dismisses normally.
    menu.showMenuAsync (juce::PopupMenu::Options()
                            .withTargetScreenArea (juce::Rectangle<int> {}.withPosition (juce::Desktop::getMousePosition()).withSize (1, 1))
                            .withStandardItemHeight (30));
}

} // namespace stilleto
