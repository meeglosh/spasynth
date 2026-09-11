#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <memory>
#include <vector>

namespace spa::dsp
{

// An SFX/sample loaded into an oscillator slot, plus the offline follower
// analysis computed at load time (background thread). The amp and pitch
// curves are what the SFX-follower mod sources stream at play time — no
// real-time analysis ever runs on the audio thread.
struct SampleData
{
    juce::AudioBuffer<float> audio;      // 1 or 2 channels
    double sourceSampleRate = 48000.0;
    juce::String name;

    // Follower curves, one value per analysis hop.
    // amp: normalized 0..1 (peak of the file = 1).
    // pitch: MIDI note mapped 24..96 -> 0..1; 0 where unvoiced/uncertain.
    std::vector<float> ampCurve;
    std::vector<float> pitchCurve;
    double hopSeconds = 0.010;

    // Tempo sync analysis (transient detection at load time). detectedBpm is
    // folded into 60..180; bpmConfidence 0..1 (0 = one-shot/no reliable
    // periodicity -- SYNC then falls back to "stretch to the nearest whole
    // beat count"). detectedBeats = the file's own length expressed in beats
    // at detectedBpm (UI readout, and syncBeatsOverride==0 default).
    double detectedBpm = 120.0;
    float bpmConfidence = 0.0f;
    float detectedBeats = 1.0f;

    // Source-file-seconds position of the first detected onset (0.0 if none
    // detected/one-shot fallback). Anchors the beat grid used by LOOP+SYNC so
    // grid lines line up with the hits rather than with t = 0.
    double firstOnsetSeconds = 0.0;

    // 1.0 at hops where an onset was detected, else 0.0 -- same hop grid as
    // ampCurve/pitchCurve. Lets the SYNC time-stretch engine shorten grains
    // right at transients (audio-thread read, no allocation) without
    // re-running detection at play time.
    std::vector<float> onsetCurve;
    bool onsetNear (double seconds, double windowSeconds) const noexcept
    {
        if (onsetCurve.empty() || hopSeconds <= 0.0)
            return false;
        const auto span = juce::jmax (1, (int) (windowSeconds / hopSeconds));
        const auto centre = (int) (seconds / hopSeconds);
        for (int i = juce::jmax (0, centre - span); i <= juce::jmin ((int) onsetCurve.size() - 1, centre + span); ++i)
            if (onsetCurve[(size_t) i] > 0.5f)
                return true;
        return false;
    }

    int lengthSamples() const noexcept { return audio.getNumSamples(); }
    double lengthSeconds() const noexcept
    {
        return audio.getNumSamples() / sourceSampleRate;
    }

    // Curve lookup by playback position (audio-thread safe).
    float curveValue (const std::vector<float>& curve, double seconds) const noexcept
    {
        if (curve.empty() || hopSeconds <= 0.0)
            return 0.0f;

        const auto idx = seconds / hopSeconds;
        const auto i0 = juce::jlimit (0, (int) curve.size() - 1, (int) idx);
        const auto i1 = juce::jmin (i0 + 1, (int) curve.size() - 1);
        const auto frac = (float) (idx - (double) i0);
        return curve[(size_t) i0] + frac * (curve[(size_t) i1] - curve[(size_t) i0]);
    }

    float ampAt (double seconds) const noexcept   { return curveValue (ampCurve, seconds); }
    float pitchAt (double seconds) const noexcept { return curveValue (pitchCurve, seconds); }
};

struct LoadedSample
{
    std::shared_ptr<const SampleData> sample;  // null on failure
    juce::String error;
};

// Reads an audio file and runs the offline follower analysis (RMS envelope +
// YIN pitch track). Synchronous and allocating — background thread only.
LoadedSample loadSampleFromFile (const juce::File& file);

} // namespace spa::dsp
