#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "../params/ParameterRegistry.h"

namespace spa::dsp
{

// MIDI-transforming arpeggiator: sits between the host's MIDI and the synth.
// Tempo-synced (phase-locked to host ppq while playing, internal beat clock
// otherwise), with classic patterns, random/walk, chord stabs, and melodic
// phrase sequences. Real-time safe: fixed-capacity note store, no allocation.
class Arpeggiator
{
public:
    struct Params
    {
        bool enable = false;
        params::ArpMode mode = params::ArpMode::up;
        int division = 12;        // index into lfoDivisionBeats()
        int octaves = 1;
        float gate = 0.8f;        // fraction of a step
        float swing = 0.0f;       // 0..0.75, delays every 2nd step
        bool latch = false;
        int phrase = 0;           // index into arpPhrases()
        int velocityMode = 0;     // 0 played, 1 fixed, 2 accent

        double bpm = 120.0;
        bool hostPlaying = false;
        double ppqAtBlockStart = 0.0;
        double sampleRate = 48000.0;
    };

    void prepare (double sampleRate);
    void reset();

    // Rewrites `midi` in place when enabled (note events consumed, arp notes
    // emitted; non-note messages pass through). Pass-through when disabled.
    void process (juce::MidiBuffer& midi, int numSamples, const Params&);

private:
    static constexpr int maxHeld = 32;
    static constexpr int maxActive = 34;   // chord mode can stack all held

    struct Held
    {
        juce::uint8 note = 0;
        juce::uint8 velocity = 0;
        int arrivalOrder = 0;
    };

    struct Active
    {
        juce::uint8 note = 0;
        double offBeat = 0.0;   // absolute beat position to release at
    };

    void addHeld (juce::uint8 note, juce::uint8 velocity);
    void removeHeld (juce::uint8 note);
    void releaseAllActive (juce::MidiBuffer& out, int samplePos);
    void triggerStep (juce::MidiBuffer& out, int samplePos, const Params&,
                      double stepBeat, double beatsPerStep);
    int pickNoteIndex (const Params&, int sequenceLength);

    Held held[maxHeld];
    int numHeld = 0;
    int arrivalCounter = 0;
    bool latchedChordDown = false;   // physical keys currently down (latch)

    Active active[maxActive];
    int numActive = 0;

    int stepCounter = 0;
    int walkIndex = 0;
    double beatClock = 0.0;          // absolute beats (internal when stopped)
    double lastHostPpq = -1.0e9;
    bool wasEnabled = false;

    juce::Random random;
    juce::MidiBuffer scratch;   // preallocated; no audio-thread allocation
    double currentSampleRate = 48000.0;
};

} // namespace spa::dsp
