#pragma once

#include <string>

namespace stilleto::native
{

/** Turns a window (HWND passed as void* so JUCE code needn't include <windows.h>) into a floating
    overlay: topmost, absent from the taskbar/Alt+Tab, and never activated by showing or clicking. */
void makeNonActivatingOverlay (void* hwnd);

/** Re-asserts topmost z-order without activating (some apps push themselves above on focus). */
void keepOnTopWithoutActivating (void* hwnd);

/** Windows 11 rounded corners and dark-mode frame for a top-level window. No-op on older systems. */
void applyModernFrame (void* hwnd, bool rounded);

/** Brings a window to the foreground (used when the user explicitly opens Stilleto). */
void bringToForeground (void* hwnd);

/** Windows "Show animations" accessibility preference (Settings > Accessibility > Visual effects). */
[[nodiscard]] bool systemAnimationsEnabled();

/** Launch at sign-in via HKCU\...\Run. */
[[nodiscard]] bool isLaunchAtStartupEnabled (const std::wstring& valueName);
[[nodiscard]] std::wstring launchAtStartupCommand (const std::wstring& valueName);   // empty if absent
bool setLaunchAtStartup (const std::wstring& valueName, bool enabled, const std::wstring& commandLine);

[[nodiscard]] std::wstring currentExecutablePath();

/** True if the CPU and OS support everything the speech engine (ggml-cpu) is compiled for:
    AVX2 with OS-enabled YMM state, FMA, F16C and BMI2. */
[[nodiscard]] bool cpuSupportsSpeechEngine();

} // namespace stilleto::native
