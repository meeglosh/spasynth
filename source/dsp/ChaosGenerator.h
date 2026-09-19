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

            // Precomputed once here (never per-sample): a power-of-two
            // quantisation of speedMul, used only in synced mode. speedMul's
            // spread (0.75x..1.33x, log2 range ~=-0.415..+0.415) is expanded
            // by speedExponentScale before rounding to the nearest integer
            // exponent, spreading walkers across roughly +/-2 octaves
            // (0.25x..4x) instead of collapsing them all to 1x -- this is
            // what makes e.g. pitch renew once a bar while amp renews once a
            // beat, a real polyrhythm rather than everyone on the same grid.
            constexpr float speedExponentScale = 4.8f;
            const auto exponent = std::round (std::log2 (w.speedMul) * speedExponentScale);
            w.syncSpeedMul = std::exp2 (juce::jlimit (-3.0f, 3.0f, exponent));

            // matrixSource is the ONE walker the user routes explicitly
            // through the mod matrix and names a rate/division for -- it
            // must run at exactly that rate/division, not some random
            // power-of-two multiple of it. The random spread above is a
            // deliberate feature for the other (internal) walkers, which
            // is what makes pitch/amp/etc drift at different, decorrelated
            // rates (the polyrhythm described above); matrixSource has no
            // such "internal decorrelation" purpose -- it IS the rate the
            // user picked, so exempt it from the spread entirely. Leaving
            // w.phase randomized above still lets it decorrelate WHERE in
            // its cycle each voice starts, without touching HOW FAST it
            // runs.
            if (i == (size_t) matrixSource)
            {
                w.speedMul = 1.0f;
                w.syncSpeedMul = 1.0f;
            }
        }
    }

    // Advances all walkers by dtSeconds (one modulation chunk). Unsynced path
    // -- bit-for-bit as shipped before the sync feature.
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

    // Synced path (ORGANIZED CHAOS). divisionBeats is the base rate's period
    // in quarter-note beats (from lfoDivisionBeats(), the same tempo-sync
    // grid the LFOs and delay use). Each walker's own period is
    // divisionBeats / its quantised syncSpeedMul, so speeds stay whole
    // divisions of one another.
    //
    // When transportValid is true, target changes are driven by the
    // absolute host beat position (hostBeatsNow): a walker's target renews
    // exactly when floor(hostBeatsNow / walkerPeriodBeats) ticks over, so
    // renewals land on the beat/bar grid itself (not on wherever the walker's
    // own phase happened to be), and every voice computing the same
    // hostBeatsNow agrees on the instant, closing the "smear" the product
    // owner rejected.
    //
    // When transportValid is false (standalone internal clock giving no
    // advancing ppq, or a stopped host -- the same case the arp's
    // gotHostPpq/hostPlaying guard exists for) chaos free-runs at the synced
    // rate via the same phase-accumulator shape as the unsynced path, rather
    // than freezing.
    void processSynced (float divisionBeats, double bpm, bool transportValid,
                         double hostBeatsNow, double dtSeconds, juce::Random& random) noexcept
    {
        for (auto& w : walkers)
        {
            const auto periodBeats = juce::jmax (1.0e-6, (double) divisionBeats / (double) w.syncSpeedMul);
            const auto rate = (float) ((bpm / 60.0) / periodBeats);   // Hz, glide speed only

            if (transportValid)
            {
                const auto cycleIndex = (int64_t) std::floor (hostBeatsNow / periodBeats);
                if (cycleIndex != w.lastSyncCycle)
                {
                    w.lastSyncCycle = cycleIndex;
                    w.target = random.nextFloat() * 2.0f - 1.0f;
                }
            }
            else
            {
                w.phase += rate * dtSeconds;
                if (w.phase >= 1.0)
                {
                    w.phase -= std::floor (w.phase);
                    w.target = random.nextFloat() * 2.0f - 1.0f;
                }
            }

            const auto alpha = 1.0f - std::exp ((float) (-juce::MathConstants<double>::twoPi
                                                         * rate * dtSeconds));
            w.value += alpha * (w.target - w.value);
        }
    }

    float value (int index) const noexcept { return walkers[(size_t) index].value; }

    // Test-only introspection (plain field reads, no cost in the real signal
    // path -- these aren't called from computeChunk). Lets chaosSyncTest
    // verify renewal cadence/grid alignment without guessing at timing from
    // the smoothed output.
    int64_t syncCycleIndexForTest (int index) const noexcept { return walkers[(size_t) index].lastSyncCycle; }
    float syncSpeedMulForTest (int index) const noexcept { return walkers[(size_t) index].syncSpeedMul; }
    float speedMulForTest (int index) const noexcept { return walkers[(size_t) index].speedMul; }

    float slotPitch (int slot) const noexcept    { return value (pitchBase + slot); }
    float slotPhase (int slot) const noexcept    { return value (phaseBase + slot); }
    float slotPosition (int slot) const noexcept { return value (positionBase + slot); }

private:
    struct Walker
    {
        double phase = 1.0;      // target-renewal phase (cycles), unsynced/free-run path
        float target = 0.0f;
        float value = 0.0f;
        float speedMul = 1.0f;
        float syncSpeedMul = 1.0f;        // precomputed power-of-two ratio, synced path only
        int64_t lastSyncCycle = -1;       // grid cycle index of the last target renewal, synced path
    };

    std::array<Walker, numWalkers> walkers {};
};

} // namespace spa::dsp
