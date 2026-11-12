#pragma once

#include <string>
#include <string_view>

namespace stilleto::text
{

/** Options applied to raw whisper output before it is shown or inserted. */
struct CleanupOptions
{
    bool tidyPunctuation = true;      // fix spacing around punctuation, capitalise sentence starts
    bool paragraphPerSegment = false; // keep segment breaks as newlines instead of joining with spaces
    bool removeDisfluencies = true;   // drop fillers (um, uh), stutters (I-I-I) and repeated words
};

/** Removes speech disfluencies so stammering or false starts are not typed:
    - filler sounds: "um", "uh", "erm", "hmm", ... (with their commas)
    - hyphenated stutters: "I-I-I" -> "I", "w-what" -> "what", "b-b-but" -> "but"
    - repeated words: "the the" -> "the", "I, I, I think" -> "I think"
    Deliberately conservative, because deleting a word the user meant is worse than leaving a
    stutter in: only a short list of stammer words is de-duplicated, and only without a sentence
    break between them; other repeats ("you, you need", "log in in", "No, no, no!", "7 7 7"),
    reduplications ("so-so"), compounds ("D-Day", "re-read") and numbers ("1-10") are kept.
    English rules: callers apply it only to English transcripts. */
[[nodiscard]] std::string removeDisfluencies (std::string_view text);

/** Removes whisper's non-speech annotations such as "[BLANK_AUDIO]", "(music)" or "[ Silence ]". */
[[nodiscard]] std::string stripNonSpeechAnnotations (std::string_view text);

/** Collapses runs of spaces/tabs and trims each line, preserving intentional newlines. */
[[nodiscard]] std::string normaliseWhitespace (std::string_view text);

/** Removes space before , . ! ? ; : and ensures a space after them; capitalises sentence starts. */
[[nodiscard]] std::string tidyPunctuation (std::string_view text);

/** Full pipeline used for transcripts. */
[[nodiscard]] std::string cleanTranscript (std::string_view rawText, const CleanupOptions& options);

/** Counts words as maximal runs of non-whitespace characters (UTF-8 aware for whitespace). */
[[nodiscard]] std::size_t countWords (std::string_view text);

/** Strict UTF-8 -> UTF-16 conversion; invalid sequences become U+FFFD. Handles supplementary
    planes (emoji etc.) as surrogate pairs. */
[[nodiscard]] std::u16string utf8ToUtf16 (std::string_view utf8);

/** UTF-16 -> UTF-8; unpaired surrogates become U+FFFD. */
[[nodiscard]] std::string utf16ToUtf8 (std::u16string_view utf16);

/** Normalises line endings to "\n". */
[[nodiscard]] std::string normaliseLineEndings (std::string_view text);

} // namespace stilleto::text
