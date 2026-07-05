#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <complex>
#include <vector>

namespace spa::dsp
{

// A wavetable: N morph frames, each stored at several band-limited mip levels
// (one per octave) so playback stays alias-free across the keyboard.
//
// Layout: mipLevel -> frame -> samples (tableSize + 1, last sample duplicates
// the first so linear interpolation never needs a wrap branch).
class Wavetable
{
public:
    static constexpr int tableSize = 2048;
    static constexpr int numMipLevels = 10;  // level k allows up to (1024 >> k) harmonics
    static constexpr int maxFrames = 256;

    const juce::String& getName() const noexcept { return name; }
    int getNumFrames() const noexcept { return numFrames; }

    // Highest mip level whose harmonic content stays below Nyquist for the
    // given fundamental.
    int mipLevelForFrequency (float frequencyHz, double sampleRate) const noexcept;

    const float* getFrame (int mipLevel, int frame) const noexcept
    {
        return levels[(size_t) mipLevel][(size_t) frame].data();
    }

    // Factory table: sine -> triangle -> saw -> square morph.
    static Wavetable createBasicShapes();

    // Builds a table from per-frame harmonic amplitude lists (index 0 = fundamental).
    static Wavetable fromHarmonics (juce::String name,
                                    const std::vector<std::vector<float>>& framesOfHarmonics);

    // Builds a table from raw audio: consecutive frames of frameSize samples.
    // Frames are resampled to tableSize if needed; frame count is capped at
    // maxFrames. This is the entry point for user/Serum-style wavetable WAVs.
    static Wavetable fromAudioFrames (juce::String name,
                                      const float* samples, int numSamples, int frameSize);

private:
    // spectra: per frame, bins 0..tableSize/2 (hermitian symmetry implied).
    // Bin 0 (DC) is ignored; bin k = harmonic k.
    static Wavetable fromSpectra (juce::String name,
                                  const std::vector<std::vector<std::complex<float>>>& spectra);

    juce::String name;
    int numFrames = 0;
    // levels[mip][frame][sample]
    std::vector<std::vector<std::vector<float>>> levels;
};

} // namespace spa::dsp
