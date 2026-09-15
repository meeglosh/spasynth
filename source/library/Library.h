#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <juce_graphics/juce_graphics.h>
#include <vector>

namespace spa::library
{

// One pack folder = one preset category (the Silverplatter convention).
struct Pack
{
    juce::String name;
    juce::File folder;
    juce::Array<juce::File> wavs;   // sorted by file size, ascending
};

// Scans a library root: every direct subfolder containing WAVs is a pack.
// WAV files sitting directly in the root (no subfolder) also form a pack of
// their own, named after the root folder itself.
std::vector<Pack> scanLibrary (const juce::File& root);

// --- Machine-level settings (library location, not per-session) -----------
juce::File getLibraryRoot();
void setLibraryRoot (const juce::File&);

// Last folder browsed in the sample/wavetable file chooser, and separately
// for the convolution IR chooser (kept apart since these are typically
// browsed from very different locations, e.g. an SFX library vs. an
// impulse-response folder). Returns an invalid File when never set or the
// remembered folder is no longer reachable (e.g. its drive is unplugged).
juce::File getLastContentFolder();
void setLastContentFolder (const juce::File& pickedFile);
juce::File getLastIRFolder();
void setLastIRFolder (const juce::File& pickedFile);

// UI accent colour overrides, remembered across sessions. Getters return
// the fallback when the user has never customized.
juce::Colour getAccentColor (juce::Colour fallback);
juce::Colour getAccentModColor (juce::Colour fallback);
void setAccentColors (juce::Colour accent, juce::Colour accentMod);

// Whether the picker links both accents to one colour input.
bool getAccentsLinked();
void setAccentsLinked (bool linked);

// Whether the "we couldn't find your sound library" one-shot editor prompt
// has already been shown on this machine (see
// SPASynthProcessor::consumeEmptyLibraryPromptDecision()). False until the
// prompt has actually been shown once.
bool getEmptyLibraryPromptShown();
void setEmptyLibraryPromptShown (bool shown);

// Favorite presets (machine preference, like the theme). Keys are
// "<category>/<name>" so they survive the presets root moving.
juce::StringArray getFavoritePresets();
bool isPresetFavorite (const juce::String& key);
void setPresetFavorite (const juce::String& key, bool favorite);

// Quick structural check: does this folder look like an SPASynth library
// (at least one pack subfolder containing WAVs, or WAVs directly in the
// root)? Agrees exactly with whether scanLibrary() would yield a pack.
bool looksLikeLibrary (const juce::File&);

// The standard install locations the content installers write to, most
// preferred first. The plugin probes these so users never have to point
// SPASynth at the library manually.
std::vector<juce::File> defaultLibraryLocations();

// Hard cap on how many directories expandLibraryCandidates() will list while
// probing for wrapper folders -- bounds cost to a handful of directory
// listings even when a base dir contains hundreds of sibling folders,
// instead of a tree walk.
constexpr int maxCandidateDirsExamined = 300;

// Expands base/company dirs into an ordered candidate list, most canonical
// first (discoverLibrary() takes the first hit, so a correct install must
// always win over a stray wrapper folder):
//   1. "<companyDir>/<libraryName>" for every company dir -- the canonical
//      install location.
//   2. "<companyDir>/<anything>/<libraryName>" -- a wrapper folder still
//      inside the right company dir.
//   3. "<baseDir>/<libraryName>" -- the user extracted only the inner
//      folder, skipping the company-dir level entirely.
//   4. "<baseDir>/<anything>/<company>/<libraryName>" -- Windows Explorer's
//      "Extract All" default: it wraps the zip's contents in a folder named
//      after the zip, so our "Silverplatter Audio/SPASynth Library" payload
//      ends up one level deeper than usual.
//   5. any other existing subfolder of each company dir, as a last-resort
//      fallback (a renamed library, a lone add-on pack).
// Never recurses below these fixed levels, and stops listing further
// directories once maxCandidateDirsExamined have been examined. Pure given
// its inputs (testable); discoverLibrary() still vets every candidate.
std::vector<juce::File> expandLibraryCandidates (const std::vector<juce::File>& baseDirs,
                                                 const std::vector<juce::File>& companyDirs,
                                                 const juce::String& company,
                                                 const juce::String& libraryName);

// Pure discovery over a candidate list (testable).
juce::File discoverLibrary (const std::vector<juce::File>& candidates);

// The library root SPASynth should use right now: the configured root if it
// still exists on disk (even if it currently has zero packs -- a user's
// explicit folder choice is never second-guessed), otherwise the first valid
// default location (which is then persisted). Returns an invalid File only
// if nothing is found — the manual "Set Library Folder..." fallback covers
// that case.
juce::File findLibraryRoot();

// Where presets live: <app data>/Silverplatter Audio/SPASynth/Presets with
// Factory/<Category>/ and User/ underneath.
juce::File defaultPresetsRoot();

// Process-wide override for defaultPresetsRoot(), message-thread only. Used
// exclusively by the test suite to redirect preset I/O away from the user's
// real Presets folder into a temp directory. Pass an invalid/empty File to
// clear the override. Never used by shipping product code paths.
void setPresetsRootOverride (const juce::File& root);

// Process-wide override for the machine-settings PropertiesFile location
// (libraryRoot, favorites, accent colours, lastContentFolder/lastIRFolder,
// darkTheme, MIDI Learn is session-state and NOT here). Message-thread only.
// Test-only: exclusively used by the test suite to keep tests from ever
// touching the user's real settings file. Setting this recreates the
// singleton immediately (flushing any pending real writes first) so the
// new location takes effect for every subsequent get/set call. Pass an
// invalid/empty File to restore the real per-user location. Never used by
// shipping product code paths.
void setSettingsFileOverride (const juce::File& file);

// The settings file currently in use (real per-user location, or the test
// override if one is set). Exposed for the hermetic-settings guard test.
juce::File getSettingsFile();

// Optional ownership stamp ("Licensed to name@example.com — Pro Edition"),
// shown in the editor footer when present. Purely informational — never
// gates anything (see EULA: no activation, no phone-home). Looked for as
// license.txt in the library root, then next to the presets folder.
juce::String getLicenseLine();
juce::String licenseLineFromFile (const juce::File&);   // pure parser (testable)

// --- Portable paths ---------------------------------------------------------
// Sample/wavetable paths inside the library are stored as "$LIB$/..." so
// presets and sessions survive the library living elsewhere on a customer's
// machine.
juce::String toPortable (const juce::File&, const juce::File& libraryRoot);
juce::File fromPortable (const juce::String&, const juce::File& libraryRoot);

} // namespace spa::library
