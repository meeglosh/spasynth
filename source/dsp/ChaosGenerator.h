#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <cmath>

namespace spa::dsp
{

// The Organic Chaos core: a bank of independent band-limited random walkers.
// Each walker picks a new random target at (roughly) the chaos rate and
// relaxes toward it through a one-pole smoother at the same rate — smoothed
// random steps that read as analog drift, never as white-noise glitch.
//
// Every walker gets its own slight rate multiplier so pitch drift, position
// drift, amp drift etc. never move in lockstep. That decorrelation is what
// makes the result feel organic.
class ChaosGenerator
{
public:
    // Walker layout: per-slot pitch/phase/position walkers, then voice-wide
    // amp, saturation, distortion, and the mod-matrix source walker.
    static constexpr int maxSlots = 4;

    enum Index
    {
        pitchBase = 0,              // + slot
        phaseBase = pitchBase + maxSlots,
        positionBase = phaseBase + maxSlots,
        amp = positionBase + maxSlots,
        saturation,
        distortion,
        matrixSource,
        numWalkers,
    };

    void prepare (juce::Random& random) noexcept
    {
        for (size_t i = 0; i < walkers.size(); ++i)
        {
            auto& w = walkers[i];
            w = {};
            // 0.75x..1.33x rate spread, deterministic-ish per walker slot but
            // seeded per voice so voices drift independently too.
            w.speedMul = 0.75f + 0.58f * random.nextFloat();
            w.phase = random.nextDouble();  // desynchronize target renewal
        }
    }

    // Advances all walkers by dtSeconds (one modulation chunk).
    void process (float rateHz, double dtSeconds, juce::Random& random) noexcept
    {
        for (auto& w : walkers)
        {
            const auto rate = rateHz * w.speedMul;

            w.phase += rate * dtSeconds;
            if (w.phase >= 1.0)
            {
                w.phase -= std::floor (w.phase);
                w.target = random.nextFloat() * 2.0f - 1.0f;
            }

            const auto alpha = 1.0f - std::exp ((float) (-juce::MathConstants<double>::twoPi
                                                         * rate * dtSeconds));
            w.value += alpha * (w.target - w.value);
        }
    }

    float value (int index) const noexcept { return walkers[(size_t) index].value; }

    float slotPitch (int slot) const noexcept    { return value (pitchBase + slot); }
    float slotPhase (int slot) const noexcept    { return value (phaseBase + slot); }
    float slotPosition (int slot) const noexcept { return value (positionBase + slot); }

private:
    struct Walker
    {
        double phase = 1.0;      // target-renewal phase (cycles)
        float target = 0.0f;
        float value = 0.0f;
        float speedMul = 1.0f;
    };

    std::array<Walker, numWalkers> walkers {};
};

} // namespace spa::dsp
