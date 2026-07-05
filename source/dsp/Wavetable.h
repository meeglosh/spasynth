#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

namespace arsenal::dsp
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

    int getNumFrames() const noexcept { return numFrames; }

    // Highest mip level whose harmonic content stays below Nyquist for the
    // given fundamental.
    int mipLevelForFrequency (float frequencyHz, double sampleRate) const noexcept;

    const float* getFrame (int mipLevel, int frame) const noexcept
    {
        return levels[(size_t) mipLevel][(size_t) frame].data();
    }

    // Builds the factory table: sine -> triangle -> saw -> square morph.
    // Replaced by user wavetable loading in checkpoint 2.
    static Wavetable createBasicShapes();

    // Builds a table from per-frame harmonic amplitude lists (index 0 = fundamental).
    static Wavetable fromHarmonics (const std::vector<std::vector<float>>& framesOfHarmonics);

private:
    int numFrames = 0;
    // levels[mip][frame][sample]
    std::vector<std::vector<std::vector<float>>> levels;
};

} // namespace arsenal::dsp
