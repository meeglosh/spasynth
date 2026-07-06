#pragma once

#include "../params/ParameterRegistry.h"

#include <array>
#include <atomic>

namespace spa::dsp
{

// Lock-free audio -> UI channel. The most recently started active voice
// publishes its effective (post-modulation) values once per modulation chunk;
// the processor publishes block-level output stats. UI display components
// read these on their repaint timers — no locks, no allocation, no waiting
// on either side.
struct Telemetry
{
    // Voice arbitration: startNote takes a serial from noteSerial; a voice
    // publishes only if its serial is the newest seen so far.
    std::atomic<int> noteSerial { 0 };
    std::atomic<int> writerSerial { -1 };

    std::atomic<int> activeVoices { 0 };

    // Effective per-slot playback position, 0..1 (wavetable morph position,
    // sample playhead, or grain position, by mode).
    std::array<std::atomic<float>, params::maxOscSlots> slotPosition {};

    std::atomic<float> filterCutoffHz { 20000.0f };
    std::atomic<float> filterResonance { 0.0f };
    std::atomic<float> filter2CutoffHz { 20000.0f };
    std::atomic<float> filter2Resonance { 0.0f };

    std::array<std::atomic<float>, 3> envValue {};                 // amp, env2, env3
    std::array<std::atomic<float>, params::numLFOs> lfoValue {};   // post uni/bipolar
    std::array<std::atomic<float>, params::numLFOs> lfoPhase {};   // 0..1 base phase
    std::atomic<float> chaosValue { 0.0f };                        // scaled matrix source

    // Block peaks after the FX chain and master gain.
    std::atomic<float> peakL { 0.0f };
    std::atomic<float> peakR { 0.0f };
};

} // namespace spa::dsp
