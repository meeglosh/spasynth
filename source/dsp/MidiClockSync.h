#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace spa::dsp
{

// Derives tempo + transport from an incoming MIDI Beat Clock stream (24 pulses
// per quarter note) for the standalone, which has no host playhead. Feed it the
// block's MIDI each processBlock; read bpm()/isPlaying() after.
class MidiClockSync
{
public:
    void prepare (double sampleRate) { sr = sampleRate; reset(); }

    void reset()
    {
        accumSamples = 0;
        lastClockSample = -1;
        smoothedInterval = 0.0;
        running = false;
    }

    void process (const juce::MidiBuffer& midi, int numSamples)
    {
        for (const auto meta : midi)
        {
            const auto m = meta.getMessage();
            const auto t = accumSamples + (juce::int64) meta.samplePosition;

            if (m.isMidiClock())
            {
                if (lastClockSample >= 0)
                {
                    const double interval = (double) (t - lastClockSample);   // samples/clock
                    // 24 clocks per quarter note; sanity-gate to 20..400 BPM.
                    const double instBpm = 60.0 * sr / (24.0 * juce::jmax (1.0, interval));
                    if (interval > 0.0 && instBpm > 20.0 && instBpm < 400.0)
                        smoothedInterval = smoothedInterval > 0.0
                                             ? smoothedInterval * 0.8 + interval * 0.2
                                             : interval;
                }
                lastClockSample = t;
            }
            else if (m.isMidiStart() || m.isMidiContinue()) { running = true; }
            else if (m.isMidiStop())                        { running = false; }
        }

        accumSamples += numSamples;
    }

    bool   hasClock()  const { return smoothedInterval > 0.0; }
    bool   isPlaying() const { return running; }
    double bpm() const
    {
        return smoothedInterval > 0.0 ? 60.0 * sr / (24.0 * smoothedInterval) : 120.0;
    }

private:
    double sr = 48000.0;
    juce::int64 accumSamples = 0;
    juce::int64 lastClockSample = -1;
    double smoothedInterval = 0.0;   // samples between clocks (smoothed)
    bool running = false;
};

} // namespace spa::dsp
