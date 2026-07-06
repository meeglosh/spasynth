#include "Randomizer.h"

namespace spa::params
{

juce::String lockGroupName (LockGroup g)
{
    switch (g)
    {
        case LockGroup::oscillators: return "OSC";
        case LockGroup::filter:      return "FILTER";
        case LockGroup::envelopes:   return "ENV";
        case LockGroup::lfos:        return "LFO";
        case LockGroup::macros:      return "MACRO";
        case LockGroup::chaos:       return "CHAOS";
        case LockGroup::arp:         return "ARP";
        case LockGroup::fx:          return "FX";
        case LockGroup::matrix:      return "MATRIX";
        case LockGroup::count:       break;
    }
    return {};
}

LockGroup lockGroupFor (Section s)
{
    switch (s)
    {
        case Section::oscA:
        case Section::oscB:
        case Section::oscC:     return LockGroup::oscillators;
        case Section::filter1:
        case Section::filter2:  return LockGroup::filter;
        case Section::ampEnv:
        case Section::env2:
        case Section::env3:     return LockGroup::envelopes;
        case Section::lfo1:
        case Section::lfo2:
        case Section::lfo3:     return LockGroup::lfos;
        case Section::macros:   return LockGroup::macros;
        case Section::chaos:    return LockGroup::chaos;
        case Section::arp:      return LockGroup::arp;
        case Section::fxDist:
        case Section::fxChorus:
        case Section::fxDelay:
        case Section::fxReverb:
        case Section::fxEQ:     return LockGroup::fx;
        case Section::matrix:   return LockGroup::matrix;
        case Section::global:   break;
    }
    return LockGroup::count;  // global: never randomized as a group
}

float sampleRandomValue (const RandomSpec& spec, float wildness, juce::Random& rng)
{
    auto lo = spec.minNorm;
    auto hi = spec.maxNorm;

    if (wildness > 0.5f)
    {
        // Open the window toward the full range.
        const auto t = (wildness - 0.5f) * 2.0f;
        lo = juce::jmap (t, lo, 0.0f);
        hi = juce::jmap (t, hi, 1.0f);
    }
    else
    {
        // Shrink the window toward the musical centre.
        const auto t = (0.5f - wildness) * 2.0f;
        lo = lo + (spec.biasCentre - lo) * t * 0.8f;
        hi = hi - (hi - spec.biasCentre) * t * 0.8f;
    }

    if (hi < lo)
        std::swap (lo, hi);

    auto v = lo + rng.nextFloat() * (hi - lo);

    // Bias fades as wildness rises: full-wild rolls are uniform.
    const auto strength = juce::jlimit (0.0f, 1.0f,
                                        spec.biasStrength * (1.5f - wildness));
    v += (spec.biasCentre - v) * strength;

    return juce::jlimit (0.0f, 1.0f, v);
}

void randomizeAll (juce::AudioProcessorValueTreeState& apvts, float wildness,
                   juce::uint32 lockedMask, juce::Random& rng)
{
    for (const auto& def : all())
    {
        if (! def.random.enabled)
            continue;

        const auto group = lockGroupFor (def.section);
        if (group != LockGroup::count && (lockedMask & (1u << (int) group)) != 0)
            continue;

        if (auto* param = apvts.getParameter (def.id))
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost (sampleRandomValue (def.random, wildness, rng));
            param->endChangeGesture();
        }
    }
}

} // namespace spa::params
