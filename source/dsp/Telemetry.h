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
    // sample playhead, or grain-cloud centre, by mode).
    std::array<std::atomic<float>, params::maxOscSlots> slotPosition {};

    // Live granular grain cloud per slot, for the animated waveform playheads:
    // each active grain's normalized read position (0..1) and window amplitude
    // (0..1); count is how many entries are valid. Cosmetic only, so relaxed
    // atomics with count published last is fine.
    static constexpr int maxVizGrains = 12;
    struct GrainViz
    {
        std::atomic<int> count { 0 };
        std::array<std::atomic<float>, maxVizGrains> pos {};
        std::array<std::atomic<float>, maxVizGrains> amp {};
    };
    std::array<GrainViz, params::maxOscSlots> grainViz {};

    std::atomic<float> filterCutoffHz { 20000.0f };
    std::atomic<float> filterResonance { 0.0f };
    std::atomic<float> filter2CutoffHz { 20000.0f };
    std::atomic<float> filter2Resonance { 0.0f };

    std::array<std::atomic<float>, 3> envValue {};                 // amp, env2, env3
    std::array<std::atomic<float>, params::numLFOs> lfoValue {};   // post uni/bipolar
    std::array<std::atomic<float>, params::numLFOs> lfoPhase {};   // 0..1 base phase
    std::atomic<float> chaosValue { 0.0f };                        // scaled matrix source

    // Scrolling chaos trace for the ORGANIC CHAOS display: the raw chaos
    // matrix-source value (-1..1), written every chaosTraceDecimation-th mod
    // chunk by the same narrating voice as chaosValue above (see the
    // writerSerial arbitration around chaosValue's write site). At a 64-sample
    // mod chunk and 48 kHz that's ~750 chunks/s; with decimation 1 (every
    // chunk) that's ~750/s, so the 2048-sample ring spans roughly 2.7 seconds.
    // Decimation is kept configurable (a fast (8 Hz+) chaos rate's target
    // renewals otherwise slide across too few points, reading as a cliff
    // instead of a slope) -- raise it again if the ring size ever needs to
    // shrink. The UI reads the window ending at chaosTraceWrite and scrolls
    // it left, newest at the right.
    static constexpr int chaosTraceSize = 2048;       // power of two
    static constexpr int chaosTraceDecimation = 1;
    std::array<std::atomic<float>, chaosTraceSize> chaosTrace {};
    std::atomic<int> chaosTraceWrite { 0 };
    std::atomic<int> chaosTraceCounter { 0 };         // decimation counter, audio-thread only

    // Block peaks after the FX chain and master gain.
    std::atomic<float> peakL { 0.0f };
    std::atomic<float> peakR { 0.0f };

    // Post-master mono scope ring for the EQ spectrum analyzer. The audio thread
    // pushes the master output sample-by-sample; the UI reads the latest window
    // ending at scopeWrite and runs its own FFT. Cosmetic, so relaxed atomics.
    static constexpr int scopeSize = 2048;   // power of two for the FFT
    std::array<std::atomic<float>, scopeSize> scope {};
    std::atomic<int> scopeWrite { 0 };

    // Scrolling limiter history: per-block output peak (0..~1) and gain reduction
    // (dB, <= 0). The UI reads the window ending at limWrite and scrolls it.
    static constexpr int limiterHistory = 512;
    std::array<std::atomic<float>, limiterHistory> limOut {};
    std::array<std::atomic<float>, limiterHistory> limGrDb {};
    std::atomic<int> limWrite { 0 };

    // Live modulation-viz: per mod-destination effective (post-route,
    // clamped) normalized value 0..1, and whether that destination currently
    // has any active route with non-zero depth. Written once per mod chunk
    // by the narrating voice at the same writerSerial-gated site as
    // chaosValue above, straight from the `eff[]` array it already computed
    // applying routes in normalized space. When no voice is active/narrating,
    // these simply stop updating (last values held) -- UI mod-viz indicators
    // dim rather than blank using Telemetry::activeVoices == 0, per Knob's
    // pollModViz() in Controls.h.
    std::array<std::atomic<float>, params::maxModDests> modDestValue {};
    std::array<std::atomic<bool>, params::maxModDests> modDestActive {};

    // MIDI Learn diagnostics: counts every incoming message, broken down by
    // type, seen at the very top of processBlock, before any transformation
    // (keyboard-state merge, oversampling scale, arp rewrite). Lets the UI
    // show Mike not just THAT MIDI is arriving but WHAT KIND -- a controller
    // whose knobs never reach processBlock (e.g. consumed upstream by Logic's
    // Control Surfaces, or a device that needs "send CC" enabled) shows notes
    // incrementing while midiCcSeen stays at 0, which is the actual signal
    // Mike needs. One switch per message keeps this cheap. last*Number/
    // *Channel are cosmetic (last-write-wins is fine).
    std::atomic<uint32_t> midiNoteOnSeen { 0 };
    std::atomic<uint32_t> midiCcSeen { 0 };
    std::atomic<uint32_t> midiPitchWheelSeen { 0 };
    std::atomic<uint32_t> midiChannelPressureSeen { 0 };   // channel (mono) aftertouch
    std::atomic<uint32_t> midiAftertouchSeen { 0 };        // poly (per-note) aftertouch
    std::atomic<uint32_t> midiProgramChangeSeen { 0 };
    std::atomic<uint32_t> midiSysExSeen { 0 };
    std::atomic<uint32_t> midiOtherSeen { 0 };
    std::atomic<int> lastCcNumber { -1 };
    std::atomic<int> lastCcChannel { -1 };
    std::atomic<int> lastPitchBendChannel { -1 };
    std::atomic<int> lastAftertouchChannel { -1 };
};

} // namespace spa::dsp
