// Win32 integration tests: global hotkey and system-wide text insertion into a real Notepad window.
//
// These drive the interactive desktop: they open Notepad on a temporary file, inject keystrokes and
// read the result back with WM_GETTEXT. Run on an unlocked desktop and do not type while they run.

#include "TestFramework.h"

#include "core/TextProcessing.h"
#include "windows/GlobalHotkey.h"
#include "windows/TextInserter.h"

#include <windows.h>
#include <shellapi.h>

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <thread>

using namespace stilleto;
using namespace std::chrono_literals;

namespace
{
void pumpMessagesFor (std::chrono::milliseconds duration)
{
    const auto end = std::chrono::steady_clock::now() + duration;
    while (std::chrono::steady_clock::now() < end)
    {
        MSG msg;
        while (PeekMessageW (&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage (&msg);
            DispatchMessageW (&msg);
        }
        std::this_thread::sleep_for (5ms);
    }
}

void sendKeys (std::initializer_list<WORD> vks)
{
    std::vector<INPUT> in;
    for (auto vk : vks) { INPUT i {}; i.type = INPUT_KEYBOARD; i.ki.wVk = vk; in.push_back (i); }
    for (auto it = vks.end(); it != vks.begin();)
    {
        --it;
        INPUT i {}; i.type = INPUT_KEYBOARD; i.ki.wVk = *it; i.ki.dwFlags = KEYEVENTF_KEYUP; in.push_back (i);
    }
    SendInput (static_cast<UINT> (in.size()), in.data(), sizeof (INPUT));
}

/** Test-only activation: a process that generated the last input event may set the foreground
    window, so a harmless unassigned key (0xE8) is injected first. */
bool forceForeground (HWND hwnd)
{
    for (int attempt = 0; attempt < 10; ++attempt)
    {
        // Fallback once key-based activation has failed (e.g. the taskbar holds the foreground
        // lock): a click on the test window's own title bar activates it like a real click.
        if (attempt == 3)
        {
            RECT r {};
            GetWindowRect (hwnd, &r);
            SetCursorPos ((r.left + r.right) / 2, r.top + 12);
            std::this_thread::sleep_for (100ms);
            INPUT click[2] {};
            click[0].type = click[1].type = INPUT_MOUSE;
            click[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
            click[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
            SendInput (2, click, sizeof (INPUT));
            std::this_thread::sleep_for (400ms);
            if (GetForegroundWindow() == hwnd)
                return true;
        }

        INPUT unassigned[2] {};
        unassigned[0].type = unassigned[1].type = INPUT_KEYBOARD;
        unassigned[0].ki.wVk = unassigned[1].ki.wVk = 0xE8;
        unassigned[1].ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput (2, unassigned, sizeof (INPUT));

        SetForegroundWindow (hwnd);
        std::this_thread::sleep_for (150ms);
        if (GetForegroundWindow() == hwnd)
            return true;
    }
    return false;
}

/** A Notepad window editing a fresh temporary file. */
struct NotepadSession
{
    std::filesystem::path file;
    HWND window = nullptr;

    explicit NotepadSession (const char* tag)
    {
        file = std::filesystem::temp_directory_path()
             / (std::string ("stilleto-test-") + tag + "-" + std::to_string (GetTickCount64()) + ".txt");
        std::ofstream (file).close();

        ShellExecuteW (nullptr, L"open", L"notepad.exe", file.wstring().c_str(), nullptr, SW_SHOWNORMAL);

        const auto stem = file.stem().wstring();
        const auto deadline = std::chrono::steady_clock::now() + 15s;

        // Find the window by title: when this test runs from a background console, Windows'
        // foreground lock may stop Notepad from activating on its own.
        struct Search { std::wstring stem; HWND found = nullptr; } search { stem };

        while (std::chrono::steady_clock::now() < deadline && search.found == nullptr)
        {
            EnumWindows ([] (HWND hwnd, LPARAM param) -> BOOL
            {
                auto& s = *reinterpret_cast<Search*> (param);
                wchar_t title[512] {};
                GetWindowTextW (hwnd, title, 512);
                if (IsWindowVisible (hwnd) && std::wstring (title).find (s.stem) != std::wstring::npos)
                {
                    s.found = hwnd;
                    return FALSE;
                }
                return TRUE;
            }, reinterpret_cast<LPARAM> (&search));

            if (search.found == nullptr)
                std::this_thread::sleep_for (100ms);
        }

        if (search.found != nullptr && forceForeground (search.found))
            window = search.found;

        std::this_thread::sleep_for (700ms);   // let the editor finish loading and place the caret
    }

    ~NotepadSession()
    {
        if (window != nullptr && IsWindow (window))
        {
            if (sendToNotepad ({ VK_CONTROL, 'S' }))   // save the temp file so closing never prompts
            {
                std::this_thread::sleep_for (400ms);
                sendToNotepad ({ VK_CONTROL, 'W' });   // close this tab only
            }
            std::this_thread::sleep_for (400ms);
        }
        std::error_code ec;
        std::filesystem::remove (file, ec);
    }

    [[nodiscard]] bool ok() const { return window != nullptr; }

    /** Reads the focused editor's text (Windows 11 Notepad: RichEditD2DPT; classic: Edit). */
    [[nodiscard]] std::string text() const
    {
        GUITHREADINFO info { sizeof (GUITHREADINFO) };
        HWND editor = nullptr;
        if (GetGUIThreadInfo (GetWindowThreadProcessId (window, nullptr), &info))
            editor = info.hwndFocus;

        if (editor == nullptr) return "<no focused editor>";

        DWORD_PTR length = 0;
        SendMessageTimeoutW (editor, WM_GETTEXTLENGTH, 0, 0, SMTO_ABORTIFHUNG, 2000, &length);
        std::wstring buffer (length + 1, L'\0');
        DWORD_PTR copied = 0;
        SendMessageTimeoutW (editor, WM_GETTEXT, buffer.size(), reinterpret_cast<LPARAM> (buffer.data()), SMTO_ABORTIFHUNG, 2000, &copied);
        buffer.resize (copied);
        return text::normaliseLineEndings (text::utf16ToUtf8 (std::u16string (buffer.begin(), buffer.end())));
    }

    /** Sends keys only while Notepad is foreground, so a focus change can never make the test
        press Ctrl+A / Delete in someone else's document. */
    bool sendToNotepad (std::initializer_list<WORD> vks) const
    {
        if (GetForegroundWindow() != window)
            forceForeground (window);
        if (GetForegroundWindow() != window)
        {
            std::printf ("    (skipped keys: Notepad is not in the foreground)\n");
            return false;
        }
        sendKeys (vks);
        return true;
    }

    void clear() const
    {
        if (! sendToNotepad ({ VK_CONTROL, 'A' })) return;
        std::this_thread::sleep_for (150ms);
        sendToNotepad ({ VK_DELETE });
        std::this_thread::sleep_for (200ms);
    }
};

std::string clipboardText()
{
    std::string result;
    if (OpenClipboard (nullptr))
    {
        if (HANDLE h = GetClipboardData (CF_UNICODETEXT))
            if (auto* p = static_cast<const char16_t*> (GlobalLock (h)))
            {
                result = text::utf16ToUtf8 (p);
                GlobalUnlock (h);
            }
        CloseClipboard();
    }
    return result;
}

InsertResult insertAndSettle (const std::string& utf8, const InsertionTarget& target, InsertOptions options = {})
{
    auto result = TextInserter::insert (text::utf8ToUtf16 (utf8), target, options);
    std::this_thread::sleep_for (400ms);   // Notepad processes injected input asynchronously
    return result;
}
} // namespace

//==============================================================================================
TEST_CASE ("hotkey: register, detect conflicts and unregister")
{
    const auto spec = *HotkeySpec::parse ("Ctrl+Alt+Shift+F11");

    GlobalHotkey first, second;
    REQUIRE (first.registerHotkey (spec, [] {}) == GlobalHotkey::Result::Ok);
    CHECK (first.isRegistered());
    CHECK (first.isAvailable (spec));                       // held by itself
    CHECK (! second.isAvailable (spec));                    // conflict visible to others
    CHECK (second.registerHotkey (spec, [] {}) == GlobalHotkey::Result::AlreadyInUse);

    first.pause();
    CHECK (first.isPaused() && ! first.isRegistered());
    CHECK (second.isAvailable (spec));
    CHECK (first.resume() == GlobalHotkey::Result::Ok);

    first.unregister();
    CHECK (! first.isRegistered());
    CHECK (second.registerHotkey (spec, [] {}) == GlobalHotkey::Result::Ok);
    second.unregister();

    CHECK (first.registerHotkey (HotkeySpec { 0, 'A' }, [] {}) == GlobalHotkey::Result::InvalidShortcut);
}

TEST_CASE ("hotkey: failed re-registration keeps the previous shortcut")
{
    const auto a = *HotkeySpec::parse ("Ctrl+Alt+Shift+F10");
    const auto b = *HotkeySpec::parse ("Ctrl+Alt+Shift+F9");

    GlobalHotkey blocker, subject;
    REQUIRE (blocker.registerHotkey (b, [] {}) == GlobalHotkey::Result::Ok);
    REQUIRE (subject.registerHotkey (a, [] {}) == GlobalHotkey::Result::Ok);
    CHECK (subject.registerHotkey (b, [] {}) == GlobalHotkey::Result::AlreadyInUse);
    CHECK (subject.isRegistered());
    CHECK (subject.currentSpec() == a);
    CHECK (! blocker.isAvailable (a));
}

TEST_CASE ("hotkey: fires while another application has focus")
{
    NotepadSession notepad ("hotkey");
    REQUIRE (notepad.ok());

    int presses = 0;
    GlobalHotkey hotkey;
    REQUIRE (hotkey.registerHotkey (*HotkeySpec::parse ("Ctrl+Alt+Shift+F12"), [&] { ++presses; }) == GlobalHotkey::Result::Ok);

    REQUIRE (GetForegroundWindow() == notepad.window);
    sendKeys ({ VK_CONTROL, VK_MENU, VK_SHIFT, VK_F12 });
    pumpMessagesFor (500ms);
    CHECK_EQ (presses, 1);
    CHECK (GetForegroundWindow() == notepad.window);   // the hotkey did not steal focus
    CHECK_EQ (notepad.text(), std::string());          // and did not type into the editor

    hotkey.unregister();
    REQUIRE (notepad.sendToNotepad ({ VK_CONTROL, VK_MENU, VK_SHIFT, VK_F12 }));
    pumpMessagesFor (300ms);
    CHECK_EQ (presses, 1);                              // no callback after unregistering
}

//==============================================================================================
TEST_CASE ("insert: sentence, punctuation, newlines and Unicode into Notepad")
{
    NotepadSession notepad ("insert");
    REQUIRE (notepad.ok());

    const auto target = TextInserter::captureForeground();
    REQUIRE (target.isValid());
    CHECK (target.window == notepad.window);
    std::printf ("    target: %s (%ls)\n", target.describe().c_str(), target.processName.c_str());

    {
        const std::string sentence = "The quick brown fox jumps over the lazy dog.";
        const auto r = insertAndSettle (sentence, target);
        CHECK (r.succeeded());
        CHECK (r.methodUsed == InsertMethod::ClipboardPaste);   // Automatic pastes
        CHECK_EQ (notepad.text(), sentence);
        notepad.clear();
    }

    {
        InsertOptions typed;
        typed.method = InsertMethod::UnicodeKeys;
        const std::string shortText = "Short and sweet.";
        const auto r = insertAndSettle (shortText, target, typed);
        CHECK (r.succeeded());
        CHECK (r.methodUsed == InsertMethod::UnicodeKeys);
        CHECK_EQ (notepad.text(), shortText);
        notepad.clear();
    }

    {
        const std::string punctuated = "Hello, world! Is this \"quoted\"? Yes: (100%) & more...\nSecond line.\n\nNew paragraph.";
        const auto r = insertAndSettle (punctuated, target);
        CHECK (r.succeeded());
        CHECK_EQ (notepad.text(), punctuated);
        notepad.clear();

        // Typed newlines use Shift+Enter (safe in chat apps) and must still produce line breaks.
        InsertOptions typed;
        typed.method = InsertMethod::UnicodeKeys;
        const std::string lines = "Line one.\nLine two!\n\n(3) \"end\"";
        const auto r2 = insertAndSettle (lines, target, typed);
        CHECK (r2.succeeded());
        CHECK_EQ (notepad.text(), lines);
        notepad.clear();
    }

    {
        // 26 UTF-16 units including a surrogate pair, both typed and pasted.
        const std::string unicode = "Café naïve — 日本語 🎙 Привет";
        InsertOptions typed;
        typed.method = InsertMethod::UnicodeKeys;
        const auto r = insertAndSettle (unicode, target, typed);
        CHECK (r.succeeded());
        CHECK (r.methodUsed == InsertMethod::UnicodeKeys);
        CHECK_EQ (notepad.text(), unicode);
        notepad.clear();

        InsertOptions pasted;
        pasted.method = InsertMethod::ClipboardPaste;
        const auto r2 = insertAndSettle (unicode, target, pasted);
        CHECK (r2.succeeded());
        CHECK_EQ (notepad.text(), unicode);
        notepad.clear();
    }

    CHECK (GetForegroundWindow() == notepad.window);
}

TEST_CASE ("insert: long text uses the clipboard and restores previous contents")
{
    NotepadSession notepad ("clipboard");
    REQUIRE (notepad.ok());

    const std::string original = "Clipboard contents that belong to the user \xE2\x9C\x93";
    REQUIRE (TextInserter::copyToClipboard (text::utf8ToUtf16 (original)));

    std::string longText;
    while (longText.size() < 2000)
        longText += "Stilleto inserts longer dictation through a clipboard paste and puts the clipboard back. ";

    const auto target = TextInserter::captureForeground();
    REQUIRE (target.isValid());

    InsertOptions options;
    const auto r = insertAndSettle (longText, target, options);
    CHECK (r.succeeded());
    CHECK (r.methodUsed == InsertMethod::ClipboardPaste);
    CHECK (r.clipboardRestored);
    CHECK_EQ (notepad.text(), longText);
    CHECK_EQ (clipboardText(), original);
}

TEST_CASE ("insert: forced typing never touches the clipboard")
{
    // A fresh editor: right after a large paste Notepad is busy and even the test's own Ctrl+A can
    // arrive as plain text, which would make this check measure the harness instead of the inserter.
    NotepadSession notepad ("typing");
    REQUIRE (notepad.ok());

    const std::string original = "Clipboard contents that belong to the user";
    REQUIRE (TextInserter::copyToClipboard (text::utf8ToUtf16 (original)));

    const auto target = TextInserter::captureForeground();
    REQUIRE (target.isValid());

    InsertOptions options;
    options.method = InsertMethod::UnicodeKeys;
    const std::string medium = "Stilleto types this short phrase key by key.";
    const auto r = insertAndSettle (medium, target, options);
    CHECK (r.succeeded());
    CHECK (r.methodUsed == InsertMethod::UnicodeKeys);
    CHECK_EQ (notepad.text(), medium);
    CHECK_EQ (clipboardText(), original);
}

TEST_CASE ("insert: a clipboard with many formats is restored and the user is told about losses")
{
    NotepadSession notepad ("manyformats");
    REQUIRE (notepad.ok());

    // Text plus 20 private formats (> 16): only priority formats are captured, so a notice is due.
    const std::string original = "Rich clipboard from another application";
    REQUIRE (OpenClipboard (nullptr));
    EmptyClipboard();
    const auto wide = text::utf8ToUtf16 (original) + u'\0';
    auto put = [] (UINT format, const void* data, size_t size)
    {
        HGLOBAL m = GlobalAlloc (GMEM_MOVEABLE, size);
        std::memcpy (GlobalLock (m), data, size);
        GlobalUnlock (m);
        SetClipboardData (format, m);
    };
    put (CF_UNICODETEXT, wide.data(), wide.size() * sizeof (char16_t));
    for (int i = 0; i < 20; ++i)
    {
        const auto name = L"StilletoTestFormat" + std::to_wstring (i);
        const int value = i;
        put (RegisterClipboardFormatW (name.c_str()), &value, sizeof (value));
    }
    CloseClipboard();

    const auto target = TextInserter::captureForeground();
    REQUIRE (target.isValid());
    const auto r = insertAndSettle ("Pasted while the clipboard held many formats.", target);
    CHECK (r.succeeded());
    CHECK (! r.clipboardRestored);
    CHECK (! r.clipboardNote.empty());
    CHECK_EQ (clipboardText(), original);   // the text itself is always restored
    notepad.clear();
}

TEST_CASE ("insert: closed targets and own windows are refused")
{
    InsertionTarget gone;
    gone.window = reinterpret_cast<HWND> (static_cast<std::uintptr_t> (0x7FFFFFF0));   // not a window
    const auto r = TextInserter::insert (u"text", gone, {});
    CHECK (r.status == InsertResult::Status::TargetClosed);
    CHECK (! r.succeeded());

    const auto empty = TextInserter::insert (u"", gone, {});
    CHECK (! empty.succeeded());

    CHECK (! TextInserter::isTargetElevatedAboveUs (GetCurrentProcessId()));
}

TEST_CASE ("tune: keystroke pacing (diagnostic, set STILLETO_TUNE=1)")
{
    if (std::getenv ("STILLETO_TUNE") == nullptr) { std::printf ("    SKIPPED\n"); return; }

    NotepadSession notepad ("tune");
    REQUIRE (notepad.ok());
    const auto target = TextInserter::captureForeground();

    const std::string sample = "Pacing check: the quick brown fox jumps over the lazy dog, caf\xC3\xA9 \xE2\x80\x94 \xF0\x9F\x8E\x99 done. "
                               "Second sentence makes this a realistic dictation length for one utterance.";

    if (std::string (std::getenv ("STILLETO_TUNE")) == "2")
    {
        const std::u16string letters = u"abcdefghijklmnopqrstuvwxyz";
        for (std::size_t i = 0; i < letters.size(); ++i)
        {
            INPUT in[2] {};
            in[0].type = in[1].type = INPUT_KEYBOARD;
            in[0].ki.wScan = in[1].ki.wScan = letters[i];
            in[0].ki.dwFlags = KEYEVENTF_UNICODE;
            in[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
            SendInput (2, in, sizeof (INPUT));
            std::this_thread::sleep_for (120ms);
            SHORT ctrl = GetAsyncKeyState (VK_CONTROL), alt = GetAsyncKeyState (VK_MENU), shift = GetAsyncKeyState (VK_SHIFT);
            std::printf ("    sent '%c' fg=%s ctrl=%x alt=%x shift=%x -> [%s]\n", static_cast<char> (letters[i]),
                         GetForegroundWindow() == notepad.window ? "notepad" : "OTHER", ctrl & 0x8000, alt & 0x8000, shift & 0x8000,
                         notepad.text().c_str());
        }
        notepad.clear();
        return;
    }

    struct Config { std::size_t batch; int delay; };
    for (const auto c : { Config { 24, 0 }, Config { 1, 10 }, Config { 1, 20 }, Config { 1, 30 }, Config { 1, 35 } })
    {
        InsertOptions o;
        o.method = InsertMethod::UnicodeKeys;
        o.unitsPerBatch = c.batch;
        o.chunkDelayMs = c.delay;
        const auto start = std::chrono::steady_clock::now();
        const auto r = insertAndSettle (sample, target, o);
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds> (std::chrono::steady_clock::now() - start).count() - 400;
        const bool match = notepad.text() == sample;
        std::printf ("    batch=%2zu delay=%dms -> %s (%lld ms)\n", c.batch, c.delay, match && r.succeeded() ? "exact" : "GARBLED", ms);
        notepad.clear();
    }
}

int main (int argc, char** argv)
{
    std::printf ("Stilleto platform tests: Notepad will open briefly. Do not type until they finish.\n\n");
    return stilleto::test::runAll (argc > 1 ? argv[1] : nullptr);
}
