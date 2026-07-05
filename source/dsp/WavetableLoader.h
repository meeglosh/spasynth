#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include "Wavetable.h"

#include <memory>

namespace spa::dsp
{

struct LoadedWavetable
{
    std::shared_ptr<const Wavetable> table;  // null on failure
    juce::String error;
};

// Reads a WAV (or AIFF/FLAC) into a Wavetable. Files whose length is a
// multiple of 2048 samples follow the Serum-style multi-frame convention;
// anything else is treated as a single-cycle wave and resampled.
// Synchronous and allocating — call from a background thread, never the
// audio thread.
LoadedWavetable loadWavetableFromFile (const juce::File& file);

} // namespace spa::dsp
