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
