#include "Wavetable.h"

#include <juce_dsp/juce_dsp.h>
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

Wavetable Wavetable::fromSpectra (juce::String tableName,
                                  const std::vector<std::vector<std::complex<float>>>& spectra)
{
    constexpr int fftOrder = 11;  // 2^11 = tableSize
    static_assert ((1 << fftOrder) == tableSize);

    juce::dsp::FFT fft (fftOrder);
    std::vector<std::complex<float>> bins (tableSize);
    std::vector<std::complex<float>> time (tableSize);

    Wavetable wt;
    wt.name = std::move (tableName);
    wt.numFrames = (int) spectra.size();
    wt.levels.assign (numMipLevels, {});
    for (auto& level : wt.levels)
        level.assign ((size_t) wt.numFrames, std::vector<float> (tableSize + 1, 0.0f));

    for (size_t f = 0; f < spectra.size(); ++f)
    {
        const auto& spectrum = spectra[f];
        float frameGain = 1.0f;  // shared across mip levels so transitions don't step

        for (int level = 0; level < numMipLevels; ++level)
        {
            const auto maxHarmonic = juce::jmin ((size_t) (1024 >> level),
                                                 spectrum.size() - 1, (size_t) tableSize / 2 - 1);

            std::fill (bins.begin(), bins.end(), std::complex<float> {});
            for (size_t k = 1; k <= maxHarmonic; ++k)
            {
                bins[k] = spectrum[k];
                bins[(size_t) tableSize - k] = std::conj (spectrum[k]);
            }

            fft.perform (bins.data(), time.data(), true);

            if (level == 0)
            {
                float peak = 0.0f;
                for (const auto& s : time)
                    peak = juce::jmax (peak, std::abs (s.real()));
                frameGain = peak > 0.0f ? 1.0f / peak : 1.0f;
            }

            auto& table = wt.levels[(size_t) level][f];
            for (int i = 0; i < tableSize; ++i)
                table[(size_t) i] = time[(size_t) i].real() * frameGain;
            table[tableSize] = table[0];
        }
    }

    return wt;
}

Wavetable Wavetable::fromHarmonics (juce::String tableName,
                                    const std::vector<std::vector<float>>& framesOfHarmonics)
{
    std::vector<std::vector<std::complex<float>>> spectra;
    spectra.reserve (framesOfHarmonics.size());

    for (const auto& harmonics : framesOfHarmonics)
    {
        std::vector<std::complex<float>> spectrum (harmonics.size() + 1);
        for (size_t h = 0; h < harmonics.size(); ++h)
            spectrum[h + 1] = { 0.0f, -harmonics[h] };  // sine phase
        spectra.push_back (std::move (spectrum));
    }

    return fromSpectra (std::move (tableName), spectra);
}

Wavetable Wavetable::fromAudioFrames (juce::String tableName,
                                      const float* samples, int numSamples, int frameSize)
{
    jassert (frameSize > 0 && numSamples >= frameSize);

    constexpr int fftOrder = 11;
    juce::dsp::FFT fft (fftOrder);

    const auto numFrames = juce::jlimit (1, maxFrames, numSamples / frameSize);

    std::vector<std::vector<std::complex<float>>> spectra;
    spectra.reserve ((size_t) numFrames);

    std::vector<std::complex<float>> time (tableSize);
    std::vector<std::complex<float>> bins (tableSize);

    for (int f = 0; f < numFrames; ++f)
    {
        const auto* frame = samples + (size_t) f * (size_t) frameSize;

        // Resample the frame to tableSize if needed (linear; fine for the
        // typical 2048-sample convention where this is a straight copy).
        for (int i = 0; i < tableSize; ++i)
        {
            const auto srcPos = (double) i * frameSize / tableSize;
            const auto i0 = juce::jmin ((int) srcPos, frameSize - 1);
            const auto i1 = (i0 + 1) % frameSize;
            const auto frac = (float) (srcPos - (double) i0);
            time[(size_t) i] = { frame[i0] + frac * (frame[i1] - frame[i0]), 0.0f };
        }

        fft.perform (time.data(), bins.data(), false);
        spectra.emplace_back (bins.begin(), bins.begin() + tableSize / 2 + 1);
    }

    return fromSpectra (std::move (tableName), spectra);
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

    return fromHarmonics ("Basic Shapes", { sine, triangle, saw, square });
}

} // namespace arsenal::dsp
