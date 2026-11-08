#include "core/TextProcessing.h"

#include <algorithm>
#include <array>
#include <cctype>

namespace stilleto::text
{
namespace
{
bool isAsciiSpace (char c) noexcept { return c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v'; }

std::string toLowerAscii (std::string_view s)
{
    std::string out (s);
    std::transform (out.begin(), out.end(), out.begin(), [] (unsigned char c) { return static_cast<char> (std::tolower (c)); });
    return out;
}

bool looksLikeSoundAnnotation (std::string_view inner)
{
    static constexpr std::array keywords {
        "music", "silence", "laugh", "applause", "noise", "blank", "inaudible", "sigh", "cough",
        "static", "beep", "typing", "wind", "breath", "throat", "sound", "click", "clap", "no speech"
    };

    const auto lower = toLowerAscii (inner);
    return std::any_of (keywords.begin(), keywords.end(), [&] (const char* k) { return lower.find (k) != std::string::npos; });
}
} // namespace

std::string stripNonSpeechAnnotations (std::string_view text)
{
    std::string out;
    out.reserve (text.size());

    for (std::size_t i = 0; i < text.size(); ++i)
    {
        const char c = text[i];
        const char close = c == '[' ? ']' : c == '(' ? ')' : c == '*' ? '*' : '\0';

        if (close != '\0')
        {
            const auto end = text.find (close, i + 1);

            if (end != std::string_view::npos && end - i < 64)
            {
                const auto inner = text.substr (i + 1, end - i - 1);

                // Square brackets are never dictated by whisper; () and ** only when they name a sound.
                if (c == '[' || looksLikeSoundAnnotation (inner))
                {
                    i = end;
                    out += ' ';
                    continue;
                }
            }
        }

        out += c;
    }

    return out;
}

std::string normaliseLineEndings (std::string_view text)
{
    std::string out;
    out.reserve (text.size());

    for (std::size_t i = 0; i < text.size(); ++i)
    {
        if (text[i] == '\r')
        {
            out += '\n';
            if (i + 1 < text.size() && text[i + 1] == '\n') ++i;
        }
        else
        {
            out += text[i];
        }
    }

    return out;
}

std::string normaliseWhitespace (std::string_view input)
{
    const auto text = normaliseLineEndings (input);
    std::string out;
    out.reserve (text.size());

    std::size_t lineStart = 0;
    int pendingNewlines = 0;

    while (lineStart <= text.size())
    {
        auto lineEnd = text.find ('\n', lineStart);
        if (lineEnd == std::string::npos) lineEnd = text.size();

        std::string line;
        bool lastWasSpace = true;   // trims leading space

        for (std::size_t i = lineStart; i < lineEnd; ++i)
        {
            const char c = text[i];
            if (isAsciiSpace (c))
            {
                if (! lastWasSpace) line += ' ';
                lastWasSpace = true;
            }
            else
            {
                line += c;
                lastWasSpace = false;
            }
        }

        while (! line.empty() && line.back() == ' ') line.pop_back();

        if (line.empty())
        {
            if (! out.empty()) ++pendingNewlines;
        }
        else
        {
            if (! out.empty())
                out.append (static_cast<std::size_t> (std::clamp (pendingNewlines + 1, 1, 2)), '\n');

            out += line;
            pendingNewlines = 0;
        }

        if (lineEnd == text.size()) break;
        lineStart = lineEnd + 1;
    }

    return out;
}

std::string tidyPunctuation (std::string_view text)
{
    std::string out;
    out.reserve (text.size() + 8);

    auto isClosingPunct = [] (char c) { return c == ',' || c == '.' || c == '!' || c == '?' || c == ';' || c == ':'; };

    for (std::size_t i = 0; i < text.size(); ++i)
    {
        const char c = text[i];

        // Drop spaces directly before closing punctuation ("word ," -> "word,").
        if (c == ' ' && i + 1 < text.size() && isClosingPunct (text[i + 1]))
            continue;

        out += c;

        // Ensure a space after , ; ! ? when a letter follows directly ("a,b" -> "a, b").
        // ':' and '.' are skipped so times (10:30), decimals (3.14) and abbreviations survive.
        if ((c == ',' || c == ';' || c == '!' || c == '?') && i + 1 < text.size())
        {
            const auto next = static_cast<unsigned char> (text[i + 1]);
            const bool betweenDigits = c == ',' && i > 0 && std::isdigit (static_cast<unsigned char> (text[i - 1])) && std::isdigit (next);
            if (std::isalpha (next) && ! betweenDigits)
                out += ' ';
        }
    }

    // Capitalise the first letter of the text and of each new line. Sentence starts are left to
    // whisper, which already capitalises them; guessing would break "e.g. this" or "3 p.m. today".
    bool capitaliseNext = true;
    for (std::size_t i = 0; i < out.size(); ++i)
    {
        const auto c = static_cast<unsigned char> (out[i]);

        if (c == '\n')
            capitaliseNext = true;
        else if (capitaliseNext && std::isalpha (c))
        {
            out[i] = static_cast<char> (std::toupper (c));
            capitaliseNext = false;
        }
        else if (! std::isspace (c) && c != '"' && c != '\'')
            capitaliseNext = false;
    }

    return out;
}

std::string cleanTranscript (std::string_view rawText, const CleanupOptions& options)
{
    auto text = normaliseLineEndings (rawText);

    if (! options.paragraphPerSegment)
        std::replace (text.begin(), text.end(), '\n', ' ');

    text = normaliseWhitespace (stripNonSpeechAnnotations (text));

    if (options.tidyPunctuation)
        text = normaliseWhitespace (tidyPunctuation (text));

    return text;
}

std::size_t countWords (std::string_view text)
{
    std::size_t count = 0;
    bool inWord = false;

    for (std::size_t i = 0; i < text.size(); ++i)
    {
        const auto c = static_cast<unsigned char> (text[i]);
        bool space = std::isspace (c) != 0;

        // U+00A0 NO-BREAK SPACE (C2 A0) and U+3000 IDEOGRAPHIC SPACE (E3 80 80) separate words too.
        if (c == 0xC2 && i + 1 < text.size() && static_cast<unsigned char> (text[i + 1]) == 0xA0)
        {
            space = true;
            ++i;
        }
        else if (c == 0xE3 && i + 2 < text.size() && static_cast<unsigned char> (text[i + 1]) == 0x80
                 && static_cast<unsigned char> (text[i + 2]) == 0x80)
        {
            space = true;
            i += 2;
        }

        if (space)
        {
            inWord = false;
        }
        else if (! inWord)
        {
            inWord = true;
            ++count;
        }
    }

    return count;
}

std::u16string utf8ToUtf16 (std::string_view s)
{
    std::u16string out;
    out.reserve (s.size());

    constexpr char32_t replacement = 0xFFFD;
    std::size_t i = 0;

    while (i < s.size())
    {
        const auto b0 = static_cast<unsigned char> (s[i]);
        char32_t cp = replacement;
        std::size_t len = 1;

        auto cont = [&] (std::size_t k) -> int
        {
            if (i + k >= s.size()) return -1;
            const auto b = static_cast<unsigned char> (s[i + k]);
            return (b & 0xC0) == 0x80 ? (b & 0x3F) : -1;
        };

        if (b0 < 0x80)
        {
            cp = b0;
        }
        else if ((b0 & 0xE0) == 0xC0 && b0 >= 0xC2)
        {
            if (const int c1 = cont (1); c1 >= 0) { cp = (char32_t (b0 & 0x1F) << 6) | char32_t (c1); len = 2; }
        }
        else if ((b0 & 0xF0) == 0xE0)
        {
            const int c1 = cont (1), c2 = c1 >= 0 ? cont (2) : -1;
            if (c1 >= 0 && c2 >= 0)
            {
                const char32_t v = (char32_t (b0 & 0x0F) << 12) | (char32_t (c1) << 6) | char32_t (c2);
                if (v >= 0x800 && (v < 0xD800 || v > 0xDFFF)) { cp = v; len = 3; }
                else len = 3;
            }
            else if (c1 >= 0) len = 2;
        }
        else if ((b0 & 0xF8) == 0xF0 && b0 <= 0xF4)
        {
            const int c1 = cont (1), c2 = c1 >= 0 ? cont (2) : -1, c3 = c2 >= 0 ? cont (3) : -1;
            if (c1 >= 0 && c2 >= 0 && c3 >= 0)
            {
                const char32_t v = (char32_t (b0 & 0x07) << 18) | (char32_t (c1) << 12) | (char32_t (c2) << 6) | char32_t (c3);
                if (v >= 0x10000 && v <= 0x10FFFF) cp = v;
                len = 4;
            }
            else
            {
                len = c2 >= 0 ? 3 : c1 >= 0 ? 2 : 1;
            }
        }

        if (cp >= 0x10000)
        {
            const char32_t v = cp - 0x10000;
            out += static_cast<char16_t> (0xD800 + (v >> 10));
            out += static_cast<char16_t> (0xDC00 + (v & 0x3FF));
        }
        else
        {
            out += static_cast<char16_t> (cp);
        }

        i += len;
    }

    return out;
}

std::string utf16ToUtf8 (std::u16string_view s)
{
    std::string out;
    out.reserve (s.size());

    for (std::size_t i = 0; i < s.size(); ++i)
    {
        char32_t cp = s[i];

        if (cp >= 0xD800 && cp <= 0xDBFF)
        {
            if (i + 1 < s.size() && s[i + 1] >= 0xDC00 && s[i + 1] <= 0xDFFF)
            {
                cp = 0x10000 + ((cp - 0xD800) << 10) + (char32_t (s[i + 1]) - 0xDC00);
                ++i;
            }
            else cp = 0xFFFD;
        }
        else if (cp >= 0xDC00 && cp <= 0xDFFF)
        {
            cp = 0xFFFD;
        }

        if (cp < 0x80)
        {
            out += static_cast<char> (cp);
        }
        else if (cp < 0x800)
        {
            out += static_cast<char> (0xC0 | (cp >> 6));
            out += static_cast<char> (0x80 | (cp & 0x3F));
        }
        else if (cp < 0x10000)
        {
            out += static_cast<char> (0xE0 | (cp >> 12));
            out += static_cast<char> (0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char> (0x80 | (cp & 0x3F));
        }
        else
        {
            out += static_cast<char> (0xF0 | (cp >> 18));
            out += static_cast<char> (0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char> (0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char> (0x80 | (cp & 0x3F));
        }
    }

    return out;
}

} // namespace stilleto::text
