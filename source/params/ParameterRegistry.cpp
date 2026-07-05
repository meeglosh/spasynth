#include "ParameterRegistry.h"

namespace arsenal::params
{

juce::String sectionName (Section s)
{
    switch (s)
    {
        case Section::global:  return "Global";
        case Section::oscA:    return "Osc A";
        case Section::oscB:    return "Osc B";
        case Section::oscC:    return "Osc C";
        case Section::filter1: return "Filter 1";
        case Section::ampEnv:  return "Amp Env";
        case Section::env2:    return "Env 2";
        case Section::env3:    return "Env 3";
        case Section::lfo1:    return "LFO 1";
        case Section::lfo2:    return "LFO 2";
        case Section::lfo3:    return "LFO 3";
        case Section::macros:   return "Macros";
        case Section::chaos:    return "Chaos";
        case Section::fxDist:   return "FX Dist";
        case Section::fxChorus: return "FX Chorus";
        case Section::fxDelay:  return "FX Delay";
        case Section::fxReverb: return "FX Reverb";
        case Section::fxEQ:     return "FX EQ";
        case Section::matrix:   return "Mod Matrix";
    }
    return {};
}

Section oscSection (int slotIndex)
{
    jassert (slotIndex >= 0 && slotIndex < numOscSlots);
    return (Section) ((int) Section::oscA + slotIndex);
}

Section lfoSection (int lfoIndex)
{
    jassert (lfoIndex >= 0 && lfoIndex < numLFOs);
    return (Section) ((int) Section::lfo1 + lfoIndex);
}

const juce::StringArray& modSourceNames()
{
    static const juce::StringArray names {
        "None", "Env 1 (Amp)", "Env 2", "Env 3",
        "LFO 1", "LFO 2", "LFO 3",
        "Macro 1", "Macro 2", "Macro 3", "Macro 4",
        "Velocity", "Mod Wheel", "Aftertouch",
        "Chaos",
        "SFX A Amp", "SFX A Pitch",
        "SFX B Amp", "SFX B Pitch",
        "SFX C Amp", "SFX C Pitch",
    };
    jassert (names.size() == numModSources);
    return names;
}

const juce::StringArray& lfoDivisionNames()
{
    static const juce::StringArray names {
        "8/1", "4/1", "2/1", "1/1",
        "1/2", "1/2T", "1/4", "1/4.", "1/4T",
        "1/8", "1/8.", "1/8T", "1/16", "1/16T", "1/32",
    };
    return names;
}

float lfoDivisionBeats (int divisionChoice)
{
    static constexpr float beats[] = {
        32.0f, 16.0f, 8.0f, 4.0f,
        2.0f, 4.0f / 3.0f, 1.0f, 1.5f, 2.0f / 3.0f,
        0.5f, 0.75f, 1.0f / 3.0f, 0.25f, 1.0f / 6.0f, 0.125f,
    };
    static_assert (std::size (beats) == 15);
    return beats[juce::jlimit (0, (int) std::size (beats) - 1, divisionChoice)];
}

juce::String destDisplayName (const ParamDef& def)
{
    // Osc names already carry their slot letter ("A Position").
    const auto isOsc = def.section == Section::oscA || def.section == Section::oscB
                    || def.section == Section::oscC;
    return isOsc ? "Osc " + def.name : sectionName (def.section) + " " + def.name;
}

namespace id
{
    juce::String oscSlotLetter (int slotIndex)
    {
        return juce::String::charToString ((juce::juce_wchar) ('A' + slotIndex));
    }

    juce::String oscSlot (int slotIndex, const char* key)
    {
        return "osc" + oscSlotLetter (slotIndex) + "." + key;
    }

    juce::String envParam (int envNumber, const char* key)
    {
        jassert (envNumber == 2 || envNumber == 3);
        return "env" + juce::String (envNumber) + "." + key;
    }

    juce::String lfoParam (int lfoIndex, const char* key)
    {
        return "lfo" + juce::String (lfoIndex + 1) + "." + key;
    }

    juce::String macro (int macroIndex)
    {
        return "macros.macro" + juce::String (macroIndex + 1);
    }

    juce::String routeParam (int routeIndex, const char* key)
    {
        return "matrix.route" + juce::String (routeIndex + 1) + "." + key;
    }
}

static juce::NormalisableRange<float> frequencyRange (float min, float max)
{
    juce::NormalisableRange<float> r (min, max);
    r.setSkewForCentre (std::sqrt (min * max)); // log-ish response, geometric centre
    return r;
}

static void addOscSlotParams (std::vector<ParamDef>& p, int slot)
{
    const auto section = oscSection (slot);
    const auto letter = id::oscSlotLetter (slot) + " ";
    const auto pid = [slot] (const char* key) { return id::oscSlot (slot, key); };

    // Slot A defaults on; others off. Randomizer may toggle B/C but leaves at
    // least A biased strongly on so patches always sound.
    p.push_back ({ pid (id::osc::enable), letter + "Enable", section,
                   ParamKind::boolParam, {}, slot == 0 ? 1.0f : 0.0f, "",
                   false, { .enabled = true, .biasCentre = slot == 0 ? 1.0f : 0.6f,
                            .biasStrength = slot == 0 ? 1.0f : 0.3f } });
    p.push_back ({ pid (id::osc::position), letter + "Position", section,
                   ParamKind::floatParam, { 0.0f, 1.0f }, 0.0f, "",
                   true, { .enabled = true } });
    p.push_back ({ pid (id::osc::coarse), letter + "Coarse", section,
                   ParamKind::floatParam, { -24.0f, 24.0f, 1.0f }, 0.0f, "st",
                   true, { .enabled = true, .minNorm = 0.25f, .maxNorm = 0.75f,
                           .biasCentre = 0.5f, .biasStrength = 0.8f } });
    p.push_back ({ pid (id::osc::fine), letter + "Fine", section,
                   ParamKind::floatParam, { -100.0f, 100.0f, 1.0f }, 0.0f, "ct",
                   true, { .enabled = true, .biasCentre = 0.5f, .biasStrength = 0.9f } });
    p.push_back ({ pid (id::osc::level), letter + "Level", section,
                   ParamKind::floatParam, { -60.0f, 0.0f, 0.01f, 3.0f }, -6.0f, "dB",
                   true, { .enabled = true, .minNorm = 0.5f, .biasCentre = 0.8f,
                           .biasStrength = 0.5f } });
    p.push_back ({ pid (id::osc::pan), letter + "Pan", section,
                   ParamKind::floatParam, { -1.0f, 1.0f }, 0.0f, "",
                   true, { .enabled = true, .biasCentre = 0.5f, .biasStrength = 0.7f } });
    p.push_back ({ pid (id::osc::phase), letter + "Phase", section,
                   ParamKind::floatParam, { 0.0f, 1.0f }, 0.0f, "",
                   false, { .enabled = true, .biasCentre = 0.0f, .biasStrength = 0.8f } });
    p.push_back ({ pid (id::osc::phaseMode), letter + "Phase Mode", section,
                   ParamKind::choiceParam, {}, 0.0f, "",
                   false, { .enabled = true },
                   { "Reset", "Random", "Free" } });
    p.push_back ({ pid (id::osc::unisonCount), letter + "Unison", section,
                   ParamKind::intParam, { 1.0f, 8.0f, 1.0f }, 1.0f, "",
                   false, { .enabled = true, .maxNorm = 0.75f, .biasCentre = 0.1f,
                            .biasStrength = 0.5f } });
    p.push_back ({ pid (id::osc::unisonDetune), letter + "Uni Detune", section,
                   ParamKind::floatParam, { 0.0f, 100.0f, 0.0f, 0.5f }, 15.0f, "ct",
                   true, { .enabled = true, .maxNorm = 0.6f, .biasCentre = 0.2f,
                           .biasStrength = 0.4f } });
    p.push_back ({ pid (id::osc::unisonBlend), letter + "Uni Blend", section,
                   ParamKind::floatParam, { 0.0f, 1.0f }, 0.7f, "",
                   true, { .enabled = true } });
    p.push_back ({ pid (id::osc::unisonWidth), letter + "Uni Width", section,
                   ParamKind::floatParam, { 0.0f, 1.0f }, 0.8f, "",
                   true, { .enabled = true, .biasCentre = 0.7f, .biasStrength = 0.3f } });

    // --- Sample/SFX engine ---------------------------------------------------
    p.push_back ({ pid (id::osc::mode), letter + "Mode", section,
                   ParamKind::choiceParam, {}, 0.0f, "",
                   false, { .enabled = true, .biasCentre = 0.0f, .biasStrength = 0.5f },
                   { "Wavetable", "Sample", "Granular" } });
    p.push_back ({ pid (id::osc::sampleStart), letter + "Smp Start", section,
                   ParamKind::floatParam, { 0.0f, 1.0f }, 0.0f, "",
                   false, { .enabled = true, .maxNorm = 0.7f, .biasCentre = 0.0f,
                            .biasStrength = 0.6f } });
    p.push_back ({ pid (id::osc::loop), letter + "Loop", section,
                   ParamKind::boolParam, {}, 1.0f, "", false, { .enabled = true } });
    p.push_back ({ pid (id::osc::loopStart), letter + "Loop Start", section,
                   ParamKind::floatParam, { 0.0f, 1.0f }, 0.0f, "",
                   false, { .enabled = true, .biasCentre = 0.1f, .biasStrength = 0.5f } });
    p.push_back ({ pid (id::osc::loopEnd), letter + "Loop End", section,
                   ParamKind::floatParam, { 0.0f, 1.0f }, 1.0f, "",
                   false, { .enabled = true, .biasCentre = 0.9f, .biasStrength = 0.5f } });
    p.push_back ({ pid (id::osc::keytrack), letter + "Keytrack", section,
                   ParamKind::boolParam, {}, 1.0f, "", false, { .enabled = true } });
    p.push_back ({ pid (id::osc::rootNote), letter + "Root Note", section,
                   ParamKind::intParam, { 0.0f, 127.0f, 1.0f }, 60.0f, "",
                   false, { .enabled = false } });
    p.push_back ({ pid (id::osc::grainSize), letter + "Grain Size", section,
                   ParamKind::floatParam, { 10.0f, 500.0f, 0.0f, 0.4f }, 80.0f, "ms",
                   true, { .enabled = true, .biasCentre = 0.3f, .biasStrength = 0.3f } });
    p.push_back ({ pid (id::osc::grainDensity), letter + "Grain Density", section,
                   ParamKind::floatParam, { 1.0f, 100.0f, 0.0f, 0.4f }, 20.0f, "/s",
                   true, { .enabled = true, .biasCentre = 0.4f, .biasStrength = 0.3f } });
    p.push_back ({ pid (id::osc::grainPos), letter + "Grain Pos", section,
                   ParamKind::floatParam, { 0.0f, 1.0f }, 0.0f, "",
                   true, { .enabled = true } });
    p.push_back ({ pid (id::osc::grainSpray), letter + "Grain Spray", section,
                   ParamKind::floatParam, { 0.0f, 1.0f }, 0.1f, "",
                   true, { .enabled = true, .maxNorm = 0.7f, .biasCentre = 0.2f,
                           .biasStrength = 0.3f } });
    p.push_back ({ pid (id::osc::grainPitch), letter + "Grain Pitch", section,
                   ParamKind::floatParam, { -24.0f, 24.0f, 1.0f }, 0.0f, "st",
                   false, { .enabled = true, .minNorm = 0.25f, .maxNorm = 0.75f,
                            .biasCentre = 0.5f, .biasStrength = 0.7f } });
}

static void addADSRParams (std::vector<ParamDef>& p, Section section, const juce::String& idPrefix)
{
    p.push_back ({ idPrefix + ".attack", "Attack", section,
                   ParamKind::floatParam, { 0.001f, 10.0f, 0.0f, 0.35f }, 0.005f, "s",
                   true, { .enabled = true, .maxNorm = 0.6f, .biasCentre = 0.15f,
                           .biasStrength = 0.6f } });
    p.push_back ({ idPrefix + ".decay", "Decay", section,
                   ParamKind::floatParam, { 0.001f, 10.0f, 0.0f, 0.35f }, 0.2f, "s",
                   true, { .enabled = true } });
    p.push_back ({ idPrefix + ".sustain", "Sustain", section,
                   ParamKind::floatParam, { 0.0f, 1.0f }, 0.8f, "",
                   true, { .enabled = true, .minNorm = 0.2f } });
    p.push_back ({ idPrefix + ".release", "Release", section,
                   ParamKind::floatParam, { 0.001f, 20.0f, 0.0f, 0.35f }, 0.15f, "s",
                   true, { .enabled = true, .maxNorm = 0.7f } });
}

static void addLFOParams (std::vector<ParamDef>& p, int lfoIndex)
{
    const auto section = lfoSection (lfoIndex);
    const auto prefix = "L" + juce::String (lfoIndex + 1) + " ";
    const auto pid = [lfoIndex] (const char* key) { return id::lfoParam (lfoIndex, key); };

    p.push_back ({ pid (id::lfo::shape), prefix + "Shape", section,
                   ParamKind::choiceParam, {}, 0.0f, "",
                   false, { .enabled = true },
                   { "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "S&H" } });
    p.push_back ({ pid (id::lfo::rate), prefix + "Rate", section,
                   ParamKind::floatParam, frequencyRange (0.01f, 40.0f), 1.0f, "Hz",
                   false, { .enabled = true, .minNorm = 0.2f, .maxNorm = 0.8f } });
    p.push_back ({ pid (id::lfo::sync), prefix + "Sync", section,
                   ParamKind::boolParam, {}, 0.0f, "",
                   false, { .enabled = true } });
    p.push_back ({ pid (id::lfo::division), prefix + "Division", section,
                   ParamKind::choiceParam, {}, 6.0f /* 1/4 */, "",
                   false, { .enabled = true, .minNorm = 0.2f, .maxNorm = 0.9f },
                   lfoDivisionNames() });
    p.push_back ({ pid (id::lfo::phase), prefix + "Phase", section,
                   ParamKind::floatParam, { 0.0f, 1.0f }, 0.0f, "",
                   false, { .enabled = true, .biasCentre = 0.0f, .biasStrength = 0.8f } });
    p.push_back ({ pid (id::lfo::retrig), prefix + "Retrig", section,
                   ParamKind::boolParam, {}, 1.0f, "",
                   false, { .enabled = true } });
    p.push_back ({ pid (id::lfo::unipolar), prefix + "Unipolar", section,
                   ParamKind::boolParam, {}, 0.0f, "",
                   false, { .enabled = true } });
}

// Non-matrix parameter definitions, in registry order.
static std::vector<ParamDef> buildCoreDefs()
{
    std::vector<ParamDef> p;

    p.push_back ({ id::masterGain, "Master", Section::global,
                   ParamKind::floatParam, { -60.0f, 6.0f, 0.01f, 2.5f }, -6.0f, "dB",
                   false, { .enabled = false } });

    for (int slot = 0; slot < numOscSlots; ++slot)
        addOscSlotParams (p, slot);

    p.push_back ({ id::filter1Type, "Type", Section::filter1,
                   ParamKind::choiceParam, {}, 0.0f, "",
                   false, { .enabled = true },
                   { "LP 12", "LP 24", "HP 12", "HP 24",
                     "BP 12", "BP 24", "Notch 12", "Notch 24" } });
    p.push_back ({ id::filter1Cutoff, "Cutoff", Section::filter1,
                   ParamKind::floatParam, frequencyRange (20.0f, 20000.0f), 20000.0f, "Hz",
                   true, { .enabled = true, .minNorm = 0.2f, .biasCentre = 0.6f,
                           .biasStrength = 0.3f } });
    p.push_back ({ id::filter1Resonance, "Resonance", Section::filter1,
                   ParamKind::floatParam, { 0.0f, 1.0f }, 0.0f, "",
                   true, { .enabled = true, .maxNorm = 0.85f, .biasCentre = 0.3f,
                           .biasStrength = 0.4f } });
    p.push_back ({ id::filter1Drive, "Drive", Section::filter1,
                   ParamKind::floatParam, { 0.0f, 1.0f }, 0.0f, "",
                   true, { .enabled = true, .maxNorm = 0.7f, .biasCentre = 0.2f,
                           .biasStrength = 0.5f } });

    addADSRParams (p, Section::ampEnv, "ampEnv");
    addADSRParams (p, Section::env2, "env2");
    addADSRParams (p, Section::env3, "env3");

    for (int i = 0; i < numLFOs; ++i)
        addLFOParams (p, i);

    for (int m = 0; m < numMacros; ++m)
        p.push_back ({ id::macro (m), "Macro " + juce::String (m + 1), Section::macros,
                       ParamKind::floatParam, { 0.0f, 1.0f }, 0.0f, "",
                       false, { .enabled = true, .biasCentre = 0.3f, .biasStrength = 0.3f } });

    // --- Organic Chaos ------------------------------------------------------
    namespace ch = id::chaos;
    p.push_back ({ ch::enable, "Chaos Enable", Section::chaos,
                   ParamKind::boolParam, {}, 1.0f, "",
                   false, { .enabled = true, .biasCentre = 0.8f, .biasStrength = 0.4f } });
    p.push_back ({ ch::depth, "Depth", Section::chaos,
                   ParamKind::floatParam, { 0.0f, 1.0f }, 0.4f, "",
                   true, { .enabled = true, .biasCentre = 0.4f, .biasStrength = 0.3f } });
    p.push_back ({ ch::rate, "Rate", Section::chaos,
                   ParamKind::floatParam, frequencyRange (0.05f, 25.0f), 2.0f, "Hz",
                   true, { .enabled = true, .minNorm = 0.1f, .maxNorm = 0.8f } });
    p.push_back ({ ch::mix, "Mix", Section::chaos,
                   ParamKind::floatParam, { 0.0f, 1.0f }, 1.0f, "",
                   true, { .enabled = true, .minNorm = 0.3f, .biasCentre = 0.8f,
                           .biasStrength = 0.3f } });
    p.push_back ({ ch::pitchOn, "Pitch Drift", Section::chaos,
                   ParamKind::boolParam, {}, 1.0f, "", false, { .enabled = true } });
    p.push_back ({ ch::pitchAmount, "Pitch Amt", Section::chaos,
                   ParamKind::floatParam, { 0.0f, 100.0f, 0.0f, 0.4f }, 8.0f, "ct",
                   false, { .enabled = true, .maxNorm = 0.5f, .biasCentre = 0.15f,
                            .biasStrength = 0.5f } });
    p.push_back ({ ch::phaseOn, "Phase Drift", Section::chaos,
                   ParamKind::boolParam, {}, 1.0f, "", false, { .enabled = true } });
    p.push_back ({ ch::phaseAmount, "Phase Amt", Section::chaos,
                   ParamKind::floatParam, { 0.0f, 1.0f }, 0.15f, "",
                   false, { .enabled = true, .biasCentre = 0.2f, .biasStrength = 0.4f } });
    p.push_back ({ ch::positionOn, "Position Drift", Section::chaos,
                   ParamKind::boolParam, {}, 1.0f, "", false, { .enabled = true } });
    p.push_back ({ ch::positionAmount, "Position Amt", Section::chaos,
                   ParamKind::floatParam, { 0.0f, 1.0f }, 0.2f, "",
                   false, { .enabled = true, .biasCentre = 0.25f, .biasStrength = 0.3f } });
    p.push_back ({ ch::ampOn, "Amp Drift", Section::chaos,
                   ParamKind::boolParam, {}, 1.0f, "", false, { .enabled = true } });
    p.push_back ({ ch::ampAmount, "Amp Amt", Section::chaos,
                   ParamKind::floatParam, { 0.0f, 1.0f }, 0.2f, "",
                   false, { .enabled = true, .biasCentre = 0.2f, .biasStrength = 0.4f } });
    p.push_back ({ ch::satOn, "Sat Drift", Section::chaos,
                   ParamKind::boolParam, {}, 0.0f, "", false, { .enabled = true } });
    p.push_back ({ ch::saturation, "Saturation", Section::chaos,
                   ParamKind::floatParam, { 0.0f, 1.0f }, 0.3f, "",
                   false, { .enabled = true, .maxNorm = 0.8f, .biasCentre = 0.3f,
                            .biasStrength = 0.3f } });
    p.push_back ({ ch::distOn, "Dist Drift", Section::chaos,
                   ParamKind::boolParam, {}, 0.0f, "", false, { .enabled = true } });
    p.push_back ({ ch::distortion, "Distortion", Section::chaos,
                   ParamKind::floatParam, { 0.0f, 1.0f }, 0.2f, "",
                   false, { .enabled = true, .maxNorm = 0.7f, .biasCentre = 0.2f,
                            .biasStrength = 0.4f } });

    // --- FX chain -------------------------------------------------------
    namespace fx = id::fx;

    p.push_back ({ fx::distEnable, "Dist On", Section::fxDist,
                   ParamKind::boolParam, {}, 0.0f, "",
                   false, { .enabled = true, .biasCentre = 0.3f, .biasStrength = 0.3f } });
    p.push_back ({ fx::distType, "Dist Type", Section::fxDist,
                   ParamKind::choiceParam, {}, 0.0f, "", false, { .enabled = true },
                   { "Soft", "Hard", "Fold" } });
    p.push_back ({ fx::distDrive, "Dist Drive", Section::fxDist,
                   ParamKind::floatParam, { 0.0f, 1.0f }, 0.3f, "",
                   false, { .enabled = true, .maxNorm = 0.8f, .biasCentre = 0.3f,
                            .biasStrength = 0.3f } });
    p.push_back ({ fx::distTone, "Dist Tone", Section::fxDist,
                   ParamKind::floatParam, frequencyRange (500.0f, 20000.0f), 8000.0f, "Hz",
                   false, { .enabled = true, .minNorm = 0.3f } });
    p.push_back ({ fx::distMix, "Dist Mix", Section::fxDist,
                   ParamKind::floatParam, { 0.0f, 1.0f }, 1.0f, "",
                   false, { .enabled = true, .minNorm = 0.3f } });

    p.push_back ({ fx::chorusEnable, "Chorus On", Section::fxChorus,
                   ParamKind::boolParam, {}, 0.0f, "",
                   false, { .enabled = true, .biasCentre = 0.4f, .biasStrength = 0.3f } });
    p.push_back ({ fx::chorusRate, "Chorus Rate", Section::fxChorus,
                   ParamKind::floatParam, frequencyRange (0.05f, 5.0f), 0.8f, "Hz",
                   false, { .enabled = true, .maxNorm = 0.7f } });
    p.push_back ({ fx::chorusDepth, "Chorus Depth", Section::fxChorus,
                   ParamKind::floatParam, { 0.0f, 1.0f }, 0.3f, "",
                   false, { .enabled = true, .biasCentre = 0.35f, .biasStrength = 0.3f } });
    p.push_back ({ fx::chorusFeedback, "Chorus FB", Section::fxChorus,
                   ParamKind::floatParam, { -0.9f, 0.9f }, 0.0f, "",
                   false, { .enabled = true, .biasCentre = 0.5f, .biasStrength = 0.6f } });
    p.push_back ({ fx::chorusMix, "Chorus Mix", Section::fxChorus,
                   ParamKind::floatParam, { 0.0f, 1.0f }, 0.5f, "",
                   false, { .enabled = true, .biasCentre = 0.5f, .biasStrength = 0.3f } });

    p.push_back ({ fx::delayEnable, "Delay On", Section::fxDelay,
                   ParamKind::boolParam, {}, 0.0f, "",
                   false, { .enabled = true, .biasCentre = 0.4f, .biasStrength = 0.3f } });
    p.push_back ({ fx::delaySync, "Delay Sync", Section::fxDelay,
                   ParamKind::boolParam, {}, 1.0f, "",
                   false, { .enabled = true, .biasCentre = 0.8f, .biasStrength = 0.5f } });
    p.push_back ({ fx::delayTime, "Delay Time", Section::fxDelay,
                   ParamKind::floatParam, { 1.0f, 2000.0f, 0.0f, 0.4f }, 350.0f, "ms",
                   false, { .enabled = true, .minNorm = 0.2f, .maxNorm = 0.8f } });
    p.push_back ({ fx::delayDivision, "Delay Div", Section::fxDelay,
                   ParamKind::choiceParam, {}, 6.0f /* 1/4 */, "",
                   false, { .enabled = true, .minNorm = 0.3f, .maxNorm = 0.9f },
                   lfoDivisionNames() });
    p.push_back ({ fx::delayFeedback, "Delay FB", Section::fxDelay,
                   ParamKind::floatParam, { 0.0f, 0.95f }, 0.35f, "",
                   false, { .enabled = true, .maxNorm = 0.75f, .biasCentre = 0.4f,
                            .biasStrength = 0.3f } });
    p.push_back ({ fx::delayPingPong, "Ping Pong", Section::fxDelay,
                   ParamKind::boolParam, {}, 0.0f, "", false, { .enabled = true } });
    p.push_back ({ fx::delayMix, "Delay Mix", Section::fxDelay,
                   ParamKind::floatParam, { 0.0f, 1.0f }, 0.35f, "",
                   false, { .enabled = true, .maxNorm = 0.8f, .biasCentre = 0.35f,
                            .biasStrength = 0.3f } });

    p.push_back ({ fx::reverbEnable, "Reverb On", Section::fxReverb,
                   ParamKind::boolParam, {}, 0.0f, "",
                   false, { .enabled = true, .biasCentre = 0.6f, .biasStrength = 0.3f } });
    p.push_back ({ fx::reverbSize, "Reverb Size", Section::fxReverb,
                   ParamKind::floatParam, { 0.0f, 1.0f }, 0.5f, "",
                   false, { .enabled = true } });
    p.push_back ({ fx::reverbDamping, "Reverb Damp", Section::fxReverb,
                   ParamKind::floatParam, { 0.0f, 1.0f }, 0.5f, "",
                   false, { .enabled = true } });
    p.push_back ({ fx::reverbWidth, "Reverb Width", Section::fxReverb,
                   ParamKind::floatParam, { 0.0f, 1.0f }, 1.0f, "",
                   false, { .enabled = true, .minNorm = 0.4f } });
    p.push_back ({ fx::reverbMix, "Reverb Mix", Section::fxReverb,
                   ParamKind::floatParam, { 0.0f, 1.0f }, 0.3f, "",
                   false, { .enabled = true, .maxNorm = 0.8f, .biasCentre = 0.3f,
                            .biasStrength = 0.3f } });

    p.push_back ({ fx::eqEnable, "EQ On", Section::fxEQ,
                   ParamKind::boolParam, {}, 0.0f, "",
                   false, { .enabled = true, .biasCentre = 0.3f, .biasStrength = 0.4f } });
    p.push_back ({ fx::eqLowGain, "EQ Low", Section::fxEQ,
                   ParamKind::floatParam, { -12.0f, 12.0f, 0.1f }, 0.0f, "dB",
                   false, { .enabled = true, .biasCentre = 0.5f, .biasStrength = 0.6f } });
    p.push_back ({ fx::eqMidFreq, "EQ Mid Freq", Section::fxEQ,
                   ParamKind::floatParam, frequencyRange (200.0f, 8000.0f), 1000.0f, "Hz",
                   false, { .enabled = true } });
    p.push_back ({ fx::eqMidGain, "EQ Mid", Section::fxEQ,
                   ParamKind::floatParam, { -12.0f, 12.0f, 0.1f }, 0.0f, "dB",
                   false, { .enabled = true, .biasCentre = 0.5f, .biasStrength = 0.6f } });
    p.push_back ({ fx::eqHighGain, "EQ High", Section::fxEQ,
                   ParamKind::floatParam, { -12.0f, 12.0f, 0.1f }, 0.0f, "dB",
                   false, { .enabled = true, .biasCentre = 0.5f, .biasStrength = 0.6f } });

    return p;
}

const std::vector<ParamDef>& all()
{
    static const std::vector<ParamDef> defs = []
    {
        auto p = buildCoreDefs();

        // Matrix routes are appended after everything else so the destination
        // choice list can be generated from the defs above.
        juce::StringArray destNames { "None" };
        for (const auto& def : p)
            if (def.modDestination)
                destNames.add (destDisplayName (def));

        for (int r = 0; r < numModRoutes; ++r)
        {
            const auto label = "R" + juce::String (r + 1) + " ";
            p.push_back ({ id::routeParam (r, id::route::source), label + "Source",
                           Section::matrix, ParamKind::choiceParam, {}, 0.0f, "",
                           false, { .enabled = true }, modSourceNames() });
            p.push_back ({ id::routeParam (r, id::route::dest), label + "Dest",
                           Section::matrix, ParamKind::choiceParam, {}, 0.0f, "",
                           false, { .enabled = true }, destNames });
            p.push_back ({ id::routeParam (r, id::route::depth), label + "Depth",
                           Section::matrix, ParamKind::floatParam,
                           { -1.0f, 1.0f }, 0.0f, "",
                           false, { .enabled = true, .biasCentre = 0.5f,
                                    .biasStrength = 0.4f } });
        }

        return p;
    }();

    return defs;
}

const std::vector<ModDest>& modDestinations()
{
    static const std::vector<ModDest> dests = []
    {
        std::vector<ModDest> d;
        int index = 0;
        for (const auto& def : all())
            if (def.modDestination)
                d.push_back ({ &def, index++ });
        return d;
    }();

    return dests;
}

int numModDests()
{
    return (int) modDestinations().size();
}

int modDestIndex (const juce::String& paramID)
{
    for (const auto& dest : modDestinations())
        if (dest.def->id == paramID)
            return dest.index;

    return -1;
}

const ParamDef* find (const juce::String& paramID)
{
    for (const auto& def : all())
        if (paramID == def.id)
            return &def;

    return nullptr;
}

static std::unique_ptr<juce::RangedAudioParameter> makeParameter (const ParamDef& def)
{
    const juce::ParameterID pid { def.id, 1 };

    switch (def.kind)
    {
        case ParamKind::boolParam:
            return std::make_unique<juce::AudioParameterBool> (pid, def.name,
                                                               def.defaultValue >= 0.5f);
        case ParamKind::intParam:
            return std::make_unique<juce::AudioParameterInt> (pid, def.name,
                                                              (int) def.range.start,
                                                              (int) def.range.end,
                                                              (int) def.defaultValue);
        case ParamKind::choiceParam:
            return std::make_unique<juce::AudioParameterChoice> (pid, def.name, def.choices,
                                                                 (int) def.defaultValue);
        case ParamKind::floatParam:
            break;
    }

    return std::make_unique<juce::AudioParameterFloat> (
        pid, def.name, def.range, def.defaultValue,
        juce::AudioParameterFloatAttributes().withLabel (def.unit));
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (auto section : allSections)
    {
        auto group = std::make_unique<juce::AudioProcessorParameterGroup> (
            sectionName (section), sectionName (section), "|");

        for (const auto& def : all())
            if (def.section == section)
                group->addChild (makeParameter (def));

        layout.add (std::move (group));
    }

    return layout;
}

} // namespace arsenal::params
