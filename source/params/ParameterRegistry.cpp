#include "ParameterRegistry.h"

namespace arsenal::params
{

juce::String sectionName (Section s)
{
    switch (s)
    {
        case Section::global:  return "Global";
        case Section::oscA:    return "Osc A";
        case Section::filter1: return "Filter 1";
        case Section::ampEnv:  return "Amp Env";
    }
    return {};
}

static juce::NormalisableRange<float> frequencyRange (float min, float max)
{
    juce::NormalisableRange<float> r (min, max);
    r.setSkewForCentre (std::sqrt (min * max)); // log-ish response, geometric centre
    return r;
}

const std::vector<ParamDef>& all()
{
    static const std::vector<ParamDef> defs = []
    {
        std::vector<ParamDef> p;

        p.push_back ({ id::masterGain, "Master", Section::global,
                       { -60.0f, 6.0f, 0.01f, 2.5f }, -6.0f, "dB",
                       false, { .enabled = false } });

        p.push_back ({ id::oscAPosition, "Position", Section::oscA,
                       { 0.0f, 1.0f }, 0.0f, "",
                       true, { .enabled = true } });
        p.push_back ({ id::oscACoarse, "Coarse", Section::oscA,
                       { -24.0f, 24.0f, 1.0f }, 0.0f, "st",
                       true, { .enabled = true, .minNorm = 0.25f, .maxNorm = 0.75f,
                               .biasCentre = 0.5f, .biasStrength = 0.8f } });
        p.push_back ({ id::oscAFine, "Fine", Section::oscA,
                       { -100.0f, 100.0f, 1.0f }, 0.0f, "ct",
                       true, { .enabled = true, .biasCentre = 0.5f, .biasStrength = 0.9f } });
        p.push_back ({ id::oscALevel, "Level", Section::oscA,
                       { -60.0f, 0.0f, 0.01f, 3.0f }, -6.0f, "dB",
                       true, { .enabled = true, .minNorm = 0.5f, .biasCentre = 0.8f,
                               .biasStrength = 0.5f } });
        p.push_back ({ id::oscAPan, "Pan", Section::oscA,
                       { -1.0f, 1.0f }, 0.0f, "",
                       true, { .enabled = true, .biasCentre = 0.5f, .biasStrength = 0.7f } });

        p.push_back ({ id::filter1Type, "Type", Section::filter1,
                       {}, 0.0f, "", false, { .enabled = true },
                       { "LP 12", "LP 24", "HP 12", "HP 24",
                         "BP 12", "BP 24", "Notch 12", "Notch 24" } });
        p.push_back ({ id::filter1Cutoff, "Cutoff", Section::filter1,
                       frequencyRange (20.0f, 20000.0f), 20000.0f, "Hz",
                       true, { .enabled = true, .minNorm = 0.2f, .biasCentre = 0.6f,
                               .biasStrength = 0.3f } });
        p.push_back ({ id::filter1Resonance, "Resonance", Section::filter1,
                       { 0.0f, 1.0f }, 0.0f, "",
                       true, { .enabled = true, .maxNorm = 0.85f, .biasCentre = 0.3f,
                               .biasStrength = 0.4f } });
        p.push_back ({ id::filter1Drive, "Drive", Section::filter1,
                       { 0.0f, 1.0f }, 0.0f, "",
                       true, { .enabled = true, .maxNorm = 0.7f, .biasCentre = 0.2f,
                               .biasStrength = 0.5f } });

        p.push_back ({ id::ampAttack, "Attack", Section::ampEnv,
                       { 0.001f, 10.0f, 0.0f, 0.35f }, 0.005f, "s",
                       true, { .enabled = true, .maxNorm = 0.6f, .biasCentre = 0.15f,
                               .biasStrength = 0.6f } });
        p.push_back ({ id::ampDecay, "Decay", Section::ampEnv,
                       { 0.001f, 10.0f, 0.0f, 0.35f }, 0.2f, "s",
                       true, { .enabled = true } });
        p.push_back ({ id::ampSustain, "Sustain", Section::ampEnv,
                       { 0.0f, 1.0f }, 0.8f, "",
                       true, { .enabled = true, .minNorm = 0.2f } });
        p.push_back ({ id::ampRelease, "Release", Section::ampEnv,
                       { 0.001f, 20.0f, 0.0f, 0.35f }, 0.15f, "s",
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

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (const auto& def : all())
    {
        const juce::ParameterID pid { def.id, 1 };

        if (! def.choices.empty())
        {
            juce::StringArray names;
            for (auto* c : def.choices)
                names.add (c);

            layout.add (std::make_unique<juce::AudioParameterChoice> (
                pid, def.name, names, (int) def.defaultValue));
        }
        else
        {
            layout.add (std::make_unique<juce::AudioParameterFloat> (
                pid, def.name, def.range, def.defaultValue,
                juce::AudioParameterFloatAttributes().withLabel (def.unit)));
        }
    }

    return layout;
}

} // namespace arsenal::params
