#include "Wavetable.h"

#include <cmath>

namespace arsenal::dsp
{

int Wavetable::mipLevelForFrequency (float frequencyHz, double sampleRate) const noexcept
{
    const auto allowedHarmonics = (float) (sampleRate * 0.5) / juce::jmax (1.0f, frequencyHz);

    for (int level = 0; level < numMipLevels - 1; ++level)
        if ((float) (1024 >> level) <= allowedHarmonics)
            return level;

    return numMipLevels - 1;
}

Wavetable Wavetable::fromHarmonics (const std::vector<std::vector<float>>& framesOfHarmonics)
{
    Wavetable wt;
    wt.numFrames = (int) framesOfHarmonics.size();
    wt.levels.resize (numMipLevels);

    for (int level = 0; level < numMipLevels; ++level)
    {
        const auto maxHarmonics = (size_t) (1024 >> level);
        auto& frames = wt.levels[(size_t) level];
        frames.resize ((size_t) wt.numFrames);

        for (size_t f = 0; f < (size_t) wt.numFrames; ++f)
        {
            const auto& harmonics = framesOfHarmonics[f];
            auto& table = frames[f];
            table.assign (tableSize + 1, 0.0f);

            const auto count = std::min (harmonics.size(), maxHarmonics);

            for (size_t h = 0; h < count; ++h)
            {
                if (harmonics[h] == 0.0f)
                    continue;

                const auto w = juce::MathConstants<double>::twoPi * (double) (h + 1) / (double) tableSize;

                for (int i = 0; i < tableSize; ++i)
                    table[(size_t) i] += harmonics[h] * (float) std::sin (w * i);
            }

            // Normalize to unit peak so morph position doesn't change loudness.
            float peak = 0.0f;
            for (int i = 0; i < tableSize; ++i)
                peak = juce::jmax (peak, std::abs (table[(size_t) i]));

            if (peak > 0.0f)
                for (int i = 0; i < tableSize; ++i)
                    table[(size_t) i] /= peak;

            table[tableSize] = table[0];
        }
    }

    return wt;
}

Wavetable Wavetable::createBasicShapes()
{
    constexpr size_t maxHarmonics = 1024;

    std::vector<float> sine (maxHarmonics, 0.0f);
    sine[0] = 1.0f;

    std::vector<float> triangle (maxHarmonics, 0.0f);
    for (size_t h = 0; h < maxHarmonics; h += 2)
    {
        const auto n = (float) (h + 1);
        triangle[h] = (((h / 2) % 2 == 0) ? 1.0f : -1.0f) / (n * n);
    }

    std::vector<float> saw (maxHarmonics, 0.0f);
    for (size_t h = 0; h < maxHarmonics; ++h)
        saw[h] = 1.0f / (float) (h + 1);

    std::vector<float> square (maxHarmonics, 0.0f);
    for (size_t h = 0; h < maxHarmonics; h += 2)
        square[h] = 1.0f / (float) (h + 1);

    return fromHarmonics ({ sine, triangle, saw, square });
}

} // namespace arsenal::dsp
