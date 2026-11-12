# Stilleto

**Your voice. Anywhere you type.**

Stilleto is a native Windows voice-typing application. Put the cursor in any text field (Notepad,
Word, a browser, WhatsApp Desktop, Visual Studio, ChatGPT), press **Ctrl + Alt + Space**, speak,
press it again, and Stilleto transcribes your speech **locally** with whisper.cpp and inserts the
text at the cursor. No account, no cloud API and no internet connection are needed for transcription.

Built with C++20, JUCE 9 and whisper.cpp v1.9.5.

---

## Features

- **System-wide shortcut** (`RegisterHotKey`): works while any other application has focus; conflict
  detection, rebinding in Settings, pause/resume from the tray.
- **Offline speech recognition** with whisper.cpp on a worker thread; the model is loaded once and kept
  in memory. Cancellable inference.
- **Text insertion** into the window that was active when you pressed the shortcut, via clipboard paste
  (your clipboard is snapshotted and restored, and the transient text is excluded from clipboard
  history) or paced Unicode keystrokes. Never steals focus.
- **Clean transcripts for natural speech**: filler sounds ("um", "uh", "erm"), stammered starts
  ("I-I-I", "w-what") and accidentally repeated words ("the the") are removed before typing, while
  intentional repeats ("that that", "had had") and real hyphenated words are kept. This is on by
  default and can be changed in *Settings > Text insertion > Remove fillers and stutters*.
- **Floating widget**: a non-activating, always-on-top pill with live waveform, timer and status.
- **Dashboard**: overview with real usage metrics, dictation page, searchable history, model manager
  with verified downloads, settings and help.
- **Tray icon**: open, start/stop dictation, settings, pause shortcut, quit.
- **Privacy**: raw audio is never written to disk unless you turn on "Save raw recordings"; history can
  be disabled or cleared.

## Requirements

- Windows 10 or 11 (x64)
- Visual Studio 2022 (17.x) or Visual Studio 2026 (18.x) with the *Desktop development with C++* workload
- CMake 3.25 or newer (the copy bundled with Visual Studio works)
- JUCE 9.x. By default the build looks for `external/JUCE`, then for `../../JUCE` relative to this
  folder (the `D:\Audio Development\JUCE` checkout in the reference environment). Override it with
  `-DSTILLETO_JUCE_DIR=<path>`.
- whisper.cpp, included as a git submodule pinned to `v1.9.5`

## Getting the source

```powershell
git clone --recurse-submodules https://github.com/Joshypeace/Stilleto.git
cd Stilleto
# If you cloned without --recurse-submodules:
git submodule update --init --recursive
```

## Model setup

Stilleto needs one Whisper model in ggml format. `base.en` (142 MB) is recommended.

**Option A: in the app.** Open **Models** and click **Download** next to *Base (English)*. Stilleto
confirms the size, downloads from `huggingface.co/ggerganov/whisper.cpp`, verifies the published
SHA-1, and only then installs the file into `%LOCALAPPDATA%\Stilleto\models`.

**Option B: manually.** Download `ggml-base.en.bin` from
<https://huggingface.co/ggerganov/whisper.cpp/tree/main> and put it in either:

- `%LOCALAPPDATA%\Stilleto\models` (the default model folder; changeable on the Models page), or
- `models\` in this repository (picked up automatically by development builds), or
- `models\` next to `Stilleto.exe`.

Model files are excluded from git. See [models/README.md](models/README.md) for checksums.

## Building

### Visual Studio solution (recommended)

```powershell
# Visual Studio 2022
cmake --preset vs2022
cmake --build --preset vs2022-release      # or vs2022-debug

# Visual Studio 2026
cmake --preset vs2026
cmake --build --preset vs2026-release      # or vs2026-debug
```

Open `out\build\vs2022\Stilleto.sln` (or `vs2026`) to work in the IDE. Set **Stilleto** as the
startup project.

### Visual Studio "Open Folder" / Ninja

Open the folder in Visual Studio and pick the **x64 Debug (Ninja)** or **x64 Release (Ninja)** preset.
From a *Developer PowerShell*:

```powershell
cmake --preset x64-release
cmake --build --preset x64-release
```

### Output

```
out\build\<preset>\Stilleto_artefacts\<Config>\Stilleto.exe
```

The executable uses the static MSVC runtime, so it needs no Visual C++ redistributable. Copy it
together with a model, or let it download one.

### Build options

| Option | Default | Meaning |
| --- | --- | --- |
| `STILLETO_JUCE_DIR` | auto | Path to JUCE |
| `STILLETO_BUILD_TESTS` | `ON` | Build the test executables |
| `STILLETO_GPU_VULKAN` | `OFF` | Build whisper.cpp with Vulkan (requires the Vulkan SDK) |
| `STILLETO_TEST_HOOKS` | `OFF` | Compile the `STILLETO_TEST_AUDIO` end-to-end hook (test builds only) |
| `STILLETO_NATIVE_CPU` | `OFF` | Optimise for this machine's CPU only (not for distribution); the default targets AVX2 |

## Running

Run `Stilleto.exe`. The first launch opens the dashboard; closing the window keeps Stilleto running
in the tray, so the shortcut keeps working. Use **Quit Stilleto** in the tray menu to exit.

- `Stilleto.exe --minimized` starts in the tray (used by *Launch at Windows sign-in*).
- Only one instance runs; launching it again brings the existing window forward.

Data locations:

| What | Where |
| --- | --- |
| Settings | `%APPDATA%\Stilleto\settings.xml` |
| History | `%APPDATA%\Stilleto\history.json` |
| Log | `%APPDATA%\Stilleto\stilleto.log` |
| Models | `%LOCALAPPDATA%\Stilleto\models` |
| Raw recordings (opt-in only) | `%APPDATA%\Stilleto\recordings` |

## Installing on another PC

Download or copy one of the release packages produced by `scripts\Build-Release.ps1`:

| Package | Use it when |
| --- | --- |
| `Stilleto-<version>-Setup.msi` | Normal install: Program Files, Start Menu and desktop shortcuts, Settings > Apps uninstall entry. Needs administrator approval. For no desktop shortcut: `msiexec /i Stilleto-<version>-Setup.msi INSTALLDESKTOPSHORTCUT=0`. |
| `Stilleto-<version>-Portable.zip` | No install or admin rights: unzip anywhere and run `Stilleto.exe`. Keep the `models` folder next to it. |

Both include the recommended `base.en` model, so dictation works offline immediately, and the
*Stilleto Quick Start* guide.

**Target PC requirements:** 64-bit Windows 10 or 11, a processor with AVX2, FMA and BMI2 (Intel 4th
generation / AMD Ryzen or newer; Stilleto shows a clear message otherwise), a microphone, and about 200 MB of disk
space. No Visual C++ redistributable or other runtime is needed.

**First launch:** the test builds are not code-signed, so Windows SmartScreen may say "Windows protected
your PC". Choose *More info > Run anyway*. If dictation hears nothing, check *Windows Settings > Privacy &
security > Microphone > Let desktop apps access your microphone*.

**Uninstall:** *Settings > Apps > Stilleto*, or the Start Menu shortcut. Settings and history in
`%APPDATA%\Stilleto` and downloaded models in `%LOCALAPPDATA%\Stilleto\models` (possibly several
hundred MB) are kept; delete those folders to remove them. Uninstall also removes the uninstalling
user's "Launch at Windows sign-in" entry.

### Building the release packages

```powershell
powershell -ExecutionPolicy Bypass -File scripts\Build-Release.ps1
```

This builds Release with test hooks OFF, checks that the executable does not contain the test hook,
verifies (or downloads) the model against its published SHA-1, and writes the MSI and zip to `dist\`
with their SHA-256 hashes. It requires WiX Toolset v3.14
(`-WixDir` if it is installed elsewhere). The version comes from `project(Stilleto VERSION ...)` in
`CMakeLists.txt`; the MSI upgrades older versions in place.

## Tests

```powershell
ctest --preset vs2026-release               # unit + whisper integration tests
out\build\vs2026\tests\Release\StilletoPlatformTests.exe   # hotkey + Notepad insertion (drives the desktop)
# full app end to end (build it with: cmake --preset vs2026-e2e; cmake --build --preset vs2026-e2e)
powershell -ExecutionPolicy Bypass -File tests\e2e_notepad.ps1
```

See [docs/TESTING.md](docs/TESTING.md) for what each suite covers and the manual checklist.

## Architecture

```
src/
  core/           Pure C++ (no JUCE/Win32): state machine, hotkey spec, text cleanup, UTF-8/16,
                  SHA-1, speech gate. Unit tested.
  audio/          AudioRecorder (JUCE device, lock-free FIFO), AudioResampler (windowed sinc)
  transcription/  WhisperEngine (sync, single-thread), WhisperTranscriber (worker thread),
                  ModelCatalog, ModelManager (scan, verified download)
  windows/        GlobalHotkey, TextInserter, ForegroundTracker, NativeWindow (pure Win32),
                  TrayIcon (JUCE)
  history/        HistoryManager (atomic JSON writes on a background thread)
  settings/       SettingsManager (typed, persisted, observable)
  theme/          Design tokens, look-and-feel, vector icons and brand mark, animation helpers
  components/     Buttons, cards, sidebar, top bar, waveform, mic orb, floating widget, hotkey editor
  pages/          Overview, Dictation, History, Models, Settings, About
  AppController   Owns everything and runs the dictation state machine
```

**Threading.** The JUCE message thread owns the UI, state machine, hotkey window and tray. The audio
callback only downmixes, meters and writes into a preallocated FIFO, which a drain thread empties.
One whisper worker owns the model context: loads, inference and frees are serialised there, so the
context can never be freed mid-inference and no lock is held during inference. Insertion runs on a
pool thread. Every worker reports back with `MessageManager::callAsync`, guarded by a liveness token,
so callbacks never reach destroyed objects.

**State machine:** `Idle → Recording → Processing → Inserting → Completed`, with `Error` reachable
from anywhere. Transitions are validated (`core/DictationState.cpp`). A shortcut press while
`Processing` or `Inserting` never starts an overlapping recording.

## Known limitations

- Windows' UIPI prevents normal applications from injecting input into **elevated (administrator)**
  windows. Stilleto detects this and keeps the transcript for copying. Run Stilleto elevated if you
  need to dictate into elevated apps.
- **Secure fields** (passwords, UAC prompts), some games, and some remote-desktop or terminal windows
  ignore simulated input or paste.
- **Clipboard paste is the default** because measurements showed that Windows 11 Notepad garbles
  `KEYEVENTF_UNICODE` keystrokes sent faster than about 30 ms per character (see
  `tests/PlatformTests.cpp`, "tune"). The *Typing* method stays available in Settings for apps that
  block paste; increase the keystroke delay if characters are dropped.
- **Hotkey dictation never pulls focus back.** If you switch to another window before the transcript
  is ready, nothing is typed; insert it from the dashboard or history instead.
- **Record, then transcribe.** Live, word-by-word transcription is not implemented yet. The engine
  and state machine are structured so chunked inference can be added later.
- Whisper can mis-hear names and jargon; larger models (Small, Large v3 Turbo) are more accurate.
- The prebuilt configuration uses whisper.cpp's CPU backend (AVX2). GPU acceleration requires
  rebuilding with `STILLETO_GPU_VULKAN=ON`.
- Insertion is not guaranteed for every Windows application.

## Developer aids

- `Stilleto.exe --snapshot <dir>` renders every page off-screen to PNG and exits.
- `Stilleto.exe --export-icon <file.png>` regenerates `assets/StilletoIcon.png` from the vector mark.
- `STILLETO_TEST_AUDIO=<file.wav>`: for automated end-to-end tests only, and only in builds
  configured with `-DSTILLETO_TEST_HOOKS=ON`. After a real microphone
  capture, the recording is replaced by this WAV file. Never set it in normal use.
