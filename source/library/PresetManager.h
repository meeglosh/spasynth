#pragma once

#include "Library.h"
#include <juce_audio_processors/juce_audio_processors.h>

#include <functional>

namespace spa::library
{

// Preset save/load and the factory-preset generator. Presets are
// human-readable XML wrapping the same state tree the host chunk uses;
// sample paths inside are portable ("$LIB$/...").
//
// Directory layout under the presets root:
//   Factory/<Category>/<Name>.spasynth   (generated from library packs)
//   User/<Name>.spasynth                 (category "User")
//   User/<Bank>/[.../]<Name>.spasynth    (a user preset bank -- category is
//                                          the bank's folder name; anything
//                                          nested deeper still belongs to
//                                          its top-level bank)
class PresetManager : public juce::ChangeBroadcaster
{
public:
    static constexpr const char* presetExtension = ".spasynth";

    struct PresetInfo
    {
        juce::String name;
        juce::String category;
        juce::File file;
        bool isUser = false;   // true for anything under User/ (root or a
                                // bank subfolder). NOT the same as
                                // category == "User" -- a bank's category is
                                // its own folder name.
    };

    PresetManager (std::function<juce::ValueTree()> captureState,
                   std::function<void (const juce::ValueTree&)> applyState,
                   juce::File presetsRoot);

    void rescan();
    const std::vector<PresetInfo>& getPresets() const { return presets; }
    juce::StringArray getCategories() const;

    bool loadPreset (int index);
    bool loadPresetFile (const juce::File&);
    void loadNext();
    void loadPrevious();

    // Restores every parameter to its ParameterRegistry default (the
    // pristine state captured at construction, before any preset/session
    // load) — "start afresh" after randomizing or a long tweak session.
    void resetToDefault();

    // If chosenFolder is inside the User presets folder (a bank the user
    // just picked or created via the save dialog's "New Folder"), the
    // preset is written there; otherwise it falls back to the User root.
    bool saveUserPreset (const juce::String& name, const juce::File& chosenFolder = {});

    // Moves a USER preset to the system Trash and rescans. Returns false --
    // touching nothing at all -- for a factory preset, for a file this
    // manager has not scanned, or if the trash move itself fails.
    //
    // juce::File::moveToTrash(), deliberately, NOT deleteFile(): a deleted
    // preset is a user's own work and this is a one-click action with no
    // confirmation dialog, so it has to stay recoverable (a tester already
    // recovered a preset from the Trash once, on 1.0.10).
    //
    // The empty bank folder that deleting a bank's last preset leaves behind
    // is deliberately left in place: it is the user's own folder, and it
    // drops out of the browser's pack filter by itself (getCategories() is
    // derived from the presets actually found).
    //
    // Navigation: if the deleted preset was the loaded one, the SOUND is left
    // exactly as it is (nothing is re-applied or reset) and only the
    // navigation cursor is cleared -- currentIndex becomes -1, so the next
    // loadNext()/loadPrevious() starts from the ends of the freshly rescanned
    // list instead of indexing a vector whose entries have just shifted.
    // Otherwise the cursor is re-resolved by file so next/prev keep their
    // place across the rescan.
    bool deleteUserPreset (const juce::File& file);

    // Exposed for tests: -1 = nothing loaded from the list.
    int getCurrentIndex() const { return currentIndex; }
    juce::File getUserPresetFolder() const { return presetsRoot.getChildFile ("User"); }

    juce::String getCurrentName() const { return currentName; }

    // Writes showcase presets for each pack (Keys / Texture / Pulse
    // templates, each with several distinct sonic recipes -- see the
    // "Recipe table" comment in PresetManager.cpp). Fast: builds state
    // trees directly, never loads audio. Returns the number of presets
    // written; regenerates a pack's Factory folder whenever its existing
    // presets are missing the "recipe" stamp or carry an older
    // factoryRecipeVersion, and otherwise skips it.
    int generateFactoryPresets (const std::vector<Pack>& packs,
                                const juce::File& libraryRoot);

    // Bumped whenever the factory-preset recipes change; stamped into every
    // generated preset's XML root ("recipe" attribute) so generateFactoryPresets
    // knows to regenerate stale Factory folders. v1 = the original
    // one-recipe-per-archetype scheme (no stamp at all -- treated as v1).
    // v2 = the SFX-follower recipe table. v3 = every Pulse/Keys/Texture
    // variant keeps the pack's own WAV audible in OSC A, drops the
    // arpeggiator everywhere, and switches variant selection from a
    // per-pack hash to round-robin-by-sorted-index (see variantForIndex
    // below) so alphabetically adjacent packs never land on the same
    // recipe by chance. v4 = every Pulse variant now MIXES the pack's own
    // sample with a genuine synth oscillator (wavetable/analog/fm/pluck)
    // driven by that sample's own follower/ENV/LFO in a different way per
    // variant (see the Pulse recipe table in PresetManager.cpp), instead of
    // several variants being sample-only; and every Keys/Texture/Pulse
    // sample layer uses a short safe loop window (and granular a nudged-
    // inward base grain position) so a held note stays audible against a
    // real, long SFX file for as long as it's held, rather than riding that
    // one file's own natural decay into silence -- found and fixed via a
    // real-library audibility pass (factoryPresetsRealLibraryAudibleTest).
    // v5 (1.0.15) = reverbMix values retuned for the new LINEAR mix law
    // (0..1 = dry..wet, replacing the old equal-power crossfade) that ships
    // alongside the Dattorro-plate-derived reverb engine -- the old 0.3-0.6
    // values were ear-tuned against the old curve and read far too wet under
    // the new one. v6 (1.0.15) = Pulse variant 5 ("Bright Pluck Layer")'s
    // amp sustain raised 0.06 -> 0.35: its PLUCK layer decays to silence on
    // its own (a physically-modeled excitation, not a sustaining
    // oscillator) and the old low sustain floor let a held note ride both
    // layers under the silence threshold -- found via --real-library
    // ("Budgie Parakeet Pulse", "Paper Pulse").
    // v7 (1.0.17) = regenerated for the built-in wavetable Table menu and
    // the re-voiced Dattorro-plate reverb engine (Mike's request: "regenerate
    // all the Pulse presets to account for the new wavetable options and the
    // updated reverb"). Pulse's OSC B wavetable layers now pick a table that
    // suits the variant's character (Supersaw, Unison Spread, PWM, Bells --
    // see the Pulse recipe table in PresetManager.cpp) instead of always
    // defaulting to Basic Shapes; the FM layers (variants 1 and 4) stay FM,
    // since the FM oscillator engine has no table param to select at all.
    // Reverb on every Pulse/Keys/Texture variant that uses it now sets an
    // explicit reverbMode (Room/Plate/Chamber/Hall/Spring) suited to its
    // character, with size/decay/damping tuned per mode, and mix kept in
    // the linear-law range (12-20% light, up to ~30% for the drone/wash
    // variants).
    // v9 (1.0.25) = PlateReverb.h's wet output switched from a single end-
    // of-line-only tap to Dattorro's own distributed multi-tap stereo output
    // (fixes reverb arriving noticeably later than the PRE-DELAY knob says).
    // Unlike v8's per-MODE trim, the new taps' level is NOT a uniform per-
    // mode multiplier on the old wet level: part of the sum (the delay1-
    // based "early" taps) is deliberately NOT scaled down by a preset's own
    // decayGain/RT60 the way the rest of the wet path always has been, so
    // how much a given preset's balance moved depends on ITS OWN size/decay
    // settings, not just its mode (a short-decay, small-size Room preset
    // moved far more than a near-default one). So every reverbMix value
    // below that was previously v8-compensated was re-solved PER PRESET
    // (not per mode): mix' = R / (R + Wnew), where R = 10^(target_dB/20) is
    // the preset's pre-1.0.25 (v8) wet-to-dry balance at its own settings
    // (dry RMS is untouched by this change) and Wnew = 10^(measured new
    // wet/dry-at-mix-1_dB/20) is that SAME preset's own new-engine wet/dry
    // ratio -- so every preset keeps exactly its v8 balance, not an
    // averaged/approximate one (reverbNormalisationTest's balance probe
    // pins the result to within 0.3 dB per preset, same as v8). RE-SOLVED
    // 2026-09-24 against PlateReverb.h's early-tap tone fix (kEarlyDampBlend)
    // and the level recalibration that followed it -- the mix' values are
    // NOT the ones the tap-offset fix originally shipped with, since both
    // changed Wnew again for every preset. Confirmed this recompensation is
    // still required (not something the engine-level fixes alone made
    // redundant): a synthetic mode x decay x size sweep
    // (reverbLevelMatchesOldEngineTest) shows the level shift is a real,
    // preset-dependent 1-4 dB even after the engine-level recalibration, so
    // going back to a per-mode-only (v8-style) trim would NOT keep these
    // presets' balance within tolerance.
    static constexpr int factoryRecipeVersion = 9;

    static constexpr int numKeysVariants = 6;
    static constexpr int numTextureVariants = 5;
    static constexpr int numPulseVariants = 6;

    // Deterministic 0-based variant index for the pack's position in the
    // alphabetically-sorted (case-insensitive) pack list, offset per
    // archetype so the three presets generated for one pack don't line up
    // in a fixed pattern across archetypes, and so adjacent packs (index
    // N, N+1) always land on different Pulse/Keys/Texture recipes.
    // NOTE: inserting a new pack shifts every later pack's index and thus
    // its variant assignment -- acceptable, since preset *names* (and so
    // user favorites) are unaffected, only which recipe a pack gets.
    // Exposed for tests.
    static int pulseVariantForIndex (int sortedIndex) { return (sortedIndex + 4) % numPulseVariants; }
    static int keysVariantForIndex (int sortedIndex) { return sortedIndex % numKeysVariants; }
    static int textureVariantForIndex (int sortedIndex) { return (sortedIndex + 2) % numTextureVariants; }

private:
    juce::ValueTree makeTemplateState() const;
    bool writePreset (const juce::File& file, const juce::String& name,
                      const juce::ValueTree& state, int recipeVersionStamp = 0) const;

    juce::ValueTree buildKeysState (const juce::File& smallest, const juce::File& libraryRoot,
                                    int variant) const;
    juce::ValueTree buildTextureState (const juce::File& smallest, const juce::File& middle,
                                       const juce::File& largest, const juce::File& libraryRoot,
                                       int variant) const;
    juce::ValueTree buildPulseState (const juce::File& smallest, const juce::File& middle,
                                     const juce::File& largest, const juce::File& libraryRoot,
                                     int variant) const;

    std::function<juce::ValueTree()> captureState;
    std::function<void (const juce::ValueTree&)> applyState;
    juce::File presetsRoot;

    juce::ValueTree defaultState;   // pristine, captured at construction
    std::vector<PresetInfo> presets;
    int currentIndex = -1;
    juce::String currentName { "Init" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetManager)
};

} // namespace spa::library
