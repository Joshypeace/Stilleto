#include "windows/NativeWindow.h"

#include <windows.h>
#include <dwmapi.h>
#include <intrin.h>

#ifndef PF_AVX2_INSTRUCTIONS_AVAILABLE
 #define PF_AVX2_INSTRUCTIONS_AVAILABLE 40
#endif

namespace stilleto::native
{
namespace
{
constexpr wchar_t runKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

// Defined in recent SDKs; repeated so older SDKs still build.
constexpr DWORD dwmUseImmersiveDarkMode = 20;
constexpr DWORD dwmWindowCornerPreference = 33;
constexpr DWORD dwmCornerRound = 2;
constexpr DWORD dwmCornerDoNotRound = 1;
} // namespace

void makeNonActivatingOverlay (void* handle)
{
    auto hwnd = static_cast<HWND> (handle);
    if (hwnd == nullptr) return;

    auto exStyle = GetWindowLongPtrW (hwnd, GWL_EXSTYLE);
    exStyle |= WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_TOPMOST;
    exStyle &= ~static_cast<LONG_PTR> (WS_EX_APPWINDOW);
    SetWindowLongPtrW (hwnd, GWL_EXSTYLE, exStyle);

    SetWindowPos (hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                  SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_NOOWNERZORDER);
}

void keepOnTopWithoutActivating (void* handle)
{
    if (auto hwnd = static_cast<HWND> (handle))
        SetWindowPos (hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
}

void applyModernFrame (void* handle, bool rounded)
{
    auto hwnd = static_cast<HWND> (handle);
    if (hwnd == nullptr) return;

    const BOOL dark = TRUE;
    DwmSetWindowAttribute (hwnd, dwmUseImmersiveDarkMode, &dark, sizeof (dark));

    const DWORD corner = rounded ? dwmCornerRound : dwmCornerDoNotRound;
    DwmSetWindowAttribute (hwnd, dwmWindowCornerPreference, &corner, sizeof (corner));
}

void bringToForeground (void* handle)
{
    auto hwnd = static_cast<HWND> (handle);
    if (hwnd == nullptr) return;

    if (IsIconic (hwnd))
        ShowWindow (hwnd, SW_RESTORE);

    SetForegroundWindow (hwnd);
}

bool systemAnimationsEnabled()
{
    BOOL enabled = TRUE;
    if (! SystemParametersInfoW (SPI_GETCLIENTAREAANIMATION, 0, &enabled, 0))
        return true;
    return enabled != FALSE;
}

bool isLaunchAtStartupEnabled (const std::wstring& valueName)
{
    HKEY key = nullptr;
    if (RegOpenKeyExW (HKEY_CURRENT_USER, runKey, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
        return false;

    const bool exists = RegQueryValueExW (key, valueName.c_str(), nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS;
    RegCloseKey (key);
    return exists;
}

std::wstring launchAtStartupCommand (const std::wstring& valueName)
{
    wchar_t buffer[2048] {};
    DWORD size = sizeof (buffer);
    if (RegGetValueW (HKEY_CURRENT_USER, runKey, valueName.c_str(), RRF_RT_REG_SZ, nullptr, buffer, &size) != ERROR_SUCCESS)
        return {};
    return buffer;
}

bool cpuSupportsSpeechEngine()
{
    // IsProcessorFeaturePresent(PF_AVX2...) also requires the OS to have enabled AVX state (XSAVE),
    // which a bare CPUID check misses (e.g. some VMs).
    if (! IsProcessorFeaturePresent (PF_AVX2_INSTRUCTIONS_AVAILABLE))
        return false;

    int regs[4] {};
    __cpuid (regs, 1);
    const bool fma  = (regs[2] & (1 << 12)) != 0;
    const bool f16c = (regs[2] & (1 << 29)) != 0;

    __cpuidex (regs, 7, 0);
    const bool bmi2 = (regs[1] & (1 << 8)) != 0;

    return fma && f16c && bmi2;
}

bool setLaunchAtStartup (const std::wstring& valueName, bool enabled, const std::wstring& commandLine)
{
    HKEY key = nullptr;
    if (RegCreateKeyExW (HKEY_CURRENT_USER, runKey, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS)
        return false;

    LSTATUS status;
    if (enabled)
    {
        status = RegSetValueExW (key, valueName.c_str(), 0, REG_SZ,
                                 reinterpret_cast<const BYTE*> (commandLine.c_str()),
                                 static_cast<DWORD> ((commandLine.size() + 1) * sizeof (wchar_t)));
    }
    else
    {
        status = RegDeleteValueW (key, valueName.c_str());
        if (status == ERROR_FILE_NOT_FOUND) status = ERROR_SUCCESS;
    }

    RegCloseKey (key);
    return status == ERROR_SUCCESS;
}

std::wstring currentExecutablePath()
{
    std::wstring path (MAX_PATH, L'\0');
    for (;;)
    {
        const DWORD len = GetModuleFileNameW (nullptr, path.data(), static_cast<DWORD> (path.size()));
        if (len == 0) return {};
        if (len < path.size()) { path.resize (len); return path; }
        path.resize (path.size() * 2);
    }
}

} // namespace stilleto::native
