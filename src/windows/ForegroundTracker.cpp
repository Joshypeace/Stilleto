#include "windows/ForegroundTracker.h"

#include <windows.h>

#include <array>
#include <atomic>

namespace stilleto
{
namespace
{
std::atomic<ForegroundTracker*> activeTracker { nullptr };

bool isUsableExternalWindow (HWND hwnd)
{
    if (hwnd == nullptr || ! IsWindowVisible (hwnd))
        return false;

    DWORD pid = 0;
    GetWindowThreadProcessId (hwnd, &pid);
    if (pid == GetCurrentProcessId())
        return false;

    // Ignore the shell surfaces that briefly take the foreground (taskbar, Start, task switcher).
    wchar_t cls[64] {};
    GetClassNameW (hwnd, cls, 64);
    static constexpr std::array<const wchar_t*, 9> shellClasses {
        L"Shell_TrayWnd", L"Shell_SecondaryTrayWnd", L"Windows.UI.Core.CoreWindow",
        L"MultitaskingViewFrame", L"XamlExplorerHostIslandWindow", L"TopLevelWindowForOverflowXamlIsland",
        L"NotifyIconOverflowWindow", L"Progman", L"WorkerW"
    };

    for (const wchar_t* shellClass : shellClasses)
        if (lstrcmpW (cls, shellClass) == 0)
            return false;

    return true;
}

void CALLBACK winEventProc (HWINEVENTHOOK, DWORD event, HWND hwnd, LONG idObject, LONG, DWORD, DWORD)
{
    if (event == EVENT_SYSTEM_FOREGROUND && idObject == OBJID_WINDOW)
        if (auto* tracker = activeTracker.load())
            tracker->handleForegroundChange (hwnd);
}
} // namespace

ForegroundTracker::ForegroundTracker()
{
    activeTracker = this;
    hook = SetWinEventHook (EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr, &winEventProc,
                            0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

    handleForegroundChange (GetForegroundWindow());
}

ForegroundTracker::~ForegroundTracker()
{
    if (hook != nullptr)
        UnhookWinEvent (hook);

    activeTracker = nullptr;
}

void ForegroundTracker::handleForegroundChange (HWND__* window) noexcept
{
    if (isUsableExternalWindow (window))
        lastExternal = window;
}

HWND__* ForegroundTracker::lastExternalWindow() const noexcept
{
    return lastExternal != nullptr && IsWindow (lastExternal) ? lastExternal : nullptr;
}

} // namespace stilleto
