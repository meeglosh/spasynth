#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

namespace arsenal::params
{

// Voice architecture is sized for 4 oscillator slots; v1 exposes 3.
inline constexpr int numOscSlots = 3;
inline constexpr int maxOscSlots = 4;

// Sections drive UI grouping, host parameter groups, and the per-section lock
// toggles used by RANDOMIZE ALL. Keep in sync with sectionName().
enum class Section
{
    global,
    oscA,
    oscB,
    oscC,
    filter1,
    ampEnv,
};

juce::String sectionName (Section);
Section oscSection (int slotIndex);

// Randomization metadata consumed by RANDOMIZE ALL. Ranges are in normalized
// (0..1) parameter space. biasStrength 0 = uniform across [minNorm, maxNorm];
// 1 = tightly clustered around biasCentre.
struct RandomSpec
{
    bool enabled = true;
    float minNorm = 0.0f;
    float maxNorm = 1.0f;
    float biasCentre = 0.5f;
    float biasStrength = 0.0f;
};

enum class ParamKind { floatParam, intParam, boolParam, choiceParam };

struct ParamDef
{
    juce::String id;
    juce::String name;
    Section section;
    ParamKind kind = ParamKind::floatParam;
    juce::NormalisableRange<float> range;    // floatParam only
    float defaultValue = 0.0f;               // also holds int/bool/choice defaults
    juce::String unit;
    bool modDestination = false;             // can appear as a mod matrix destination
    RandomSpec random {};
    juce::StringArray choices {};            // choiceParam only
};

// Stable parameter IDs. Everything (DSP, UI, mod matrix, randomizer) refers to
// parameters through these, never through ad hoc string literals.
namespace id
{
    inline constexpr const char* masterGain = "global.masterGain";

    inline constexpr const char* filter1Type      = "filter1.type";
    inline constexpr const char* filter1Cutoff    = "filter1.cutoff";
    inline constexpr const char* filter1Resonance = "filter1.resonance";
    inline constexpr const char* filter1Drive     = "filter1.drive";

    inline constexpr const char* ampAttack  = "ampEnv.attack";
    inline constexpr const char* ampDecay   = "ampEnv.decay";
    inline constexpr const char* ampSustain = "ampEnv.sustain";
    inline constexpr const char* ampRelease = "ampEnv.release";

    // Per-oscillator-slot parameter IDs: "oscA.position", "oscB.level", ...
    // Suffix keys:
    namespace osc
    {
        inline constexpr const char* enable       = "enable";
        inline constexpr const char* position     = "position";
        inline constexpr const char* coarse       = "coarse";
        inline constexpr const char* fine         = "fine";
        inline constexpr const char* level        = "level";
        inline constexpr const char* pan          = "pan";
        inline constexpr const char* phase        = "phase";
        inline constexpr const char* phaseMode    = "phaseMode";
        inline constexpr const char* unisonCount  = "unisonCount";
        inline constexpr const char* unisonDetune = "unisonDetune";
        inline constexpr const char* unisonBlend  = "unisonBlend";
        inline constexpr const char* unisonWidth  = "unisonWidth";
    }

    juce::String oscSlot (int slotIndex, const char* key);
    juce::String oscSlotLetter (int slotIndex);
}

// Filter type choice order — DSP switches on the raw choice index, so this
// order is load-bearing.
enum class FilterType
{
    lp12, lp24, hp12, hp24, bp12, bp24, notch12, notch24,
};

// Oscillator phase behaviour on note-on.
enum class PhaseMode { reset, random, free_ };

const std::vector<ParamDef>& all();
const ParamDef* find (const juce::String& paramID);

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

} // namespace arsenal::params
