#pragma once

#include "core/HotkeySpec.h"

#include <functional>
#include <string>

struct HWND__;

namespace stilleto
{

/** A system-wide shortcut registered with RegisterHotKey.

    Owns a message-only window created on the constructing thread; WM_HOTKEY is dispatched to it by
    that thread's message loop (the JUCE message thread in the app), so the callback always runs on
    the thread that created this object and never needs the Stilleto window to have focus. */
class GlobalHotkey
{
public:
    enum class Result
    {
        Ok,
        AlreadyInUse,      // another application owns this combination
        InvalidShortcut,
        SystemError
    };

    using Callback = std::function<void()>;

    GlobalHotkey();
    ~GlobalHotkey();

    GlobalHotkey (const GlobalHotkey&) = delete;
    GlobalHotkey& operator= (const GlobalHotkey&) = delete;

    /** Replaces any existing registration. On failure the previous registration is restored. */
    Result registerHotkey (const HotkeySpec& spec, Callback onPressed);

    /** Releases the shortcut (idempotent). The spec is remembered for resume(). */
    void unregister();

    /** Temporarily releases / re-acquires the remembered shortcut (tray "Pause hotkey"). */
    void pause();
    Result resume();

    [[nodiscard]] bool isRegistered() const noexcept { return registered; }
    [[nodiscard]] bool isPaused() const noexcept { return paused; }
    [[nodiscard]] const HotkeySpec& currentSpec() const noexcept { return spec; }

    /** Probes whether a combination could be registered right now, without keeping it.
        A spec currently held by this instance counts as available. */
    [[nodiscard]] bool isAvailable (const HotkeySpec& candidate) const;

    [[nodiscard]] static std::string describe (Result r);

private:
    friend struct GlobalHotkeyWindowAccess;
    void handleHotkeyMessage (int id);
    Result registerInternal (const HotkeySpec& s);

    HWND__* window = nullptr;
    HotkeySpec spec;
    Callback callback;
    bool registered = false;
    bool paused = false;

    static constexpr int hotkeyId = 0x5711;
    static constexpr int probeId  = 0x5712;
};

} // namespace stilleto
