#include "core/SpeechDetection.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <string>
#include <vector>

namespace stilleto
{

SpeechAnalysis analyseSpeech (std::span<const float> pcm, double sampleRate)
{
    SpeechAnalysis a;
    if (pcm.empty() || sampleRate <= 0)
        return a;

    const auto frame = static_cast<std::size_t> (std::max (1.0, sampleRate * 0.03));   // 30 ms
    std::vector<float> frameRms;
    frameRms.reserve (pcm.size() / frame + 1);

    double total = 0.0;
    for (std::size_t start = 0; start < pcm.size(); start += frame)
    {
        const auto end = std::min (pcm.size(), start + frame);
        double sum = 0.0;
        for (std::size_t i = start; i < end; ++i)
        {
            const float s = pcm[i];
            a.peak = std::max (a.peak, std::abs (s));
            sum += static_cast<double> (s) * s;
        }
        total += sum;
        frameRms.push_back (static_cast<float> (std::sqrt (sum / static_cast<double> (end - start))));
    }

    a.rms = static_cast<float> (std::sqrt (total / static_cast<double> (pcm.size())));

    std::vector<float> sorted = frameRms;
    std::sort (sorted.begin(), sorted.end());
    a.noiseFloor = sorted[sorted.size() / 10];

    // A frame is voiced when it is clearly above both the room's noise floor and an absolute
    // threshold (-46 dBFS), so a noisy but silent room does not count as speech.
    const float threshold = std::max (a.noiseFloor * 3.0f, 0.005f);
    const auto voicedFrames = std::count_if (frameRms.begin(), frameRms.end(), [&] (float r) { return r > threshold; });
    a.voicedSeconds = static_cast<double> (voicedFrames) * static_cast<double> (frame) / sampleRate;
    return a;
}

bool isCommonHallucination (std::string_view text)
{
    std::string normalised;
    for (const char c : text)
    {
        const auto u = static_cast<unsigned char> (c);
        if (std::isalnum (u) || c == ' ')
            normalised += static_cast<char> (std::tolower (u));
    }

    while (! normalised.empty() && normalised.front() == ' ') normalised.erase (normalised.begin());
    while (! normalised.empty() && normalised.back() == ' ') normalised.pop_back();

    static constexpr std::array phrases {
        "you", "thank you", "thanks", "thank you very much", "thanks for watching",
        "thank you for watching", "bye", "okay", "so", "the end", "subtitles by the amaraorg community"
    };

    return normalised.empty() || std::find (phrases.begin(), phrases.end(), normalised) != phrases.end();
}

} // namespace stilleto
