// Unit tests for the pure logic in StilletoCore.

#include "TestFramework.h"

#include "audio/AudioResampler.h"
#include "core/DictationState.h"
#include "core/HotkeySpec.h"
#include "core/Sha1.h"
#include "core/SpeechDetection.h"
#include "core/TextProcessing.h"
#include "transcription/ModelCatalog.h"

#include <filesystem>
#include <fstream>
#include <numbers>

using namespace stilleto;

//==============================================================================================
// State machine
TEST_CASE ("state machine: normal dictation path is valid")
{
    using S = DictationState;
    CHECK (isValidTransition (S::Idle, S::Recording));
    CHECK (isValidTransition (S::Recording, S::Processing));
    CHECK (isValidTransition (S::Processing, S::Inserting));
    CHECK (isValidTransition (S::Inserting, S::Completed));
    CHECK (isValidTransition (S::Completed, S::Idle));
    CHECK (isValidTransition (S::Completed, S::Recording));
}

TEST_CASE ("state machine: overlapping work is rejected")
{
    using S = DictationState;
    CHECK (! isValidTransition (S::Processing, S::Recording));
    CHECK (! isValidTransition (S::Inserting, S::Recording));
    CHECK (! isValidTransition (S::Recording, S::Recording));
    CHECK (! isValidTransition (S::Idle, S::Processing));
    CHECK (isValidTransition (S::Idle, S::Inserting));       // re-insert a history entry
    CHECK (! isValidTransition (S::Inserting, S::Idle));   // insertion cannot be abandoned half way
    CHECK (! canStartRecording (S::Processing));
    CHECK (! canStartRecording (S::Inserting));
    CHECK (! canStartRecording (S::Recording));
    CHECK (canStartRecording (S::Error));
    CHECK (isBusy (S::Processing) && isBusy (S::Inserting) && ! isBusy (S::Recording));
}

//==============================================================================================
// Hotkeys
TEST_CASE ("hotkey: default is Ctrl+Alt+Space")
{
    const auto spec = HotkeySpec::defaultSpec();
    CHECK_EQ (spec.toDisplayString(), std::string ("Ctrl + Alt + Space"));
    CHECK_EQ (spec.toStorageString(), std::string ("Ctrl+Alt+Space"));
    CHECK (spec.isUsable());
}

TEST_CASE ("hotkey: round trip and tolerant parsing")
{
    for (const char* text : { "Ctrl+Alt+Space", "ctrl + shift + d", "Win+Alt+F9", "F8", "Alt+Num5", "Ctrl+Alt+`" })
    {
        const auto parsed = HotkeySpec::parse (text);
        REQUIRE (parsed.has_value());
        const auto again = HotkeySpec::parse (parsed->toStorageString());
        REQUIRE (again.has_value());
        CHECK (*again == *parsed);
    }

    const auto d = HotkeySpec::parse ("ctrl + shift + d");
    REQUIRE (d.has_value());
    CHECK_EQ (d->virtualKey, std::uint32_t ('D'));
    CHECK_EQ (d->modifiers, HotkeySpec::modControl | HotkeySpec::modShift);
}

TEST_CASE ("hotkey: unusable or malformed shortcuts are rejected")
{
    CHECK (! HotkeySpec::parse ("A").has_value());            // would hijack typing
    CHECK (! HotkeySpec::parse ("Shift+A").has_value());      // still ordinary typing
    CHECK (! HotkeySpec::parse ("Ctrl+Banana").has_value());
    CHECK (! HotkeySpec::parse ("Hyper+Space").has_value());
    CHECK (! HotkeySpec::parse ("").has_value());
    CHECK (HotkeySpec::parse ("Shift+F2").has_value());       // function keys may use Shift alone
}

//==============================================================================================
// Text processing
TEST_CASE ("text: whisper annotations are removed")
{
    CHECK_EQ (text::cleanTranscript (" [BLANK_AUDIO]", {}), std::string());
    CHECK_EQ (text::cleanTranscript (" Hello there. (upbeat music) How are you?", {}), std::string ("Hello there. How are you?"));
    CHECK_EQ (text::cleanTranscript ("Call me (maybe) tomorrow.", {}), std::string ("Call me (maybe) tomorrow."));
}

TEST_CASE ("text: segments join and punctuation is tidied")
{
    text::CleanupOptions joined;
    CHECK_EQ (text::cleanTranscript (" hello world .\n This is   Stilleto ,ok", joined),
              std::string ("Hello world. This is Stilleto, ok"));
    CHECK_EQ (text::tidyPunctuation ("first\nsecond"), std::string ("First\nSecond"));

    text::CleanupOptions paragraphs;
    paragraphs.paragraphPerSegment = true;
    CHECK_EQ (text::cleanTranscript (" First line.\n Second line.", paragraphs), std::string ("First line.\nSecond line."));

    CHECK_EQ (text::tidyPunctuation ("It costs 3.14 at 10:30, e.g. today"), std::string ("It costs 3.14 at 10:30, e.g. today"));
    CHECK_EQ (text::tidyPunctuation ("1,000 items"), std::string ("1,000 items"));
}

TEST_CASE ("disfluency: fillers are removed with their commas")
{
    CHECK_EQ (text::removeDisfluencies ("Um, so I think we should go."), std::string ("So I think we should go."));
    CHECK_EQ (text::removeDisfluencies ("I was, uh, thinking about it."), std::string ("I was thinking about it."));
    CHECK_EQ (text::removeDisfluencies ("We can, um, erm, maybe meet."), std::string ("We can maybe meet."));
    CHECK_EQ (text::removeDisfluencies ("That is fine, hmm."), std::string ("That is fine."));
    CHECK_EQ (text::removeDisfluencies ("Uh"), std::string());
    // Full pipeline restores capitalisation after a leading filler.
    CHECK_EQ (text::cleanTranscript (" Um, so I think we should go.", {}), std::string ("So I think we should go."));
}

TEST_CASE ("disfluency: stutters collapse")
{
    CHECK_EQ (text::removeDisfluencies ("I-I-I think so."), std::string ("I think so."));
    CHECK_EQ (text::removeDisfluencies ("w-what time is it?"), std::string ("what time is it?"));
    CHECK_EQ (text::removeDisfluencies ("b-b-but why?"), std::string ("but why?"));
    CHECK_EQ (text::removeDisfluencies ("th-th-the report"), std::string ("the report"));
    CHECK_EQ (text::removeDisfluencies ("I I I think the the plan works."), std::string ("I think the plan works."));
    CHECK_EQ (text::removeDisfluencies ("I, I, I think so."), std::string ("I think so."));
    CHECK_EQ (text::removeDisfluencies ("We we we need it."), std::string ("We need it."));
    CHECK_EQ (text::removeDisfluencies ("And and then we left."), std::string ("And then we left."));
}

TEST_CASE ("disfluency: punctuation, quotes and capitals survive filler removal")
{
    CHECK_EQ (text::removeDisfluencies ("Is that right? Uh."), std::string ("Is that right?"));
    CHECK_EQ (text::removeDisfluencies ("Really? Um, yes."), std::string ("Really? Yes."));
    CHECK_EQ (text::removeDisfluencies ("\"Um, hello,\" she said."), std::string ("\"Hello,\" she said."));
    CHECK_EQ (text::removeDisfluencies ("Wait - um - okay."), std::string ("Wait - okay."));
    CHECK_EQ (text::removeDisfluencies ("I said no. No, no."), std::string ("I said no. No, no."));
    CHECK_EQ (text::removeDisfluencies ("The end. The the plan."), std::string ("The end. The plan."));
}

TEST_CASE ("disfluency: legitimate text is preserved")
{
    for (const char* keep : { "I know that that is true.", "She had had enough.", "It is very very good.",
                              "Please re-read the e-mail.", "A well-known co-op.", "No, no, I disagree.",
                              "The end. The next chapter.", "Call me at 10-12.", "\xD0\x94\xD0\xB0, \xD0\xB4\xD0\xB0.",
                              // Review round 2: repeats across clauses and particles are real words.
                              "I told you, you need to leave.", "If you want it, it is yours.", "Whatever you do, do it well.",
                              "Log in in the settings.", "Turn it on on Monday.", "Will Will come?",
                              // Reduplications, capitalised compounds and numbers.
                              "It was so-so.", "That's a no-no.", "Bye-bye for now.", "The D-Day landings.", "Run a T-test.",
                              "An S-shaped curve.", "Rate it 1-10.", "The score was 1-1.", "Call 555-555-5555.",
                              // Emphatic triples and digit sequences.
                              "No, no, no!", "Location, location, location.", "Ha ha ha", "My PIN is 7 7 7 2.",
                              "really really really good", "uh-huh", "mm-hmm" })
        CHECK_EQ (text::removeDisfluencies (keep), std::string (keep));

    text::CleanupOptions off;
    off.removeDisfluencies = false;
    CHECK_EQ (text::cleanTranscript ("Um, I I think", off), std::string ("Um, I I think"));
}

TEST_CASE ("text: word counting")
{
    CHECK_EQ (text::countWords (""), std::size_t (0));
    CHECK_EQ (text::countWords ("  one  two\nthree\tfour "), std::size_t (4));
    CHECK_EQ (text::countWords ("naïve café résumé"), std::size_t (3));
    CHECK_EQ (text::countWords ("a\xC2\xA0" "b"), std::size_t (2));
}

TEST_CASE ("text: UTF-8 <-> UTF-16 conversion")
{
    const std::string ascii = "Hello, world!";
    CHECK (text::utf8ToUtf16 (ascii) == u"Hello, world!");

    const std::string mixed = "caf\xC3\xA9 \xE2\x82\xAC \xF0\x9F\x8E\x99";   // café € 🎙
    const auto wide = text::utf8ToUtf16 (mixed);
    REQUIRE (wide.size() == 9);
    CHECK (wide[3] == 0x00E9);
    CHECK (wide[5] == 0x20AC);
    CHECK (wide[7] == 0xD83C && wide[8] == 0xDF99);                          // surrogate pair
    CHECK_EQ (text::utf16ToUtf8 (wide), mixed);

    // Invalid input never produces garbage code units.
    const auto bad = text::utf8ToUtf16 ("a\xFF" "b\xC3");
    CHECK (bad == std::u16string (u"a�b�"));
    CHECK (text::utf8ToUtf16 ("\xED\xA0\x80") == std::u16string (u"�"));  // encoded surrogate
}

TEST_CASE ("text: line endings normalise")
{
    CHECK_EQ (text::normaliseLineEndings ("a\r\nb\rc\n"), std::string ("a\nb\nc\n"));
    CHECK_EQ (text::normaliseWhitespace ("  a  \n\n\n\n  b "), std::string ("a\n\nb"));
}

//==============================================================================================
// Resampler
namespace
{
std::vector<float> sine (double freq, double rate, double seconds, float amplitude = 0.5f)
{
    std::vector<float> s (static_cast<std::size_t> (rate * seconds));
    for (std::size_t i = 0; i < s.size(); ++i)
        s[i] = amplitude * static_cast<float> (std::sin (2.0 * std::numbers::pi * freq * static_cast<double> (i) / rate));
    return s;
}

double goertzelAmplitude (const std::vector<float>& x, double freq, double rate, std::size_t skip)
{
    const double w = 2.0 * std::numbers::pi * freq / rate;
    double s1 = 0, s2 = 0;
    const auto n = x.size() - 2 * skip;
    for (std::size_t i = skip; i < skip + n; ++i)
    {
        const double s0 = x[i] + 2.0 * std::cos (w) * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    const double power = s1 * s1 + s2 * s2 - 2.0 * std::cos (w) * s1 * s2;
    return 2.0 * std::sqrt (std::max (0.0, power)) / static_cast<double> (n);
}
} // namespace

TEST_CASE ("resampler: 48 kHz -> 16 kHz preserves duration and in-band tones")
{
    const auto input = sine (1000.0, 48000.0, 2.0);
    const auto out = AudioResampler::resample (input, 48000.0, 16000.0);
    CHECK_EQ (out.size(), std::size_t (32000));
    CHECK_NEAR (goertzelAmplitude (out, 1000.0, 16000.0, 200), 0.5, 0.01);
}

TEST_CASE ("resampler: content above the new Nyquist is filtered, not aliased")
{
    // 12 kHz at 48 kHz would alias to 4 kHz at 16 kHz without an anti-alias filter.
    const auto input = sine (12000.0, 48000.0, 1.0);
    const auto out = AudioResampler::resample (input, 48000.0, 16000.0);
    CHECK (goertzelAmplitude (out, 4000.0, 16000.0, 200) < 0.5 * 0.001);   // > 60 dB down
}

TEST_CASE ("resampler: non-integer ratios and upsampling")
{
    const auto in441 = sine (440.0, 44100.0, 1.5);
    const auto out = AudioResampler::resample (in441, 44100.0, 16000.0);
    CHECK_EQ (out.size(), std::size_t (24000));
    CHECK_NEAR (goertzelAmplitude (out, 440.0, 16000.0, 200), 0.5, 0.01);

    const auto in8k = sine (300.0, 8000.0, 1.0);
    const auto up = AudioResampler::resample (in8k, 8000.0, 16000.0);
    CHECK_EQ (up.size(), std::size_t (16000));
    CHECK_NEAR (goertzelAmplitude (up, 300.0, 16000.0, 200), 0.5, 0.01);

    CHECK (AudioResampler::resample ({}, 48000.0, 16000.0).empty());
    const std::vector<float> same { 0.1f, 0.2f };
    CHECK (AudioResampler::resample (same, 16000.0, 16000.0) == same);
}

//==============================================================================================
// Model files
TEST_CASE ("models: catalog and file inspection")
{
    const auto* base = ModelCatalog::findById ("base.en");
    REQUIRE (base != nullptr);
    CHECK_EQ (base->fileName(), std::string ("ggml-base.en.bin"));
    CHECK (base->downloadUrl().rfind ("https://huggingface.co/ggerganov/whisper.cpp/", 0) == 0);
    CHECK (ModelCatalog::findByFileName ("ggml-small.en.bin") != nullptr);

    const auto dir = std::filesystem::temp_directory_path() / "stilleto-tests";
    std::filesystem::create_directories (dir);

    CHECK (inspectModelFile (dir / "does-not-exist.bin") == ModelFileStatus::Missing);

    {
        std::ofstream (dir / "tiny.bin", std::ios::binary) << "ggml";
    }
    CHECK (inspectModelFile (dir / "tiny.bin") == ModelFileStatus::TooSmall);

    {
        std::ofstream f (dir / "wrong.bin", std::ios::binary);
        std::vector<char> junk (2 * 1024 * 1024, 'x');
        f.write (junk.data(), static_cast<std::streamsize> (junk.size()));
    }
    CHECK (inspectModelFile (dir / "wrong.bin") == ModelFileStatus::BadMagic);

    {
        std::ofstream f (dir / "good.bin", std::ios::binary);
        std::vector<char> data (2 * 1024 * 1024, 0);
        const char magic[] = { 0x6c, 0x6d, 0x67, 0x67 };   // 'ggml' little endian
        std::copy (std::begin (magic), std::end (magic), data.begin());
        f.write (data.data(), static_cast<std::streamsize> (data.size()));
    }
    CHECK (inspectModelFile (dir / "good.bin") == ModelFileStatus::Valid);

    std::filesystem::remove_all (dir);
    CHECK_EQ (formatBytes (142ull * 1024 * 1024), std::string ("142 MB"));
}

//==============================================================================================
// SHA-1 (model download verification)
TEST_CASE ("sha1: standard test vectors")
{
    CHECK_EQ (Sha1::ofString (""), std::string ("da39a3ee5e6b4b0d3255bfef95601890afd80709"));
    CHECK_EQ (Sha1::ofString ("abc"), std::string ("a9993e364706816aba3e25717850c26c9cd0d89d"));
    CHECK_EQ (Sha1::ofString ("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
              std::string ("84983e441c3bd26ebaae4aa1f95129e5e54670f1"));

    std::string million (1000000, 'a');
    CHECK_EQ (Sha1::ofString (million), std::string ("34aa973cd4c4daa4f61eeb2bdbad27316534016f"));
}

//==============================================================================================
// Speech gate
TEST_CASE ("speech: silence and noise are rejected, speech-like audio passes")
{
    std::vector<float> silence (16000 * 2, 0.0f);
    const auto s = analyseSpeech (silence, 16000.0);
    CHECK (s.isDigitalSilence());
    CHECK (! s.hasSpeech());

    // Low room noise (-60 dBFS) with no events.
    std::vector<float> noise (16000 * 2);
    unsigned seed = 1;
    for (auto& v : noise) { seed = seed * 1103515245u + 12345u; v = ((seed >> 16) / 32768.0f - 1.0f) * 0.001f; }
    CHECK (! analyseSpeech (noise, 16000.0).hasSpeech());

    // Same noise with 0.6 s of a loud modulated tone in the middle.
    auto speech = noise;
    for (std::size_t i = 12000; i < 12000 + 9600; ++i)
        speech[i] += 0.2f * static_cast<float> (std::sin (i * 0.05) * (0.6 + 0.4 * std::sin (i * 0.002)));
    const auto a = analyseSpeech (speech, 16000.0);
    CHECK (a.hasSpeech());
    CHECK (a.voicedSeconds > 0.4);
}

TEST_CASE ("speech: common whisper hallucinations")
{
    CHECK (isCommonHallucination ("You"));
    CHECK (isCommonHallucination (" Thank you. "));
    CHECK (isCommonHallucination ("Thanks for watching!"));
    CHECK (! isCommonHallucination (""));
    CHECK (! isCommonHallucination ("Да."));   // Russian "Da."
    CHECK (! isCommonHallucination ("はい"));   // Japanese "hai"
    CHECK (! isCommonHallucination ("Thank you for the update, see you tomorrow."));
    CHECK (! isCommonHallucination ("Send the report"));
}

int main (int argc, char** argv)
{
    return stilleto::test::runAll (argc > 1 ? argv[1] : nullptr);
}
