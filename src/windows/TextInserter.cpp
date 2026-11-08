#include "windows/TextInserter.h"

#include "core/TextProcessing.h"

#include <windows.h>

#include <algorithm>
#include <initializer_list>
#include <chrono>
#include <cstring>
#include <thread>
#include <vector>

namespace stilleto
{
namespace
{
using Clock = std::chrono::steady_clock;

void sleepMs (int ms) { std::this_thread::sleep_for (std::chrono::milliseconds (ms)); }

std::string narrow (const std::wstring& w)
{
    return text::utf16ToUtf8 (std::u16string (w.begin(), w.end()));
}

std::wstring processNameFor (DWORD pid)
{
    std::wstring name;
    if (HANDLE process = OpenProcess (PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid))
    {
        wchar_t buffer[MAX_PATH] {};
        DWORD size = MAX_PATH;
        if (QueryFullProcessImageNameW (process, 0, buffer, &size))
        {
            name.assign (buffer, size);
            if (const auto slash = name.find_last_of (L"\\/"); slash != std::wstring::npos)
                name = name.substr (slash + 1);
        }
        CloseHandle (process);
    }
    return name;
}

DWORD integrityLevelOf (HANDLE process)
{
    DWORD level = SECURITY_MANDATORY_MEDIUM_RID;
    HANDLE token = nullptr;

    if (OpenProcessToken (process, TOKEN_QUERY, &token))
    {
        DWORD size = 0;
        GetTokenInformation (token, TokenIntegrityLevel, nullptr, 0, &size);
        std::vector<std::byte> buffer (size);

        if (size > 0 && GetTokenInformation (token, TokenIntegrityLevel, buffer.data(), size, &size))
        {
            const auto* label = reinterpret_cast<TOKEN_MANDATORY_LABEL*> (buffer.data());
            const auto count = *GetSidSubAuthorityCount (label->Label.Sid);
            level = *GetSidSubAuthority (label->Label.Sid, static_cast<DWORD> (count - 1));
        }

        CloseHandle (token);
    }

    return level;
}

bool anyModifierHeld()
{
    for (int vk : { VK_CONTROL, VK_MENU, VK_SHIFT, VK_LWIN, VK_RWIN })
        if (GetAsyncKeyState (vk) & 0x8000)
            return true;
    return false;
}

/** Waits for the user to release the shortcut keys; injected characters combined with a held
    Ctrl or Alt would trigger application shortcuts instead of typing. */
bool waitForModifiersReleased (int timeoutMs)
{
    const auto deadline = Clock::now() + std::chrono::milliseconds (timeoutMs);
    while (anyModifierHeld())
    {
        if (Clock::now() > deadline) return false;
        sleepMs (15);
    }
    return true;
}

/** Makes the target the foreground window if it is not already. Only succeeds when Windows
    permits it (e.g. Stilleto itself is foreground because the user clicked Insert). */
bool ensureForeground (HWND target)
{
    if (GetForegroundWindow() == target)
        return true;

    if (IsIconic (target))
        ShowWindow (target, SW_RESTORE);

    SetForegroundWindow (target);

    // If a window of another thread is foreground, Windows may refuse; attaching input queues to the
    // current foreground thread for the duration of the call is the documented workaround.
    if (GetForegroundWindow() != target)
    {
        const DWORD foregroundThread = GetWindowThreadProcessId (GetForegroundWindow(), nullptr);
        const DWORD thisThread = GetCurrentThreadId();

        if (foregroundThread != 0 && foregroundThread != thisThread && AttachThreadInput (thisThread, foregroundThread, TRUE))
        {
            SetForegroundWindow (target);
            BringWindowToTop (target);
            AttachThreadInput (thisThread, foregroundThread, FALSE);
        }
    }

    const auto deadline = Clock::now() + std::chrono::milliseconds (600);
    while (GetForegroundWindow() != target)
    {
        if (Clock::now() > deadline) return false;
        sleepMs (15);
    }

    sleepMs (60);   // let the target restore its caret before input arrives
    return true;
}

INPUT keyInput (WORD vk, bool down)
{
    INPUT in {};
    in.type = INPUT_KEYBOARD;
    in.ki.wVk = vk;
    in.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
    return in;
}

INPUT unicodeInput (wchar_t unit, bool down)
{
    INPUT in {};
    in.type = INPUT_KEYBOARD;
    in.ki.wScan = unit;
    in.ki.dwFlags = KEYEVENTF_UNICODE | (down ? 0 : KEYEVENTF_KEYUP);
    return in;
}

/** One logical character's worth of input events, so batches never split a surrogate pair. */
void appendUnitEvents (std::vector<INPUT>& events, char16_t unit, bool shiftEnter)
{
    if (unit == u'\n')
    {
        if (shiftEnter) events.push_back (keyInput (VK_SHIFT, true));
        events.push_back (keyInput (VK_RETURN, true));
        events.push_back (keyInput (VK_RETURN, false));
        if (shiftEnter) events.push_back (keyInput (VK_SHIFT, false));
    }
    else if (unit == u'\r')
    {
        // dropped: newlines are normalised to '\n'
    }
    else
    {
        events.push_back (unicodeInput (static_cast<wchar_t> (unit), true));
        events.push_back (unicodeInput (static_cast<wchar_t> (unit), false));
    }
}

InsertResult typeUnicode (const std::u16string& text, HWND target, const InsertOptions& options)
{
    InsertResult result;
    result.methodUsed = InsertMethod::UnicodeKeys;
    result.unitsTotal = text.size();

    const std::size_t unitsPerBatch = std::max<std::size_t> (1, options.unitsPerBatch);
    std::vector<INPUT> events;
    events.reserve (unitsPerBatch * 4 + 4);

    std::size_t i = 0;
    while (i < text.size())
    {
        if (GetForegroundWindow() != target)
        {
            result.status = result.unitsSent == 0 ? InsertResult::Status::TargetChanged : InsertResult::Status::PartiallyInserted;
            result.message = "Focus moved to another window, so typing stopped to avoid inserting text in the wrong place.";
            return result;
        }

        events.clear();
        std::size_t end = std::min (text.size(), i + unitsPerBatch);
        // Keep surrogate pairs together.
        if (end < text.size() && text[end - 1] >= 0xD800 && text[end - 1] <= 0xDBFF)
            ++end;

        for (std::size_t k = i; k < end; ++k)
            appendUnitEvents (events, text[k], options.newlinesAsShiftEnter);

        if (! events.empty())
        {
            const UINT sent = SendInput (static_cast<UINT> (events.size()), events.data(), sizeof (INPUT));

            if (sent != events.size())
            {
                // SendInput is blocked by UIPI or another input injector; nothing more can be sent.
                result.status = result.unitsSent == 0 ? InsertResult::Status::Failed : InsertResult::Status::PartiallyInserted;
                result.message = "Windows blocked the simulated keystrokes (the target may be running as administrator or be a secure field).";
                return result;
            }
        }

        result.unitsSent = end;
        i = end;

        if (options.chunkDelayMs > 0 && i < text.size())
            sleepMs (options.chunkDelayMs);
    }

    result.status = InsertResult::Status::Inserted;
    return result;
}

//==============================================================================================
// Clipboard snapshot/restore. Only HGLOBAL-backed formats can be copied; GDI handle formats
// (bitmaps, metafiles, palettes) and owner-display formats are skipped, and formats Windows can
// synthesise from others are not duplicated.
struct ClipboardSnapshot
{
    struct Item
    {
        UINT format;
        std::vector<std::byte> data;
    };

    std::vector<Item> items;
    bool complete = true;   // false if something could not be captured
};

bool openClipboardWithRetry (HWND owner)
{
    for (int attempt = 0; attempt < 20; ++attempt)
    {
        if (OpenClipboard (owner)) return true;
        sleepMs (15);
    }
    return false;
}

bool isCopyableFormat (UINT format)
{
    switch (format)
    {
        case CF_BITMAP: case CF_METAFILEPICT: case CF_PALETTE: case CF_ENHMETAFILE:
        case CF_OWNERDISPLAY: case CF_DSPBITMAP: case CF_DSPMETAFILEPICT: case CF_DSPENHMETAFILE:
        case CF_TEXT: case CF_OEMTEXT:           // synthesised from CF_UNICODETEXT
            return false;
        default:
            return ! (format >= CF_GDIOBJFIRST && format <= CF_GDIOBJLAST);
    }
}

ClipboardSnapshot captureClipboard()   // clipboard must be open
{
    ClipboardSnapshot snapshot;
    constexpr std::size_t maxTotalBytes = 64u * 1024u * 1024u;
    std::size_t total = 0;

    for (UINT format = EnumClipboardFormats (0); format != 0; format = EnumClipboardFormats (format))
    {
        // GDI formats are usually synthesised from CF_DIB / CF_UNICODETEXT, so skipping them is not a loss.
        if (! isCopyableFormat (format))
            continue;

        HANDLE handle = GetClipboardData (format);
        if (handle == nullptr) continue;

        const SIZE_T size = GlobalSize (handle);
        void* source = size > 0 ? GlobalLock (handle) : nullptr;
        if (source == nullptr) { snapshot.complete = false; continue; }

        if (total + size > maxTotalBytes)
        {
            GlobalUnlock (handle);
            snapshot.complete = false;
            continue;
        }

        ClipboardSnapshot::Item item { format, std::vector<std::byte> (size) };
        std::memcpy (item.data.data(), source, size);
        GlobalUnlock (handle);

        total += size;
        snapshot.items.push_back (std::move (item));
    }

    return snapshot;
}

bool setGlobalData (UINT format, const void* data, std::size_t size)
{
    HGLOBAL memory = GlobalAlloc (GMEM_MOVEABLE, std::max<std::size_t> (size, 1));
    if (memory == nullptr) return false;

    if (void* dest = GlobalLock (memory))
    {
        if (size > 0) std::memcpy (dest, data, size);
        GlobalUnlock (memory);

        if (SetClipboardData (format, memory) != nullptr)
            return true;   // the clipboard now owns the memory
    }

    GlobalFree (memory);
    return false;
}

/** Writes text plus the formats asking clipboard managers / Windows clipboard history not to keep
    this transient content. */
bool putTextOnClipboard (const std::u16string& text, bool markTransient)   // clipboard must be open
{
    if (! EmptyClipboard()) return false;

    std::u16string withNull = text;
    withNull.push_back (u'\0');

    if (! setGlobalData (CF_UNICODETEXT, withNull.data(), withNull.size() * sizeof (char16_t)))
        return false;

    if (markTransient)
    {
        const DWORD zero = 0;
        if (UINT f = RegisterClipboardFormatW (L"ExcludeClipboardContentFromMonitorProcessing")) setGlobalData (f, &zero, sizeof (zero));
        if (UINT f = RegisterClipboardFormatW (L"CanIncludeInClipboardHistory"))                 setGlobalData (f, &zero, sizeof (zero));
        if (UINT f = RegisterClipboardFormatW (L"CanUploadToCloudClipboard"))                    setGlobalData (f, &zero, sizeof (zero));
    }

    return true;
}

InsertResult pasteViaClipboard (const std::u16string& text, HWND target, const InsertOptions& options)
{
    InsertResult result;
    result.methodUsed = InsertMethod::ClipboardPaste;
    result.unitsTotal = text.size();

    if (! openClipboardWithRetry (nullptr))
    {
        result.status = InsertResult::Status::ClipboardUnavailable;
        result.message = "The clipboard is in use by another application.";
        return result;
    }

    ClipboardSnapshot snapshot;
    if (options.restoreClipboard)
        snapshot = captureClipboard();

    const bool written = putTextOnClipboard (text, true);
    CloseClipboard();

    if (! written)
    {
        result.status = InsertResult::Status::ClipboardUnavailable;
        result.message = "Stilleto could not write to the clipboard.";
        return result;
    }

    const DWORD ourSequence = GetClipboardSequenceNumber();

    if (GetForegroundWindow() != target)
    {
        result.status = InsertResult::Status::TargetChanged;
        result.message = "Focus moved to another window before pasting.";
    }
    else
    {
        INPUT keys[] = { keyInput (VK_CONTROL, true), keyInput ('V', true), keyInput ('V', false), keyInput (VK_CONTROL, false) };
        const UINT sent = SendInput (4, keys, sizeof (INPUT));

        if (sent == 4)
        {
            result.status = InsertResult::Status::Inserted;
            result.unitsSent = text.size();
        }
        else
        {
            result.status = InsertResult::Status::Failed;
            result.message = "Windows blocked the paste shortcut (the target may be running as administrator).";
        }
    }

    if (options.restoreClipboard)
    {
        // The target reads the clipboard asynchronously while handling Ctrl+V.
        sleepMs (std::max (50, options.pasteSettleMs));

        // Restore only if nothing else wrote to the clipboard meanwhile.
        if (GetClipboardSequenceNumber() == ourSequence && openClipboardWithRetry (nullptr))
        {
            EmptyClipboard();
            bool allRestored = snapshot.complete;
            for (const auto& item : snapshot.items)
                allRestored = setGlobalData (item.format, item.data.data(), item.data.size()) && allRestored;
            CloseClipboard();
            result.clipboardRestored = allRestored;
        }
    }

    return result;
}
} // namespace

//==============================================================================================
bool InsertionTarget::stillExists() const noexcept
{
    return window != nullptr && IsWindow (window);
}

std::string InsertionTarget::describe() const
{
    std::wstring label = windowTitle;
    if (label.size() > 60) label = label.substr (0, 57) + L"...";
    if (label.empty()) label = processName;
    return label.empty() ? std::string ("the active window") : narrow (label);
}

InsertionTarget TextInserter::targetFromWindow (HWND__* window)
{
    InsertionTarget target;
    if (window == nullptr || ! IsWindow (window))
        return target;

    // Compare like with like: GetForegroundWindow() returns top-level windows, so normalise child
    // handles to their root (but not to an owner, which may be a hidden window).
    if (HWND root = GetAncestor (window, GA_ROOT))
        window = root;

    DWORD pid = 0;
    const DWORD thread = GetWindowThreadProcessId (window, &pid);
    if (pid == GetCurrentProcessId())
        return {};

    target.window = window;
    target.processId = pid;
    target.processName = processNameFor (pid);

    wchar_t title[256] {};
    GetWindowTextW (window, title, 256);
    target.windowTitle = title;

    GUITHREADINFO info { sizeof (GUITHREADINFO) };
    if (GetGUIThreadInfo (thread, &info))
        target.focus = info.hwndFocus;

    return target;
}

InsertionTarget TextInserter::captureForeground()
{
    return targetFromWindow (GetForegroundWindow());
}

bool TextInserter::isTargetElevatedAboveUs (unsigned long processId)
{
    HANDLE process = OpenProcess (PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (process == nullptr)
        return GetLastError() == ERROR_ACCESS_DENIED;   // protected/elevated processes refuse even this

    const DWORD theirs = integrityLevelOf (process);
    CloseHandle (process);
    return theirs > integrityLevelOf (GetCurrentProcess());
}

bool TextInserter::copyToClipboard (const std::u16string& text)
{
    if (! openClipboardWithRetry (nullptr))
        return false;

    const bool ok = putTextOnClipboard (text, false);
    CloseClipboard();
    return ok;
}

InsertResult TextInserter::insert (const std::u16string& rawText, const InsertionTarget& target, const InsertOptions& options)
{
    InsertResult result;

    std::u16string payload;
    payload.reserve (rawText.size() + 1);
    for (std::size_t i = 0; i < rawText.size(); ++i)
    {
        if (rawText[i] == u'\r')
        {
            payload.push_back (u'\n');
            if (i + 1 < rawText.size() && rawText[i + 1] == u'\n') ++i;
        }
        else payload.push_back (rawText[i]);
    }

    if (options.appendTrailingSpace && ! payload.empty() && payload.back() != u'\n' && payload.back() != u' ')
        payload.push_back (u' ');

    result.unitsTotal = payload.size();

    if (payload.empty())
    {
        result.status = InsertResult::Status::Failed;
        result.message = "There is no text to insert.";
        return result;
    }

    if (! target.stillExists())
    {
        result.status = InsertResult::Status::TargetClosed;
        result.message = "The window you were dictating into has been closed.";
        return result;
    }

    if (isTargetElevatedAboveUs (target.processId))
    {
        result.status = InsertResult::Status::BlockedByElevation;
        result.message = "This application is running as administrator, so Windows does not allow Stilleto to type into it. "
                         "The transcript is available to copy.";
        return result;
    }

    if (! ensureForeground (target.window))
    {
        result.status = InsertResult::Status::TargetChanged;
        result.message = "The target window is no longer active, so nothing was typed. The transcript is available to copy.";
        return result;
    }

    if (! waitForModifiersReleased (2000))
    {
        result.status = InsertResult::Status::Failed;
        result.message = "Modifier keys are still held down. Release Ctrl, Alt, Shift and Win, then insert again.";
        return result;
    }

    const bool useClipboard = options.method == InsertMethod::ClipboardPaste
                              || (options.method == InsertMethod::Automatic && payload.size() > options.clipboardThreshold);

    if (useClipboard)
    {
        result = pasteViaClipboard (payload, target.window, options);

        // Clipboard locked by another app: typing is a safe fallback.
        if (result.status == InsertResult::Status::ClipboardUnavailable)
            result = typeUnicode (payload, target.window, options);
    }
    else
    {
        result = typeUnicode (payload, target.window, options);
    }

    if (result.succeeded() && result.message.empty())
        result.message = "Inserted into " + target.describe();

    return result;
}

} // namespace stilleto
