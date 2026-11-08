#include "core/HotkeySpec.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <vector>

namespace stilleto
{
namespace
{
struct NamedKey
{
    std::uint32_t vk;
    std::string_view name;
};

// Win32 virtual-key codes for the keys that make sense in a global shortcut.
constexpr std::array namedKeys {
    NamedKey { 0x20, "Space" },     NamedKey { 0x0D, "Enter" },    NamedKey { 0x09, "Tab" },
    NamedKey { 0x08, "Backspace" }, NamedKey { 0x2D, "Insert" },   NamedKey { 0x2E, "Delete" },
    NamedKey { 0x24, "Home" },      NamedKey { 0x23, "End" },      NamedKey { 0x21, "PageUp" },
    NamedKey { 0x22, "PageDown" },  NamedKey { 0x25, "Left" },     NamedKey { 0x26, "Up" },
    NamedKey { 0x27, "Right" },     NamedKey { 0x28, "Down" },     NamedKey { 0x13, "Pause" },
    NamedKey { 0x91, "ScrollLock" },NamedKey { 0xC0, "`" },        NamedKey { 0xBD, "-" },
    NamedKey { 0xBB, "=" },         NamedKey { 0xDB, "[" },        NamedKey { 0xDD, "]" },
    NamedKey { 0xDC, "\\" },        NamedKey { 0xBA, ";" },        NamedKey { 0xDE, "'" },
    NamedKey { 0xBC, "," },         NamedKey { 0xBE, "." },        NamedKey { 0xBF, "/" },
};

std::string toLower (std::string_view s)
{
    std::string out (s);
    std::transform (out.begin(), out.end(), out.begin(), [] (unsigned char c) { return static_cast<char> (std::tolower (c)); });
    return out;
}

std::string_view trim (std::string_view s)
{
    while (! s.empty() && std::isspace (static_cast<unsigned char> (s.front()))) s.remove_prefix (1);
    while (! s.empty() && std::isspace (static_cast<unsigned char> (s.back())))  s.remove_suffix (1);
    return s;
}

std::string join (const HotkeySpec& spec, std::string_view separator)
{
    std::string out;
    auto add = [&] (std::string_view part)
    {
        if (! out.empty()) out += separator;
        out += part;
    };

    if (spec.modifiers & HotkeySpec::modControl) add ("Ctrl");
    if (spec.modifiers & HotkeySpec::modAlt)     add ("Alt");
    if (spec.modifiers & HotkeySpec::modShift)   add ("Shift");
    if (spec.modifiers & HotkeySpec::modWin)     add ("Win");
    add (HotkeySpec::keyName (spec.virtualKey));
    return out;
}
} // namespace

bool HotkeySpec::isUsable() const noexcept
{
    const auto strongModifiers = modifiers & (modControl | modAlt | modWin);
    const bool isFunctionKey = virtualKey >= 0x70 && virtualKey <= 0x87;   // F1..F24 may stand alone
    return virtualKey != 0 && (strongModifiers != 0 || isFunctionKey);
}

std::string HotkeySpec::toDisplayString() const { return join (*this, " + "); }
std::string HotkeySpec::toStorageString() const { return join (*this, "+"); }

std::string HotkeySpec::keyName (std::uint32_t vk)
{
    if (vk >= 'A' && vk <= 'Z') return std::string (1, static_cast<char> (vk));
    if (vk >= '0' && vk <= '9') return std::string (1, static_cast<char> (vk));
    if (vk >= 0x70 && vk <= 0x87) return "F" + std::to_string (vk - 0x70 + 1);
    if (vk >= 0x60 && vk <= 0x69) return "Num" + std::to_string (vk - 0x60);

    for (const auto& k : namedKeys)
        if (k.vk == vk)
            return std::string (k.name);

    return "Key" + std::to_string (vk);
}

std::optional<std::uint32_t> HotkeySpec::keyFromName (std::string_view rawName)
{
    const auto name = trim (rawName);
    if (name.empty()) return std::nullopt;

    const auto lower = toLower (name);

    if (lower.size() == 1)
    {
        const auto c = static_cast<unsigned char> (std::toupper (static_cast<unsigned char> (lower[0])));
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
            return static_cast<std::uint32_t> (c);
    }

    if (lower.size() >= 2 && lower[0] == 'f' && std::all_of (lower.begin() + 1, lower.end(), ::isdigit))
    {
        const int n = std::stoi (lower.substr (1));
        if (n >= 1 && n <= 24) return static_cast<std::uint32_t> (0x70 + n - 1);
    }

    if (lower.rfind ("num", 0) == 0 && lower.size() == 4 && std::isdigit (static_cast<unsigned char> (lower[3])))
        return static_cast<std::uint32_t> (0x60 + (lower[3] - '0'));

    for (const auto& k : namedKeys)
        if (toLower (k.name) == lower)
            return k.vk;

    if (lower.rfind ("key", 0) == 0 && lower.size() > 3 && std::all_of (lower.begin() + 3, lower.end(), ::isdigit))
    {
        const auto v = std::stoul (lower.substr (3));
        if (v > 0 && v < 256) return static_cast<std::uint32_t> (v);
    }

    return std::nullopt;
}

std::optional<HotkeySpec> HotkeySpec::parse (std::string_view text)
{
    std::vector<std::string_view> parts;
    std::size_t start = 0;

    // '+' separates tokens, but a trailing "+" key (e.g. "Ctrl++") is not supported; '=' is used instead.
    for (std::size_t i = 0; i <= text.size(); ++i)
    {
        if (i == text.size() || text[i] == '+')
        {
            parts.push_back (trim (text.substr (start, i - start)));
            start = i + 1;
        }
    }

    if (parts.empty()) return std::nullopt;

    HotkeySpec spec { 0, 0 };

    for (std::size_t i = 0; i < parts.size(); ++i)
    {
        const auto lower = toLower (parts[i]);
        const bool isLast = i + 1 == parts.size();

        if (! isLast)
        {
            if (lower == "ctrl" || lower == "control")          spec.modifiers |= modControl;
            else if (lower == "alt")                            spec.modifiers |= modAlt;
            else if (lower == "shift")                          spec.modifiers |= modShift;
            else if (lower == "win" || lower == "windows")      spec.modifiers |= modWin;
            else return std::nullopt;
        }
        else
        {
            const auto key = keyFromName (parts[i]);
            if (! key) return std::nullopt;
            spec.virtualKey = *key;
        }
    }

    if (! spec.isUsable()) return std::nullopt;
    return spec;
}

} // namespace stilleto
