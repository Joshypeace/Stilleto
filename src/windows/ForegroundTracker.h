#pragma once

struct HWND__;
struct HWINEVENTHOOK__;

namespace stilleto
{

/** Remembers the most recent foreground window that does not belong to Stilleto.

    Lets "Insert" from the dashboard or history target the application the user was working in
    before switching to Stilleto. Uses an out-of-context WinEvent hook, delivered through the
    creating thread's message loop (the JUCE message thread). Only one instance may exist. */
class ForegroundTracker
{
public:
    ForegroundTracker();
    ~ForegroundTracker();

    ForegroundTracker (const ForegroundTracker&) = delete;
    ForegroundTracker& operator= (const ForegroundTracker&) = delete;

    /** The last external foreground window, or nullptr if none is known or it has closed. */
    [[nodiscard]] HWND__* lastExternalWindow() const noexcept;

    void handleForegroundChange (HWND__* window) noexcept;

private:
    HWINEVENTHOOK__* hook = nullptr;
    HWND__* lastExternal = nullptr;
};

} // namespace stilleto
