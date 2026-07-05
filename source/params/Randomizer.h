#pragma once

#include "ParameterRegistry.h"

namespace arsenal::params
{

// Lock groups for RANDOMIZE ALL — coarser than sections so the UI stays
// manageable. Locked groups keep their current values on re-roll.
enum class LockGroup
{
    oscillators,
    filter,
    envelopes,
    lfos,
    macros,
    chaos,
    fx,
    matrix,
    count,
};

inline constexpr int numLockGroups = (int) LockGroup::count;

juce::String lockGroupName (LockGroup);
LockGroup lockGroupFor (Section);

// Samples a normalized value from a RandomSpec. wildness 0 = tight around
// the musical centre, 0.5 = the spec's constrained window with its bias,
// 1 = full-range uniform.
float sampleRandomValue (const RandomSpec&, float wildness, juce::Random&);

// Re-rolls every randomizable parameter whose lock group is not in
// lockedMask (bit i = LockGroup i). Message thread only.
void randomizeAll (juce::AudioProcessorValueTreeState&, float wildness,
                   juce::uint32 lockedMask, juce::Random&);

} // namespace arsenal::params
