#pragma once

#include <span>
#include <string_view>

namespace stilleto
{

/** Cheap checks that keep whisper from "transcribing" silence. Whisper reliably hallucinates short
    phrases ("You", "Thank you.") on silent input, so silent recordings are rejected before
    inference and suspicious one-liners from quiet audio are discarded afterwards. */
struct SpeechAnalysis
{
    float peak = 0.0f;
    float rms = 0.0f;
    float noiseFloor = 0.0f;   // 10th-percentile frame RMS
    double voicedSeconds = 0.0; // time in frames clearly above the noise floor

    [[nodiscard]] bool hasSpeech() const noexcept { return voicedSeconds >= 0.15 && peak > 0.01f; }
    [[nodiscard]] bool isDigitalSilence() const noexcept { return peak <= 1.0e-6f; }
};

[[nodiscard]] SpeechAnalysis analyseSpeech (std::span<const float> pcm, double sampleRate);

/** True for the stock phrases whisper produces from non-speech (case/punctuation-insensitive). */
[[nodiscard]] bool isCommonHallucination (std::string_view text);

} // namespace stilleto
