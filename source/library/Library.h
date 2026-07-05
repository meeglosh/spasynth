#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <vector>

namespace arsenal::library
{

// One pack folder = one preset category (the Silverplatter convention).
struct Pack
{
    juce::String name;
    juce::File folder;
    juce::Array<juce::File> wavs;   // sorted by file size, ascending
};

// Scans a library root: every direct subfolder containing WAVs is a pack.
std::vector<Pack> scanLibrary (const juce::File& root);

// --- Machine-level settings (library location, not per-session) -----------
juce::File getLibraryRoot();
void setLibraryRoot (const juce::File&);

// Quick structural check: does this folder look like an Arsenal library
// (at least one pack subfolder containing WAVs)?
bool looksLikeLibrary (const juce::File&);

// The standard install locations the content installers write to, most
// preferred first. The plugin probes these so users never have to point
// Arsenal at the library manually.
std::vector<juce::File> defaultLibraryLocations();

// Pure discovery over a candidate list (testable).
juce::File discoverLibrary (const std::vector<juce::File>& candidates);

// The library root Arsenal should use right now: the configured root if it
// is still valid, otherwise the first valid default location (which is then
// persisted). Returns an invalid File only if nothing is found — the manual
// "Set Library Folder..." fallback covers that case.
juce::File findLibraryRoot();

// Where presets live: <app data>/Silverplatter Audio/Arsenal/Presets with
// Factory/<Category>/ and User/ underneath.
juce::File defaultPresetsRoot();

// --- Portable paths ---------------------------------------------------------
// Sample/wavetable paths inside the library are stored as "$LIB$/..." so
// presets and sessions survive the library living elsewhere on a customer's
// machine.
juce::String toPortable (const juce::File&, const juce::File& libraryRoot);
juce::File fromPortable (const juce::String&, const juce::File& libraryRoot);

} // namespace arsenal::library
