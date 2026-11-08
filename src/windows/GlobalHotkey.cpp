#include "windows/GlobalHotkey.h"

#include <windows.h>

namespace stilleto
{
namespace
{
constexpr wchar_t windowClassName[] = L"StilletoGlobalHotkeyWindow";
constexpr UINT modNoRepeat = 0x4000;   // MOD_NOREPEAT: holding the keys must not toggle repeatedly
} // namespace

struct GlobalHotkeyWindowAccess
{
    static LRESULT CALLBACK windowProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        if (msg == WM_HOTKEY)
        {
            if (auto* owner = reinterpret_cast<GlobalHotkey*> (GetWindowLongPtrW (hwnd, GWLP_USERDATA)))
                owner->handleHotkeyMessage (static_cast<int> (wParam));
            return 0;
        }

        return DefWindowProcW (hwnd, msg, wParam, lParam);
    }

    static void ensureClassRegistered()
    {
        static const ATOM atom = []
        {
            WNDCLASSEXW wc {};
            wc.cbSize = sizeof (wc);
            wc.lpfnWndProc = &windowProc;
            wc.hInstance = GetModuleHandleW (nullptr);
            wc.lpszClassName = windowClassName;
            return RegisterClassExW (&wc);
        }();
        (void) atom;
    }
};

GlobalHotkey::GlobalHotkey()
{
    GlobalHotkeyWindowAccess::ensureClassRegistered();

    // HWND_MESSAGE: invisible, never activated, not enumerated, only receives posted messages.
    window = CreateWindowExW (0, windowClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr,
                              GetModuleHandleW (nullptr), nullptr);

    if (window != nullptr)
        SetWindowLongPtrW (window, GWLP_USERDATA, reinterpret_cast<LONG_PTR> (this));
}

GlobalHotkey::~GlobalHotkey()
{
    unregister();

    if (window != nullptr)
    {
        SetWindowLongPtrW (window, GWLP_USERDATA, 0);
        DestroyWindow (window);
    }
}

void GlobalHotkey::handleHotkeyMessage (int id)
{
    if (id == hotkeyId && registered && callback)
        callback();
}

GlobalHotkey::Result GlobalHotkey::registerInternal (const HotkeySpec& s)
{
    if (window == nullptr)
        return Result::SystemError;

    if (! s.isUsable())
        return Result::InvalidShortcut;

    if (RegisterHotKey (window, hotkeyId, s.modifiers | modNoRepeat, s.virtualKey))
        return Result::Ok;

    return GetLastError() == ERROR_HOTKEY_ALREADY_REGISTERED ? Result::AlreadyInUse : Result::SystemError;
}

GlobalHotkey::Result GlobalHotkey::registerHotkey (const HotkeySpec& newSpec, Callback onPressed)
{
    const auto previousSpec = spec;
    const bool hadRegistration = registered;

    if (registered)
    {
        UnregisterHotKey (window, hotkeyId);
        registered = false;
    }

    const auto result = registerInternal (newSpec);

    if (result == Result::Ok)
    {
        spec = newSpec;
        callback = std::move (onPressed);
        registered = true;
        paused = false;
        return result;
    }

    // Keep the user's working shortcut if the new one is unavailable.
    if (hadRegistration && registerInternal (previousSpec) == Result::Ok)
        registered = true;

    return result;
}

void GlobalHotkey::unregister()
{
    if (registered && window != nullptr)
        UnregisterHotKey (window, hotkeyId);

    registered = false;
}

void GlobalHotkey::pause()
{
    if (registered)
    {
        unregister();
        paused = true;
    }
}

GlobalHotkey::Result GlobalHotkey::resume()
{
    if (registered)
        return Result::Ok;

    const auto result = registerInternal (spec);
    registered = result == Result::Ok;
    if (registered) paused = false;
    return result;
}

bool GlobalHotkey::isAvailable (const HotkeySpec& candidate) const
{
    if (registered && candidate == spec)
        return true;

    if (window == nullptr || ! candidate.isUsable())
        return false;

    if (! RegisterHotKey (window, probeId, candidate.modifiers | modNoRepeat, candidate.virtualKey))
        return false;

    UnregisterHotKey (window, probeId);
    return true;
}

std::string GlobalHotkey::describe (Result r)
{
    switch (r)
    {
        case Result::Ok:              return "Shortcut registered";
        case Result::AlreadyInUse:    return "This shortcut is already used by another application. Choose a different combination in Settings.";
        case Result::InvalidShortcut: return "This shortcut is not allowed. Include Ctrl, Alt or Win so it does not interfere with typing.";
        case Result::SystemError:     return "Windows could not register the global shortcut.";
    }
    return "Unknown";
}

} // namespace stilleto
