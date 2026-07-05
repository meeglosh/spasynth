#include "Library.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace arsenal::library
{

namespace
{
    constexpr const char* portablePrefix = "$LIB$";

    std::unique_ptr<juce::PropertiesFile> openSettings()
    {
        juce::PropertiesFile::Options options;
        options.applicationName = "Arsenal";
        options.folderName = "Silverplatter Audio/Arsenal";
        options.filenameSuffix = "settings";
        options.osxLibrarySubFolder = "Application Support";
        return std::make_unique<juce::PropertiesFile> (options);
    }
}

std::vector<Pack> scanLibrary (const juce::File& root)
{
    std::vector<Pack> packs;

    if (! root.isDirectory())
        return packs;

    for (const auto& folder : root.findChildFiles (juce::File::findDirectories, false))
    {
        Pack pack;
        pack.name = folder.getFileName();
        pack.folder = folder;
        pack.wavs = folder.findChildFiles (juce::File::findFiles, true, "*.wav;*.WAV");

        if (pack.wavs.isEmpty())
            continue;

        std::sort (pack.wavs.begin(), pack.wavs.end(),
                   [] (const juce::File& a, const juce::File& b)
                   { return a.getSize() < b.getSize(); });

        packs.push_back (std::move (pack));
    }

    std::sort (packs.begin(), packs.end(),
               [] (const Pack& a, const Pack& b)
               { return a.name.compareIgnoreCase (b.name) < 0; });

    return packs;
}

juce::File getLibraryRoot()
{
    const auto path = openSettings()->getValue ("libraryRoot");
    return path.isNotEmpty() ? juce::File (path) : juce::File();
}

bool looksLikeLibrary (const juce::File& root)
{
    if (! root.isDirectory())
        return false;

    for (const auto& folder : root.findChildFiles (juce::File::findDirectories, false))
        if (! folder.findChildFiles (juce::File::findFiles, false, "*.wav;*.WAV").isEmpty())
            return true;

    return false;
}

std::vector<juce::File> defaultLibraryLocations()
{
    const juce::String company = "Silverplatter Audio";
    const juce::String libraryName = "Arsenal Library";

    std::vector<juce::File> candidates;

    // Shared, user-writable, visible across accounts — the primary install
    // target (/Users/Shared on macOS, Public Documents on Windows).
    candidates.push_back (juce::File::getSpecialLocation (juce::File::commonDocumentsDirectory)
                              .getChildFile (company).getChildFile (libraryName));

    // System-wide application data.
   #if JUCE_MAC
    candidates.push_back (juce::File ("/Library/Application Support")
                              .getChildFile (company).getChildFile (libraryName));
   #else
    candidates.push_back (juce::File::getSpecialLocation (juce::File::commonApplicationDataDirectory)
                              .getChildFile (company).getChildFile (libraryName));
   #endif

    // Per-user application data.
    candidates.push_back (juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                             #if JUCE_MAC
                              .getChildFile ("Application Support")
                             #endif
                              .getChildFile (company).getChildFile (libraryName));

    // User documents (people drag content there more often than anywhere).
    candidates.push_back (juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                              .getChildFile (company).getChildFile (libraryName));

    return candidates;
}

juce::File discoverLibrary (const std::vector<juce::File>& candidates)
{
    for (const auto& candidate : candidates)
        if (looksLikeLibrary (candidate))
            return candidate;

    return {};
}

juce::File findLibraryRoot()
{
    const auto configured = getLibraryRoot();
    if (looksLikeLibrary (configured))
        return configured;

    const auto discovered = discoverLibrary (defaultLibraryLocations());
    if (discovered.isDirectory())
        setLibraryRoot (discovered);

    return discovered;
}

void setLibraryRoot (const juce::File& root)
{
    auto settings = openSettings();
    settings->setValue ("libraryRoot", root.getFullPathName());
    settings->saveIfNeeded();
}

juce::File defaultPresetsRoot()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("Silverplatter Audio").getChildFile ("Arsenal")
        .getChildFile ("Presets");
}

juce::String toPortable (const juce::File& f, const juce::File& libraryRoot)
{
    if (libraryRoot.isDirectory() && f.isAChildOf (libraryRoot))
        return portablePrefix + f.getRelativePathFrom (libraryRoot)
                                  .replaceCharacter ('\\', '/');

    return f.getFullPathName();
}

juce::File fromPortable (const juce::String& s, const juce::File& libraryRoot)
{
    if (s.startsWith (portablePrefix))
        return libraryRoot.getChildFile (s.fromFirstOccurrenceOf (portablePrefix, false, false));

    return juce::File (s);
}

} // namespace arsenal::library
