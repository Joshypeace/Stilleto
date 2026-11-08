#include "audio/AudioResampler.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace stilleto
{
namespace
{
double besselI0 (double x)
{
    // Power series; converges quickly for the beta range used by speech resampling.
    double sum = 1.0, term = 1.0;
    const double halfX = x * 0.5;
    for (int k = 1; k < 64; ++k)
    {
        term *= (halfX / k) * (halfX / k);
        sum += term;
        if (term < sum * 1e-12) break;
    }
    return sum;
}
} // namespace

std::vector<float> AudioResampler::resample (std::span<const float> input,
                                             double inputRate,
                                             double outputRate,
                                             Quality quality)
{
    if (input.empty() || inputRate <= 0.0 || outputRate <= 0.0)
        return {};

    if (std::abs (inputRate - outputRate) < 1e-9)
        return { input.begin(), input.end() };

    const double step = inputRate / outputRate;                           // input samples per output sample
    const double cutoff = 0.5 * std::min (1.0, 1.0 / step) * quality.cutoffRatio; // cycles per input sample
    const int halfTaps = std::max (4, quality.halfTaps);
    const double halfWidth = halfTaps / (2.0 * cutoff);                    // in input samples

    // Kernel lookup table indexed by distance measured in zero crossings (u = 2*cutoff*x).
    constexpr int tableResolution = 512;
    const auto tableSize = static_cast<std::size_t> (halfTaps * tableResolution + 2);
    std::vector<float> table (tableSize);
    const double i0Beta = besselI0 (quality.kaiserBeta);

    for (std::size_t i = 0; i < tableSize; ++i)
    {
        const double u = static_cast<double> (i) / tableResolution;
        const double sinc = u == 0.0 ? 1.0 : std::sin (std::numbers::pi * u) / (std::numbers::pi * u);
        const double r = std::min (1.0, u / halfTaps);
        const double window = besselI0 (quality.kaiserBeta * std::sqrt (std::max (0.0, 1.0 - r * r))) / i0Beta;
        table[i] = static_cast<float> (sinc * window);
    }

    const auto outLength = static_cast<std::size_t> (std::llround (static_cast<double> (input.size()) / step));
    std::vector<float> output (outLength);
    const auto inLength = static_cast<std::ptrdiff_t> (input.size());
    const double tableScale = 2.0 * cutoff * tableResolution;

    for (std::size_t n = 0; n < outLength; ++n)
    {
        const double centre = static_cast<double> (n) * step;
        const auto first = std::max<std::ptrdiff_t> (0, static_cast<std::ptrdiff_t> (std::ceil (centre - halfWidth)));
        const auto last  = std::min<std::ptrdiff_t> (inLength - 1, static_cast<std::ptrdiff_t> (std::floor (centre + halfWidth)));

        double acc = 0.0, weightSum = 0.0;

        for (auto k = first; k <= last; ++k)
        {
            const double pos = std::abs (static_cast<double> (k) - centre) * tableScale;
            const auto idx = static_cast<std::size_t> (pos);
            if (idx + 1 >= tableSize) continue;

            const double frac = pos - static_cast<double> (idx);
            const double w = table[idx] + (table[idx + 1] - table[idx]) * frac;
            acc += w * input[static_cast<std::size_t> (k)];
            weightSum += w;
        }

        // Normalising by the summed weights gives unity DC gain, including at the buffer edges.
        output[n] = weightSum > 1e-9 ? static_cast<float> (acc / weightSum) : 0.0f;
    }

    return output;
}

} // namespace stilleto
