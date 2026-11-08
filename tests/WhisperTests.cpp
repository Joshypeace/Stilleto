// Transcription integration tests: real whisper.cpp inference on the upstream JFK sample.

#include "TestFramework.h"

#include "audio/AudioResampler.h"
#include "core/Sha1.h"
#include "core/SpeechDetection.h"
#include "core/TextProcessing.h"
#include "transcription/ModelCatalog.h"
#include "transcription/WhisperEngine.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>

using namespace stilleto;

namespace
{
struct WavData
{
    std::vector<float> mono;
    double sampleRate = 0;
};

// Minimal RIFF reader for 16-bit PCM or 32-bit float files; channels are averaged to mono.
bool readWav (const std::filesystem::path& file, WavData& out)
{
    std::ifstream in (file, std::ios::binary);
    if (! in) return false;

    std::vector<char> bytes ((std::istreambuf_iterator<char> (in)), std::istreambuf_iterator<char>());
    if (bytes.size() < 44 || std::memcmp (bytes.data(), "RIFF", 4) != 0 || std::memcmp (bytes.data() + 8, "WAVE", 4) != 0)
        return false;

    std::uint16_t format = 0, channels = 0, bits = 0;
    std::uint32_t rate = 0;
    std::size_t pos = 12;

    while (pos + 8 <= bytes.size())
    {
        std::uint32_t size = 0;
        std::memcpy (&size, bytes.data() + pos + 4, 4);
        const char* id = bytes.data() + pos;
        const std::size_t body = pos + 8;

        if (std::memcmp (id, "fmt ", 4) == 0)
        {
            std::memcpy (&format, bytes.data() + body, 2);
            std::memcpy (&channels, bytes.data() + body + 2, 2);
            std::memcpy (&rate, bytes.data() + body + 4, 4);
            std::memcpy (&bits, bytes.data() + body + 14, 2);
        }
        else if (std::memcmp (id, "data", 4) == 0 && channels > 0)
        {
            const std::size_t frameBytes = channels * (bits / 8u);
            const std::size_t frames = std::min<std::size_t> (size, bytes.size() - body) / frameBytes;
            out.mono.resize (frames);

            for (std::size_t f = 0; f < frames; ++f)
            {
                float sum = 0;
                for (std::size_t c = 0; c < channels; ++c)
                {
                    const char* p = bytes.data() + body + f * frameBytes + c * (bits / 8u);
                    if (format == 1 && bits == 16) { std::int16_t v; std::memcpy (&v, p, 2); sum += v / 32768.0f; }
                    else if (format == 3 && bits == 32) { float v; std::memcpy (&v, p, 4); sum += v; }
                    else return false;
                }
                out.mono[f] = sum / channels;
            }

            out.sampleRate = rate;
            return true;
        }

        pos = body + size + (size & 1u);
    }

    return false;
}

std::string lower (std::string s)
{
    std::transform (s.begin(), s.end(), s.begin(), [] (unsigned char c) { return static_cast<char> (std::tolower (c)); });
    return s;
}

bool modelAvailable() { return std::filesystem::exists (STILLETO_TEST_MODEL); }

WhisperEngine& sharedEngine()
{
    static WhisperEngine engine;
    if (! engine.isLoaded())
    {
        const auto result = engine.load (STILLETO_TEST_MODEL, {});
        if (! result.ok) std::printf ("    load error: %s\n", result.error.c_str());
    }
    return engine;
}
} // namespace

TEST_CASE ("whisper: missing model is reported, not crashed on")
{
    WhisperEngine engine;
    const auto result = engine.load ("C:/definitely/not/here/ggml-base.en.bin", {});
    CHECK (! result.ok);
    CHECK (result.error.find ("not found") != std::string::npos);
    CHECK (! engine.isLoaded());

    const float pcm[16] {};
    const auto t = engine.transcribe (pcm, {});
    CHECK (t.status == WhisperEngine::Status::Failed);
}

TEST_CASE ("whisper: invalid model file is rejected")
{
    const auto file = std::filesystem::temp_directory_path() / "stilleto-invalid-model.bin";
    {
        std::ofstream f (file, std::ios::binary);
        std::vector<char> junk (3 * 1024 * 1024, 'z');
        f.write (junk.data(), static_cast<std::streamsize> (junk.size()));
    }

    WhisperEngine engine;
    const auto result = engine.load (file, {});
    CHECK (! result.ok);
    CHECK (! engine.isLoaded());
    std::filesystem::remove (file);
}

TEST_CASE ("whisper: transcribes the JFK sample")
{
    if (! modelAvailable()) { std::printf ("    SKIPPED: %s not present\n", STILLETO_TEST_MODEL); return; }

    WavData wav;
    REQUIRE (readWav (STILLETO_TEST_WAV, wav));
    REQUIRE (wav.sampleRate == 16000.0);

    auto& engine = sharedEngine();
    REQUIRE (engine.isLoaded());
    CHECK (! engine.modelInfo().multilingual);

    const auto result = engine.transcribe (wav.mono, {});
    REQUIRE (result.status == WhisperEngine::Status::Ok);

    const auto clean = text::cleanTranscript (result.text, {});
    std::printf ("    transcript (%.2fs): %s\n", result.inferenceSeconds, clean.c_str());
    CHECK (lower (clean).find ("ask not what your country can do for you") != std::string::npos);
    CHECK_EQ (result.language, std::string ("en"));
}

TEST_CASE ("whisper: audio captured at 48 kHz survives resampling to 16 kHz")
{
    if (! modelAvailable()) { std::printf ("    SKIPPED\n"); return; }

    WavData wav;
    REQUIRE (readWav (STILLETO_TEST_WAV, wav));

    // Simulate a 48 kHz microphone, then run the same conversion the recorder uses.
    const auto at48k = AudioResampler::resample (wav.mono, 16000.0, 48000.0);
    const auto back = AudioResampler::resample (at48k, 48000.0, AudioResampler::whisperSampleRate);
    CHECK_EQ (back.size(), wav.mono.size());

    const auto result = sharedEngine().transcribe (back, {});
    REQUIRE (result.status == WhisperEngine::Status::Ok);
    CHECK (lower (result.text).find ("your country") != std::string::npos);
}

TEST_CASE ("whisper: cancellation and empty input")
{
    if (! modelAvailable()) { std::printf ("    SKIPPED\n"); return; }

    WavData wav;
    REQUIRE (readWav (STILLETO_TEST_WAV, wav));

    std::atomic<bool> cancel { true };
    const auto cancelled = sharedEngine().transcribe (wav.mono, {}, &cancel);
    CHECK (cancelled.status == WhisperEngine::Status::Cancelled);

    const auto empty = sharedEngine().transcribe ({}, {});
    CHECK (empty.status == WhisperEngine::Status::Failed);

    // The engine must still work after a cancelled run.
    cancel = false;
    const auto again = sharedEngine().transcribe (wav.mono, {}, &cancel);
    CHECK (again.status == WhisperEngine::Status::Ok);
}

TEST_CASE ("whisper: silence produces no words")
{
    if (! modelAvailable()) { std::printf ("    SKIPPED\n"); return; }

    std::vector<float> silence (16000 * 2, 0.0f);
    const auto result = sharedEngine().transcribe (silence, {});
    REQUIRE (result.status == WhisperEngine::Status::Ok);
    const auto clean = text::cleanTranscript (result.text, {});
    std::printf ("    silence -> \"%s\"\n", clean.c_str());
    CHECK (text::countWords (clean) <= 2);   // whisper may hallucinate a token on pure silence
}

TEST_CASE ("models: downloaded base.en matches the upstream SHA-1")
{
    if (! modelAvailable()) { std::printf ("    SKIPPED\n"); return; }
    const auto digest = Sha1::ofFile (STILLETO_TEST_MODEL);
    REQUIRE (digest.has_value());
    CHECK_EQ (*digest, std::string (ModelCatalog::findById ("base.en")->sha1));
}

TEST_CASE ("speech: the JFK sample passes the speech gate")
{
    WavData wav;
    REQUIRE (readWav (STILLETO_TEST_WAV, wav));
    const auto a = analyseSpeech (wav.mono, wav.sampleRate);
    std::printf ("    peak=%.3f rms=%.4f floor=%.5f voiced=%.2fs\n", a.peak, a.rms, a.noiseFloor, a.voicedSeconds);
    CHECK (a.hasSpeech());
}

int main (int argc, char** argv)
{
    WhisperEngine::setLogHandler ([] (const std::string& line) { std::printf ("    [whisper] %s", line.c_str()); });
    return stilleto::test::runAll (argc > 1 ? argv[1] : nullptr);
}
