#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

namespace arsenal::params
{

// Sections drive UI grouping and the per-section lock toggles used by
// RANDOMIZE ALL. Keep this list in sync with sectionName().
enum class Section
{
    global,
    oscA,
    filter1,
    ampEnv,
};

juce::String sectionName (Section);

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

struct ParamDef
{
    const char* id;
    const char* name;
    Section section;
    juce::NormalisableRange<float> range;
    float defaultValue = 0.0f;
    const char* unit = "";
    bool modDestination = false;   // can appear as a mod matrix destination
    RandomSpec random {};
    std::vector<const char*> choices {};  // non-empty => discrete choice param
};

// Stable parameter IDs. Everything (DSP, UI, mod matrix, randomizer) refers to
// parameters through these, never through string literals.
namespace id
{
    inline constexpr const char* masterGain   = "global.masterGain";

    inline constexpr const char* oscAPosition = "oscA.position";
    inline constexpr const char* oscACoarse   = "oscA.coarse";
    inline constexpr const char* oscAFine     = "oscA.fine";
    inline constexpr const char* oscALevel    = "oscA.level";
    inline constexpr const char* oscAPan      = "oscA.pan";

    inline constexpr const char* filter1Type      = "filter1.type";
    inline constexpr const char* filter1Cutoff    = "filter1.cutoff";
    inline constexpr const char* filter1Resonance = "filter1.resonance";
    inline constexpr const char* filter1Drive     = "filter1.drive";

    inline constexpr const char* ampAttack  = "ampEnv.attack";
    inline constexpr const char* ampDecay   = "ampEnv.decay";
    inline constexpr const char* ampSustain = "ampEnv.sustain";
    inline constexpr const char* ampRelease = "ampEnv.release";
}

// Filter type choice order — DSP switches on the raw choice index, so this
// order is load-bearing.
enum class FilterType
{
    lp12, lp24, hp12, hp24, bp12, bp24, notch12, notch24,
};

const std::vector<ParamDef>& all();
const ParamDef* find (const juce::String& paramID);

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

} // namespace arsenal::params
