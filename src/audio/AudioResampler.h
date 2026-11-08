#pragma once

#include <span>
#include <vector>

namespace stilleto
{

/** Band-limited sample-rate conversion for speech (offline, not real-time).

    A Kaiser-windowed sinc kernel whose cutoff tracks the lower of the two Nyquist frequencies, so
    downsampling 48 kHz -> 16 kHz removes content above 8 kHz instead of aliasing it into the band
    whisper listens to. Linear-phase; output length is round(n * outRate / inRate). */
class AudioResampler
{
public:
    struct Quality
    {
        int halfTaps = 24;        // zero crossings per side at the output's band limit
        double kaiserBeta = 8.0;  // ~80 dB stop band
        double cutoffRatio = 0.94; // fraction of the target Nyquist kept (transition band headroom)
    };

    [[nodiscard]] static std::vector<float> resample (std::span<const float> input,
                                                      double inputRate,
                                                      double outputRate,
                                                      Quality quality);

    [[nodiscard]] static std::vector<float> resample (std::span<const float> input,
                                                      double inputRate,
                                                      double outputRate)
    {
        return resample (input, inputRate, outputRate, Quality {});
    }

    /** whisper.cpp requires 16 kHz mono float PCM. */
    static constexpr double whisperSampleRate = 16000.0;
};

} // namespace stilleto
