#include "core/TextProcessing.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <vector>

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

namespace
{
struct Token
{
    std::string leading;   // opening quotes/brackets
    std::string core;      // the word itself
    std::string trailing;  // punctuation after the word
};

bool isWordChar (unsigned char c) { return std::isalnum (c) || c == '\'' || c == '-' || c >= 0x80; }

Token splitToken (const std::string& raw)
{
    Token t;
    std::size_t start = 0, end = raw.size();
    while (start < end && ! isWordChar (static_cast<unsigned char> (raw[start]))) ++start;
    while (end > start && ! isWordChar (static_cast<unsigned char> (raw[end - 1]))) --end;
    // A trailing hyphen is a cut-off word ("wh-"), keep it in the core.
    t.leading = raw.substr (0, start);
    t.core = raw.substr (start, end - start);
    t.trailing = raw.substr (end);
    return t;
}

std::string lowerAscii (const std::string& s)
{
    std::string out = s;
    for (auto& c : out) c = static_cast<char> (std::tolower (static_cast<unsigned char> (c)));
    return out;
}

bool isFiller (const std::string& lowerCore)
{
    static const char* fillers[] = { "um", "umm", "ummm", "uh", "uhh", "uhm", "er", "err", "erm", "ah", "ahh",
                                     "eh", "hmm", "hmmm", "hm", "mm", "mmm", "mhm" };
    for (const char* f : fillers)
        if (lowerCore == f) return true;
    return false;
}

/** "I-I-I" -> "I", "w-what" -> "what", "b-b-but" -> "but". Leaves real compounds alone. */
std::string collapseHyphenStutter (const std::string& core)
{
    if (core.find ('-') == std::string::npos)
        return core;

    std::vector<std::string> parts;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= core.size(); ++i)
        if (i == core.size() || core[i] == '-')
        {
            parts.push_back (core.substr (start, i - start));
            start = i + 1;
        }

    const auto& last = parts.back();
    if (last.empty() || parts.size() < 2)
        return core;

    const auto lastLower = lowerAscii (last);
    std::size_t fragmentLength = 0;
    for (std::size_t i = 0; i + 1 < parts.size(); ++i)
    {
        const auto frag = lowerAscii (parts[i]);
        // Each fragment must be a short prefix of the final word ("w" of "what", "th" of "the").
        if (frag.empty() || frag.size() > 3 || lastLower.compare (0, frag.size(), frag) != 0)
            return core;
        fragmentLength = std::max (fragmentLength, frag.size());
    }

    // One fragment of 2-3 letters is too often a real prefix ("re-read", "co-cook"); require either
    // a single-letter fragment or repetition ("th-th-the").
    if (parts.size() == 2 && fragmentLength > 1 && lowerAscii (parts[0]) != lastLower)
        return core;

    return last;
}

/** Short words people stammer on. Doubles of other words ("that that", "had had", "very very",
    "no no") are often intentional and are only collapsed when repeated three or more times. */
bool isStutterProne (const std::string& lowerCore)
{
    static const char* words[] = { "i", "i'm", "a", "an", "the", "and", "to", "we", "it", "it's", "so", "but", "my",
                                   "you", "he", "she", "they", "in", "of", "on", "what", "this", "can", "will",
                                   "do", "if", "for", "with", "just", "like", "um", "uh" };
    for (const char* w : words)
        if (lowerCore == w) return true;
    return false;
}
} // namespace

std::string removeDisfluencies (std::string_view input)
{
    const auto text = normaliseLineEndings (input);
    std::string out;
    out.reserve (text.size());

    std::size_t lineStart = 0;
    while (lineStart <= text.size())
    {
        auto lineEnd = text.find ('\n', lineStart);
        if (lineEnd == std::string::npos) lineEnd = text.size();

        // Tokenise the line on whitespace.
        std::vector<Token> tokens;
        std::size_t i = lineStart;
        while (i < lineEnd)
        {
            while (i < lineEnd && isAsciiSpace (text[i])) ++i;
            const auto s = i;
            while (i < lineEnd && ! isAsciiSpace (text[i])) ++i;
            if (i > s) tokens.push_back (splitToken (text.substr (s, i - s)));
        }

        std::vector<Token> kept;
        for (auto& t : tokens)
        {
            t.core = collapseHyphenStutter (t.core);
            const auto lower = lowerAscii (t.core);

            if (! t.core.empty() && isFiller (lower))
            {
                // "I was, uh, thinking" -> "I was thinking": drop the comma the filler leaned on,
                // but keep sentence-ending punctuation.
                const bool fillerEndsSentence = t.trailing.find_first_of (".!?") != std::string::npos;
                if (fillerEndsSentence && ! kept.empty())
                    kept.back().trailing = t.trailing.substr (t.trailing.find_first_of (".!?"));
                else if (! kept.empty() && ! kept.back().trailing.empty() && kept.back().trailing.back() == ',')
                    kept.back().trailing.pop_back();
                continue;
            }

            // Repeated word: keep the last occurrence (it carries the following punctuation).
            if (! kept.empty() && ! t.core.empty() && kept.back().leading.empty() && t.leading.empty())
            {
                const auto prevLower = lowerAscii (kept.back().core);
                const bool prevSoftEnd = kept.back().trailing.empty() || kept.back().trailing == ",";

                if (prevLower == lower && prevSoftEnd)
                {
                    // Count the run to decide whether a non-prone double is intentional.
                    std::size_t run = 1;
                    for (auto it = kept.rbegin(); it != kept.rend() && lowerAscii (it->core) == lower; ++it) ++run;
                    const bool collapse = isStutterProne (lower) || run >= 3;

                    if (collapse)
                    {
                        // Collapse the whole run to one word, keeping the first occurrence's
                        // capitalisation ("I I think") and the last one's punctuation.
                        const auto firstCore = kept[kept.size() - (run - 1)].core;
                        kept.resize (kept.size() - (run - 1));
                        auto replacement = t;
                        replacement.core = firstCore;
                        kept.push_back (replacement);
                        continue;
                    }
                }
            }

            kept.push_back (t);
        }

        std::string line;
        for (const auto& t : kept)
        {
            const auto piece = t.leading + t.core + t.trailing;
            if (piece.empty()) continue;
            if (! line.empty()) line += ' ';
            line += piece;
        }

        out += line;
        if (lineEnd == text.size()) break;
        out += '\n';
        lineStart = lineEnd + 1;
    }

    // A sentence that started with a removed filler ("Um, so...") may begin with a stray comma.
    std::size_t first = out.find_first_not_of (" ,");
    return first == std::string::npos ? std::string() : out.substr (first);
}

std::string cleanTranscript (std::string_view rawText, const CleanupOptions& options)
{
    auto text = normaliseLineEndings (rawText);

    if (! options.paragraphPerSegment)
        std::replace (text.begin(), text.end(), '\n', ' ');

    text = normaliseWhitespace (stripNonSpeechAnnotations (text));

    if (options.removeDisfluencies)
        text = normaliseWhitespace (removeDisfluencies (text));

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
