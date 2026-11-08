#pragma once

#include <atomic>
#include <cstddef>
#include <string>

struct HWND__;

namespace stilleto
{

/** The window that should receive dictated text, captured before anything can change focus. */
struct InsertionTarget
{
    HWND__* window = nullptr;          // top-level foreground window
    HWND__* focus = nullptr;           // focused control inside it, when Windows exposes it
    unsigned long processId = 0;
    std::wstring processName;          // e.g. "notepad.exe"
    std::wstring windowTitle;

    [[nodiscard]] bool isValid() const noexcept { return window != nullptr; }
    [[nodiscard]] bool stillExists() const noexcept;
    [[nodiscard]] std::string describe() const;   // UTF-8 "Notepad — Untitled" style label
};

enum class InsertMethod
{
    Automatic,      // clipboard paste with restore; paced typing if the clipboard is locked
    UnicodeKeys,
    ClipboardPaste
};

struct InsertOptions
{
    InsertMethod method = InsertMethod::Automatic;

    // Measured on Windows 11 Notepad (tests/PlatformTests.cpp, "tune"): KEYEVENTF_UNICODE input
    // faster than ~30 ms per character is garbled (letters dropped or repeated), WaitForInputIdle
    // does not help, and even 35 ms garbles while the editor is busy. Clipboard paste was exact in
    // every run, so Automatic pastes (restoring the user's clipboard) and simulated typing is used
    // only when chosen explicitly or when the clipboard is locked by another application.
    std::size_t clipboardThreshold = 0;     // Automatic types text up to this many UTF-16 units
    std::size_t unitsPerBatch = 1;          // characters per SendInput call
    int chunkDelayMs = 35;                  // pause between keystroke batches
    int pasteSettleMs = 300;                // time the target gets to read the clipboard before restore
    bool restoreClipboard = true;
    bool newlinesAsShiftEnter = true;       // Enter alone would send the message in chat apps
    bool appendTrailingSpace = false;

    // When the target is no longer the foreground window: false = do nothing (hotkey dictation
    // must never pull focus back from wherever the user went); true = bring the target forward
    // first (the user explicitly clicked Insert in Stilleto).
    bool activateTarget = false;

    // Bring the target forward only if the foreground is Stilleto itself or a shell surface such
    // as the taskbar (dictation started from the tray menu, which takes the foreground).
    bool activateWhenStilletoForeground = false;
};

struct InsertResult
{
    enum class Status
    {
        Inserted,
        PartiallyInserted,
        TargetClosed,
        TargetChanged,        // focus moved to another window; nothing (more) was typed
        BlockedByElevation,   // target runs as administrator; UIPI drops injected input
        ClipboardUnavailable,
        Failed
    };

    Status status = Status::Failed;
    InsertMethod methodUsed = InsertMethod::UnicodeKeys;
    std::size_t unitsSent = 0;
    std::size_t unitsTotal = 0;
    bool clipboardRestored = false;
    std::string clipboardNote;   // user-facing note when the previous clipboard was not fully restored
    std::string message;   // user-facing explanation

    [[nodiscard]] bool succeeded() const noexcept { return status == Status::Inserted; }
};

/** System-wide text insertion via SendInput. All functions are blocking and may sleep briefly;
    the application calls insert() from a worker thread. */
class TextInserter
{
public:
    /** Captures the current foreground window. Returns an invalid target if the foreground window
        belongs to this process (dictating into Stilleto itself goes through its own UI). */
    [[nodiscard]] static InsertionTarget captureForeground();

    /** Builds a target from a known window handle (e.g. the last external window). */
    [[nodiscard]] static InsertionTarget targetFromWindow (HWND__* window);

    /** Inserts UTF-16 text into the target. Text is never typed into a different window: if the
        target is no longer in the foreground and cannot be restored, nothing is sent. */
    [[nodiscard]] static InsertResult insert (const std::u16string& text,
                                              const InsertionTarget& target,
                                              const InsertOptions& options,
                                              const std::atomic<bool>* cancel = nullptr);

    /** Puts text on the clipboard (Copy action). Returns false if the clipboard is locked. */
    static bool copyToClipboard (const std::u16string& text);

    /** True when the target process runs at a higher integrity level than this one. */
    [[nodiscard]] static bool isTargetElevatedAboveUs (unsigned long processId);
};

} // namespace stilleto
