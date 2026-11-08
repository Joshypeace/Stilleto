# Stilleto testing

## Automated suites

| Suite | Command | Covers |
| --- | --- | --- |
| Core unit tests | `tests\Release\StilletoCoreTests.exe` (ctest `core`) | State-machine transitions and overlap rejection; hotkey parse/format/validation; transcript cleanup (annotations, whitespace, punctuation); UTF-8↔UTF-16 incl. surrogates and invalid input; resampler duration, in-band accuracy and >60 dB anti-alias rejection (48k/44.1k/8k); model file validation; SHA-1 vectors; speech gate and hallucination filter |
| Whisper integration | `tests\Release\StilletoWhisperTests.exe` (ctest `whisper`) | Missing and invalid models; real `base.en` transcription of the JFK sample; 48 kHz capture → resample → transcribe; cancellation and recovery; empty input; silence; model SHA-1 |
| Platform (desktop) | `tests\Release\StilletoPlatformTests.exe` | Hotkey register/conflict/pause/unregister; failed rebind keeps the old shortcut; hotkey fires while Notepad is focused without stealing focus or typing; insertion into Notepad: sentence, punctuation, typed newlines, Unicode (accents, em dash, CJK, emoji, Cyrillic), 2,000-character clipboard paste with clipboard restore; forced typing never touches the clipboard; closed targets refused |
| End to end | `powershell -ExecutionPolicy Bypass -File tests\e2e_notepad.ps1` | The real app: Notepad focused → shortcut → real microphone capture at the device rate → shortcut → worker-thread transcription → insertion into Notepad exactly once, focus kept throughout |

The platform and end-to-end suites open Notepad and inject keystrokes. Run them on an unlocked
desktop and **do not use the PC while they run**. Their key presses are guarded so they only go to
the test's own Notepad window. With no human speaker, the end-to-end script sets
`STILLETO_TEST_AUDIO` so that the JFK sample replaces the captured audio *after* the real microphone
capture.

`StilletoPlatformTests.exe tune` with `STILLETO_TUNE=1` measures keystroke pacing (diagnostic).

## Results recorded during development (2026-11-08, Windows 11 22621, VS 2026 / MSVC 14.51)

- Core: 17/17 passed. Whisper: 8/8 passed (JFK sample: 11 s of audio transcribed in about 4.5 s on CPU, Release).
- Platform: 7/7 passed.
- End to end: transcript inserted into Notepad in about 4.5 s after the second shortcut press, exactly
  once, focus never left Notepad; microphone captured 2.19 s at 48 kHz.
- Measured: Windows 11 Notepad garbles Unicode keystrokes faster than about 30 ms per character
  (`WaitForInputIdle` does not help); clipboard paste was exact in every run, hence the default.

## Manual checklist

Microphone and audio
- [ ] Settings → Audio lists your microphones; *Test microphone* moves the meter when you speak and shows the device rate.
- [ ] Dictate 5 to 10 s; the waveform follows your voice (silent when you are silent).
- [ ] Unplug the microphone, then dictate: you get a clear "Microphone unavailable" error.
- [ ] Windows Settings → Privacy → Microphone → off for desktop apps: dictation reports blocked or silent audio.
- [ ] Stop within 0.3 s of starting: "Recording too short". Record silence: "No speech detected".

Focus and insertion
- [ ] Notepad, Word, a browser `<textarea>`, a chat app and VS Code: text appears at the caret.
- [ ] The floating widget appears, updates and disappears without the target losing focus (the caret keeps blinking).
- [ ] Click the widget's stop button: dictation stops and focus stays in the target.
- [ ] Switch windows while it is transcribing: nothing is typed and the transcript waits in Stilleto.
- [ ] Copy something, dictate, then paste: your original clipboard content is back.
- [ ] Notepad running as administrator: Stilleto reports that it cannot type there.
- [ ] Chat apps: multi-paragraph transcripts do not send the message early (Shift+Enter newlines).

Shortcut and tray
- [ ] Change the shortcut in Settings; the old one stops working and the new one works everywhere.
- [ ] Pick a combination another app owns: conflict message, previous shortcut still works.
- [ ] Tray menu: open, start/stop, pause/resume shortcut, settings, quit.
- [ ] Close the window: Stilleto stays in the tray and the shortcut still works.
- [ ] Quit while recording: confirmation dialog appears; after quitting, no Stilleto process remains and the microphone indicator is off.

Models and history
- [ ] Download a model: real progress, then "Verifying checksum", then it is installed. Cancel midway: no partial file remains.
- [ ] Switch models; the top bar and Overview update.
- [ ] Search history, copy, insert again, delete one entry (gone after restart), clear all (with confirmation).
- [ ] Turn history off: new dictations are not saved.

Settings and motion
- [ ] Every setting persists across a restart.
- [ ] *Reduce motion* (or Windows "Animation effects" off) removes transitions.
- [ ] *Launch at sign-in* adds and removes `HKCU\...\Run\Stilleto`.
