#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace stilleto
{

/** A global shortcut: Win32 MOD_* flags plus a virtual-key code. Kept free of <windows.h> so it can
    be unit tested and serialised; the values are identical to the Win32 constants. */
struct HotkeySpec
{
    static constexpr std::uint32_t modAlt     = 0x0001;   // MOD_ALT
    static constexpr std::uint32_t modControl = 0x0002;   // MOD_CONTROL
    static constexpr std::uint32_t modShift   = 0x0004;   // MOD_SHIFT
    static constexpr std::uint32_t modWin     = 0x0008;   // MOD_WIN

    std::uint32_t modifiers = modControl | modAlt;
    std::uint32_t virtualKey = 0x20;                       // VK_SPACE

    [[nodiscard]] static HotkeySpec defaultSpec() noexcept { return {}; }

    /** A shortcut needs at least one modifier other than Shift alone, otherwise it would hijack
        ordinary typing in every application. */
    [[nodiscard]] bool isUsable() const noexcept;

    /** "Ctrl + Alt + Space" style text for display. */
    [[nodiscard]] std::string toDisplayString() const;

    /** Compact persisted form, e.g. "Ctrl+Alt+Space". */
    [[nodiscard]] std::string toStorageString() const;

    /** Parses the persisted form (case-insensitive, tolerant of spaces). */
    [[nodiscard]] static std::optional<HotkeySpec> parse (std::string_view text);

    [[nodiscard]] static std::string keyName (std::uint32_t virtualKey);
    [[nodiscard]] static std::optional<std::uint32_t> keyFromName (std::string_view name);

    bool operator== (const HotkeySpec&) const = default;
};

} // namespace stilleto
