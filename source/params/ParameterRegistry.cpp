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
    }
    return {};
}

Section oscSection (int slotIndex)
{
    jassert (slotIndex >= 0 && slotIndex < numOscSlots);
    return (Section) ((int) Section::oscA + slotIndex);
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
                   true, { .enabled = true, .biasCentre = 0.0f, .biasStrength = 0.8f } });
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
}

const std::vector<ParamDef>& all()
{
    static const std::vector<ParamDef> defs = []
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

        p.push_back ({ id::ampAttack, "Attack", Section::ampEnv,
                       ParamKind::floatParam, { 0.001f, 10.0f, 0.0f, 0.35f }, 0.005f, "s",
                       true, { .enabled = true, .maxNorm = 0.6f, .biasCentre = 0.15f,
                               .biasStrength = 0.6f } });
        p.push_back ({ id::ampDecay, "Decay", Section::ampEnv,
                       ParamKind::floatParam, { 0.001f, 10.0f, 0.0f, 0.35f }, 0.2f, "s",
                       true, { .enabled = true } });
        p.push_back ({ id::ampSustain, "Sustain", Section::ampEnv,
                       ParamKind::floatParam, { 0.0f, 1.0f }, 0.8f, "",
                       true, { .enabled = true, .minNorm = 0.2f } });
        p.push_back ({ id::ampRelease, "Release", Section::ampEnv,
                       ParamKind::floatParam, { 0.001f, 20.0f, 0.0f, 0.35f }, 0.15f, "s",
                       true, { .enabled = true, .maxNorm = 0.7f } });

        return p;
    }();

    return defs;
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

    // One host-visible parameter group per section.
    constexpr Section sections[] = { Section::global, Section::oscA, Section::oscB,
                                     Section::oscC, Section::filter1, Section::ampEnv };

    for (auto section : sections)
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
