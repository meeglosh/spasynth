#include "PresetBrowser.h"
#include "AssignOverlay.h"   // free spa::ui::showPopupAnchored -- see its declaration comment
#include "../SPASynthProcessor.h"
#include "../library/Library.h"

namespace spa::ui
{

namespace
{
    // Chip labels and the Filter::type each one selects.
    constexpr const char* chipLabels[] = { "ALL", "KEYS", "TEXTURE", "PULSE", "USER" };
    constexpr const char* chipTypes[]  = { "",    "Keys", "Texture", "Pulse", "User" };
    constexpr int chipRadioGroup = 0x5751;

    // --- sound-type (TYPE dropdown) prefix table --------------------------
    // One line per displayed sound type -> the whole-word name prefix(es)
    // that select it (case-insensitive). Product-owner table, kept as ONE
    // small data table so a future addition/rename is a one-line edit.
    // Order here has no effect on anything user-visible (the dropdown lists
    // only the types actually present, alphabetically); it's kept roughly
    // alphabetical by display name for readability.
    struct SoundTypeEntry
    {
        const char* display;
        std::initializer_list<const char*> prefixes;
    };

    const SoundTypeEntry soundTypeTable[] = {
        { "Acid",            { "ACID" } },
        { "Ambience",        { "AMB" } },
        { "Audio Effect",    { "EFX" } },
        { "Bass",            { "BASS" } },
        { "Bell",            { "BELL" } },
        { "Brass",           { "BR" } },
        { "Choir",           { "VOX" } },
        { "Chord",           { "CH" } },
        { "Downlift",        { "DOWNER" } },
        { "Drum",            { "DRUM" } },
        { "Hit/Stab",        { "HIT" } },
        { "Hoover",          { "HV" } },
        { "Keys",            { "KEY", "KEYS" } },
        { "Lead",            { "LEAD" } },
        { "Mallet",          { "MAL" } },
        { "Midrange",        { "MID" } },
        { "Modulated Bass",  { "MDL" } },
        { "OneShot Bass",    { "OS" } },
        { "Organ",           { "OR" } },
        { "Pad",             { "PAD" } },
        { "Perc",            { "PERC" } },
        { "Pluck",           { "PLUCK" } },
        { "Rhythmic",        { "RHYTHMIC" } },
        { "Sawtooth",        { "SAW" } },
        { "Seq",             { "SEQ" } },
        { "SFX",             { "FX" } },
        { "Soundscape",      { "SC" } },
        { "String",          { "STR" } },
        { "Sweep",           { "SW" } },
        { "Synth",           { "SYN" } },
        { "Template",        { "INIT" } },
        { "Wobble",          { "WBL" } },
        { "Woodwind",        { "WW" } },
    };

    // First whole word of a preset name: everything up to the first space,
    // underscore or dash, or the whole name if none of those appear. This is
    // what whole-word prefix matching is measured against -- "SWEEP" and
    // "EFX" have no separator, so their first word is the full name, which
    // is not equal to the shorter prefixes "SW"/"FX" and correctly fails to
    // match them.
    juce::String firstWord (const juce::String& name)
    {
        const auto end = name.indexOfAnyOf (" _-", 0, false);
        return end < 0 ? name : name.substring (0, end);
    }

    // Looks the first word up against every table entry's prefix list,
    // case-insensitive, whole-word only (equalsIgnoreCase, not a substring
    // test). "" if nothing matches.
    juce::String prefixSoundType (const juce::String& name)
    {
        const auto word = firstWord (name);
        if (word.isEmpty())
            return {};

        for (const auto& entry : soundTypeTable)
            for (const auto* prefix : entry.prefixes)
                if (word.equalsIgnoreCase (prefix))
                    return entry.display;

        return {};
    }

    // Fallback for un-prefixed FACTORY presets only (Mike: current factory
    // presets have no prefix; their names end in the recipe's own trailing
    // word -- see PresetManager.cpp's Keys/Texture/Pulse generator). Never
    // applied to user presets, and never overrides an actual prefix match
    // (a factory preset whose pack name happens to start with a real prefix,
    // e.g. "Bass ... Keys", is intentionally left to the prefix table).
    juce::String factoryFallbackSoundType (const juce::String& name)
    {
        if (name.endsWithIgnoreCase (" Keys"))
            return "Keys";
        if (name.endsWithIgnoreCase (" Texture"))
            return "Soundscape";
        if (name.endsWithIgnoreCase (" Pulse"))
            return "Rhythmic";
        return {};
    }

    // The browser runs 2pt larger than the module grid (its LookAndFeel-drawn
    // controls get the same boost via the "browser"/"chip" componentIDs).
    juce::Font browserFont()
    {
        return metrics::labelFont().withHeight (metrics::labelFont().getHeight() + 2.0f);
    }

    juce::Font browserSmallFont()
    {
        return metrics::smallFont().withHeight (metrics::smallFont().getHeight() + 2.0f);
    }
}

juce::String PresetBrowser::typeOf (const library::PresetManager::PresetInfo& p)
{
    // Any user preset counts for the USER quick-filter chip, whether it
    // lives at the User/ root (category "User") or inside a bank subfolder
    // (category = the bank's name) -- isUser is the source of truth, not
    // the category string.
    if (p.isUser)
        return "User";

    for (const auto* t : { "Keys", "Texture", "Pulse" })
        if (p.name.endsWith (" " + juce::String (t)))
            return t;

    return {};
}

juce::String PresetBrowser::soundTypeOf (const library::PresetManager::PresetInfo& p)
{
    // A stored "type" attribute (1.0.26: set at save time or via "Set
    // type...") always wins over the name-prefix guess -- it's the user's
    // (or the save dialog's) explicit choice, including any custom type.
    if (p.storedType.isNotEmpty())
        return p.storedType;

    const auto prefixed = prefixSoundType (p.name);
    if (prefixed.isNotEmpty())
        return prefixed;

    // Fallback only for un-prefixed FACTORY presets (isUser == false); a
    // user preset with no matching prefix genuinely has no sound type.
    return p.isUser ? juce::String() : factoryFallbackSoundType (p.name);
}

juce::StringArray PresetBrowser::builtInSoundTypeNames()
{
    juce::StringArray names;
    for (const auto& entry : soundTypeTable)
        names.add (entry.display);
    return names;
}

juce::String PresetBrowser::favoriteKey (const library::PresetManager::PresetInfo& p)
{
    return p.category + "/" + p.name;
}

std::vector<int> PresetBrowser::filterIndices (
    const std::vector<library::PresetManager::PresetInfo>& presets,
    const Filter& filter, const juce::StringArray& favoriteKeys)
{
    std::vector<int> out;

    for (size_t i = 0; i < presets.size(); ++i)
    {
        const auto& p = presets[i];

        if (filter.type.isNotEmpty() && typeOf (p) != filter.type)
            continue;
        if (filter.category.isNotEmpty() && p.category != filter.category)
            continue;
        if (filter.soundType.isNotEmpty() && soundTypeOf (p) != filter.soundType)
            continue;
        if (filter.favoritesOnly && ! favoriteKeys.contains (favoriteKey (p)))
            continue;
        if (filter.search.isNotEmpty()
            && ! p.name.containsIgnoreCase (filter.search)
            && ! p.category.containsIgnoreCase (filter.search)
            && ! soundTypeOf (p).containsIgnoreCase (filter.search))
            continue;

        out.push_back ((int) i);
    }

    return out;
}

juce::StringArray PresetBrowser::availableSoundTypes (
    const std::vector<library::PresetManager::PresetInfo>& presets,
    const Filter& filterExcludingSoundType, const juce::StringArray& favoriteKeys)
{
    auto base = filterExcludingSoundType;
    base.soundType = {};   // ignore whatever the caller passed here -- this
                            // listing must never depend on its own filter

    juce::StringArray types;
    for (auto i : filterIndices (presets, base, favoriteKeys))
    {
        const auto t = soundTypeOf (presets[(size_t) i]);
        if (t.isNotEmpty())
            types.addIfNotAlreadyThere (t);
    }

    types.sort (true);   // alphabetical, case-insensitive
    return types;
}

PresetBrowser::PresetBrowser (SPASynthProcessor& p,
                              std::function<void()> close,
                              std::function<void()> chooseLibrary,
                              std::function<void()> requestKeyboardFocus)
    : processor (p), onClose (std::move (close)), onChooseLibrary (std::move (chooseLibrary)),
      onRequestKeyboardFocus (std::move (requestKeyboardFocus))
{
    setComponentID ("presetBrowser");
    setWantsKeyboardFocus (true);

    closeButton.setComponentID ("browser");
    closeButton.setTooltip ("Close the preset browser (Esc)");
    closeButton.onClick = [this] { if (onClose) onClose(); };
    closeButton.setMouseClickGrabsKeyboardFocus (false);   // see Controls.h's Knob
    addAndMakeVisible (closeButton);

    searchBox.setSelectAllWhenFocused (true);
    searchBox.setEscapeAndReturnKeysConsumed (false);   // Esc bubbles up = close
    searchBox.onTextChange = [this] { applyFilter(); };
    addAndMakeVisible (searchBox);

    for (size_t i = 0; i < typeChips.size(); ++i)
    {
        auto& chip = typeChips[i];
        chip.setButtonText (chipLabels[i]);
        chip.setComponentID ("chip");
        chip.setClickingTogglesState (true);
        chip.setRadioGroupId (chipRadioGroup);
        chip.onClick = [this] { applyFilter(); };
        addAndMakeVisible (chip);
    }
    typeChips[0].setToggleState (true, juce::dontSendNotification);

    categoryBox.setComponentID ("browser");
    categoryBox.setWantsKeyboardFocus (false);            // see Controls.h's Knob
    categoryBox.setMouseClickGrabsKeyboardFocus (false);  // the actual fix -- see Controls.h's Knob
    categoryBox.setTextWhenNothingSelected ("All Packs");
    categoryBox.onChange = [this] { applyFilter(); };
    addAndMakeVisible (categoryBox);

    soundTypeBox.setComponentID ("browser");
    soundTypeBox.setWantsKeyboardFocus (false);            // see Controls.h's Knob
    soundTypeBox.setMouseClickGrabsKeyboardFocus (false);  // the actual fix -- see Controls.h's Knob
    soundTypeBox.setTextWhenNothingSelected ("All Types");
    soundTypeBox.onChange = [this] { applyFilter(); };
    addAndMakeVisible (soundTypeBox);

    favoritesChip.setComponentID ("chip");
    favoritesChip.setClickingTogglesState (true);
    favoritesChip.setTooltip ("Show favorites only");
    favoritesChip.onClick = [this] { applyFilter(); };
    favoritesChip.setMouseClickGrabsKeyboardFocus (false);   // see Controls.h's Knob
    addAndMakeVisible (favoritesChip);

    list.setRowHeight (36);
    list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (list);

    countLabel.setFont (browserSmallFont());
    countLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (countLabel);

    libraryButton.setComponentID ("browser");
    libraryButton.setTooltip ("Point SPASynth at the Silverplatter library folder");
    libraryButton.onClick = [this] { if (onChooseLibrary) onChooseLibrary(); };
    libraryButton.setMouseClickGrabsKeyboardFocus (false);   // see Controls.h's Knob
    addAndMakeVisible (libraryButton);

    rescanButton.setComponentID ("browser");
    rescanButton.setTooltip ("Rescan now (the library also refreshes automatically "
                              "when packs are added)");
    rescanButton.onClick = [this] { processor.refreshLibrary(); };
    rescanButton.setMouseClickGrabsKeyboardFocus (false);   // see Controls.h's Knob
    addAndMakeVisible (rescanButton);

    importButton.setComponentID ("browser");
    importButton.setTooltip ("Import .spasynth files, a folder, or a .zip pack "
                              "(drag and drop onto this drawer works too)");
    importButton.onClick = [this] { showImportChooser(); };
    importButton.setMouseClickGrabsKeyboardFocus (false);   // see Controls.h's Knob
    addAndMakeVisible (importButton);

    // Blanket sweep for everything else in the drawer -- the ListBox (and its
    // internal viewport/rows/scrollbars), the type chips, and any other
    // click-grabbing default JUCE gives its widgets. searchBox is the one
    // deliberate exception: it needs to take focus on click so the user can
    // type. A listener on the row container is also needed below: rows are
    // created lazily while scrolling, after this sweep has already run.
    for (int i = 0; i < getNumChildComponents(); ++i)
        if (auto* child = getChildComponent (i); child != &searchBox)
            disableMouseClickFocusGrab (*child);

    if (auto* viewedContent = list.getViewport()->getViewedComponent())
        viewedContent->addComponentListener (this);   // rows created lazily on scroll

    processor.getPresetManager().addChangeListener (this);
    refresh();
}

PresetBrowser::~PresetBrowser()
{
    processor.getPresetManager().removeChangeListener (this);
}

void PresetBrowser::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refresh();
}

void PresetBrowser::componentChildrenChanged (juce::Component& c)
{
    // Re-sweep from the row container whenever it gains/loses children (new
    // ListBox rows scrolled into view). Cheap and idempotent -- does NOT
    // re-register listeners, so it can't re-enter the ListenerList that's
    // mid-callback for this very notification. disableMouseClickFocusGrab
    // is the shared helper in Controls.h.
    disableMouseClickFocusGrab (c);
}

void PresetBrowser::openImmediately()
{
    setVisible (true);
    if (! openBounds.isEmpty())
        setBounds (openBounds);
    toFront (false);
}

void PresetBrowser::refresh()
{
    const auto& t = currentTheme();

    presets = processor.getPresetManager().getPresets();
    favoriteKeys = library::getFavoritePresets();

    // Rebuild the pack combo, keeping the current pick when it still exists.
    const auto selectedCategory = categoryBox.getSelectedId() > 1 ? categoryBox.getText()
                                                                  : juce::String();
    categoryBox.clear (juce::dontSendNotification);
    categoryBox.addItem ("All Packs", 1);
    int itemID = 2;
    for (const auto& category : processor.getPresetManager().getCategories())
    {
        categoryBox.addItem (category, itemID);
        if (category == selectedCategory)
            categoryBox.setSelectedId (itemID, juce::dontSendNotification);
        ++itemID;
    }
    if (categoryBox.getSelectedId() == 0)
        categoryBox.setSelectedId (1, juce::dontSendNotification);

    searchBox.setFont (browserFont());
    searchBox.setColour (juce::TextEditor::backgroundColourId, t.display);
    searchBox.setColour (juce::TextEditor::textColourId, t.textPrimary);
    searchBox.setColour (juce::TextEditor::outlineColourId, t.outline);
    searchBox.setColour (juce::TextEditor::focusedOutlineColourId, t.accentMod);
    searchBox.setTextToShowWhenEmpty ("Search presets...", t.textSecondary);
    searchBox.applyColourToAllText (t.textPrimary);

    countLabel.setColour (juce::Label::textColourId, t.textSecondary);

    applyFilter();
}

void PresetBrowser::applyFilter()
{
    Filter filter;
    filter.search = searchBox.getText().trim();
    for (size_t i = 0; i < typeChips.size(); ++i)
        if (typeChips[i].getToggleState())
            filter.type = chipTypes[i];
    if (categoryBox.getSelectedId() > 1)
        filter.category = categoryBox.getText();
    filter.favoritesOnly = favoritesChip.getToggleState();

    // Rebuild the TYPE dropdown from presets matching every OTHER filter
    // (search/chip/pack/favourites, never its own current pick), keeping the
    // current selection if it's still offered, else falling back to "All
    // Types". Runs before the final filtered list below so soundType can be
    // read back off the (possibly just-rebuilt) combo.
    const auto selectedSoundType = soundTypeBox.getSelectedId() > 1 ? soundTypeBox.getText()
                                                                     : juce::String();
    const auto offeredTypes = availableSoundTypes (presets, filter, favoriteKeys);

    soundTypeBox.clear (juce::dontSendNotification);
    soundTypeBox.addItem ("All Types", 1);
    int soundTypeID = 2;
    for (const auto& t : offeredTypes)
    {
        soundTypeBox.addItem (t, soundTypeID);
        if (t == selectedSoundType)
            soundTypeBox.setSelectedId (soundTypeID, juce::dontSendNotification);
        ++soundTypeID;
    }
    if (soundTypeBox.getSelectedId() == 0)
        soundTypeBox.setSelectedId (1, juce::dontSendNotification);

    if (soundTypeBox.getSelectedId() > 1)
        filter.soundType = soundTypeBox.getText();

    filtered = filterIndices (presets, filter, favoriteKeys);

    countLabel.setText (juce::String (filtered.size()) + " of "
                            + juce::String (presets.size()) + " presets",
                        juce::dontSendNotification);

    list.updateContent();

    // Highlight (and reveal) the loaded preset when it survives the filter.
    const auto currentName = processor.getPresetManager().getCurrentName();
    int currentRow = -1;
    for (size_t row = 0; row < filtered.size(); ++row)
        if (presets[(size_t) filtered[row]].name == currentName)
            currentRow = (int) row;

    if (currentRow >= 0)
    {
        list.selectRow (currentRow);
        list.scrollToEnsureRowIsOnscreen (currentRow);
    }
    else
    {
        list.deselectAllRows();
    }

    repaint();
}

int PresetBrowser::getNumRows()
{
    return (int) filtered.size();
}

void PresetBrowser::paintListBoxItem (int row, juce::Graphics& g, int width, int height,
                                      bool rowIsSelected)
{
    if (row < 0 || row >= (int) filtered.size())
        return;

    const auto& t = currentTheme();
    const auto& p = presets[(size_t) filtered[(size_t) row]];
    auto r = juce::Rectangle<int> (0, 0, width, height);

    if (rowIsSelected)
    {
        g.setColour (t.accent.withAlpha (0.13f));
        g.fillRect (r);
        g.setColour (t.accent);
        g.fillRect (r.removeFromLeft (2));
    }

    const auto starZone = r.removeFromRight (30);
    const bool favorite = favoriteKeys.contains (favoriteKey (p));
    g.setColour (favorite ? t.accent : t.textSecondary.withAlpha (0.45f));
    g.setFont (juce::Font (juce::FontOptions (15.0f)));
    g.drawText (favorite ? juce::String::fromUTF8 ("\xe2\x98\x85")     // ★
                         : juce::String::fromUTF8 ("\xe2\x98\x86"),    // ☆
                starZone, juce::Justification::centred);

    auto text = r.reduced (8, 4);
    g.setColour (t.textPrimary);
    g.setFont (browserFont());
    g.drawText (p.name, text.removeFromTop (text.getHeight() / 2),
                juce::Justification::centredLeft, true);
    g.setColour (t.textSecondary);
    g.setFont (browserSmallFont());
    g.drawText (p.category, text, juce::Justification::centredLeft, true);

    g.setColour (t.outline.withAlpha (0.5f));
    g.fillRect (0, height - 1, width, 1);
}

void PresetBrowser::listBoxItemClicked (int row, const juce::MouseEvent& e)
{
    if (row < 0 || row >= (int) filtered.size())
        return;

    const auto& p = presets[(size_t) filtered[(size_t) row]];

    // Right-click opens the row context menu and does nothing else -- it
    // never loads the preset and never toggles the favourite, wherever in
    // the row it landed. (JUCE routes a right-click through
    // listBoxItemClicked exactly like a left-click; see ListBox's
    // performSelection.) The editor's global MIDI Learn right-click handler
    // sees this same click and bows out on it -- see the header comment.
    if (e.mods.isPopupMenu())
    {
        showRowMenu (row);
        return;
    }

    // Star zone toggles favourite instead of loading.
    if (e.getMouseDownX() > list.getWidth() - 34)
    {
        const auto key = favoriteKey (p);
        library::setPresetFavorite (key, ! favoriteKeys.contains (key));
        favoriteKeys = library::getFavoritePresets();

        if (favoritesChip.getToggleState())
            applyFilter();          // un-starring removes it from the list
        else
            list.repaintRow (row);
        return;
    }

    // searchBox is the one component in this drawer allowed to keep keyboard
    // focus on click (it needs it to type). If it had focus and the user
    // then picks a preset, hand focus to the on-screen keyboard (if visible)
    // so QWERTY note-play resumes instead of silently staying in the search
    // field -- everything else in the drawer no longer moves focus at all,
    // so without this the keyboard would only get focus back if the user
    // happened to click a virtual key.
    const bool searchHadFocus = searchBox.hasKeyboardFocus (false);

    processor.getPresetManager().loadPresetFile (p.file);

    if (searchHadFocus && onRequestKeyboardFocus)
        onRequestKeyboardFocus();
}

int PresetBrowser::findVisibleRow (const juce::String& presetName) const
{
    for (size_t row = 0; row < filtered.size(); ++row)
        if (presets[(size_t) filtered[row]].name == presetName)
            return (int) row;

    return -1;
}

bool PresetBrowser::canDeleteRow (int row) const
{
    if (row < 0 || row >= (int) filtered.size())
        return false;

    // isUser, not category == "User": a preset inside a bank subfolder
    // carries the bank's name as its category but is still the user's.
    return presets[(size_t) filtered[(size_t) row]].isUser;
}

juce::PopupMenu PresetBrowser::buildRowMenu (int row, juce::StringArray* typeMenuNamesOut) const
{
    juce::PopupMenu menu;

    if (row < 0 || row >= (int) filtered.size())
        return menu;

    const auto& p = presets[(size_t) filtered[(size_t) row]];
    const bool isUser = canDeleteRow (row);   // isUser, not category == "User" -- see canDeleteRow

    menu.addSectionHeader (p.name);
    menu.addItem (deleteMenuItemId, "Move to Trash", isUser);
    menu.addItem (exportPresetMenuItemId, "Export preset...");   // works for factory too, read-only

    // "Set type..." rewrites the file, so it's user-preset-only, same gate
    // as delete. Offered types = the built-in table + every custom type
    // currently in use, in that order; ids are dense from firstTypeMenuItemId
    // so a caller can recover the chosen name via typeMenuNamesOut.
    {
        juce::PopupMenu typeMenu;
        juce::StringArray names = builtInSoundTypeNames();
        names.addArray (processor.getPresetManager().customTypesInUse (builtInSoundTypeNames()));
        int id = firstTypeMenuItemId;
        for (const auto& name : names)
            typeMenu.addItem (id++, name, isUser);
        typeMenu.addSeparator();
        typeMenu.addItem (newTypeMenuItemId, "New type...", isUser);
        if (typeMenuNamesOut != nullptr)
            *typeMenuNamesOut = names;
        menu.addSubMenu ("Set type...", typeMenu, isUser);
    }

    // "Export bank..." has no natural home on the plain pack/bank ComboBox
    // (JUCE combo items don't carry a per-item context menu), so it lives
    // here instead, on any preset that belongs to a real user bank (not the
    // User/ root itself, which isn't a "bank").
    if (isUser && p.category != "User")
        menu.addItem (exportBankMenuItemId, "Export bank \"" + p.category + "\"...");

    return menu;
}

void PresetBrowser::showRowMenu (int row)
{
    juce::StringArray typeMenuNames;
    auto menu = buildRowMenu (row, &typeMenuNames);
    if (menu.getNumItems() == 0)
        return;

    // Never menu.showMenuAsync: a plain PopupMenu with nothing in the editor
    // holding real keyboard focus flashes and vanishes under a real AU/VST3
    // host. See ContentComponent::showPopupAnchored's declaration comment.
    juce::Component::SafePointer<PresetBrowser> safe (this);
    showPopupAnchored (*this, menu, juce::PopupMenu::Options().withMousePosition(),
                       [safe, row, typeMenuNames] (int result)
                       {
                           if (safe == nullptr || result == 0)
                               return;

                           if (result == deleteMenuItemId)
                               safe->deleteRow (row);
                           else if (result == exportPresetMenuItemId)
                               safe->exportPresetRow (row);
                           else if (result == exportBankMenuItemId)
                               safe->exportBankRow (row);
                           else if (result == newTypeMenuItemId)
                               safe->promptNewTypeForRow (row);
                           else if (result >= firstTypeMenuItemId
                                    && result < firstTypeMenuItemId + typeMenuNames.size())
                               safe->applyTypeToRow (row, typeMenuNames[result - firstTypeMenuItemId]);
                       });
}

bool PresetBrowser::deleteRow (int row)
{
    if (! canDeleteRow (row))
        return false;

    // Copy what's needed out first -- refresh() below rebuilds `presets`,
    // so any reference into it dies partway through this function.
    const auto& p = presets[(size_t) filtered[(size_t) row]];
    const auto file = p.file;
    const auto key = favoriteKey (p);

    if (! processor.getPresetManager().deleteUserPreset (file))
        return false;

    // The favourite key is category + "/" + name, not a path, so leaving it
    // behind would linger in the settings file forever AND silently re-apply
    // itself to any later preset saved with the same name in the same bank.
    library::setPresetFavorite (key, false);

    // The manager's rescan broadcast is async; refresh now so the row is gone
    // immediately, with the search text, type chip, pack pick and favourites
    // chip all re-applied by applyFilter().
    refresh();
    return true;
}

namespace
{
    // Non-blocking heads-up for a warning that doesn't need a decision --
    // used for the "won't travel" export warning and the "needs library
    // packs" import summary. Shown with SafePointer so a fast-closing test
    // editor never leaves a dangling callback.
    void showInfo (juce::Component* parent, const juce::String& title, const juce::String& message)
    {
        if (message.isEmpty())
            return;
        juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
                                                     title, message, parent);
    }
}

void PresetBrowser::exportPresetRow (int row)
{
    if (row < 0 || row >= (int) filtered.size())
        return;

    const auto& p = presets[(size_t) filtered[(size_t) row]];

    fileChooser = std::make_unique<juce::FileChooser> (
        "Export preset", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                              .getChildFile (p.name + library::PresetManager::presetExtension),
        "*" + juce::String (library::PresetManager::presetExtension));

    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode
                             | juce::FileBrowserComponent::canSelectFiles,
                              [this, file = p.file] (const juce::FileChooser& fc)
    {
        const auto dest = fc.getResult();
        if (dest == juce::File())
            return;

        const auto result = processor.getPresetManager().exportPreset (file, dest);
        if (! result.ok)
        {
            showInfo (this, "Export Failed", "The preset could not be written to \""
                                              + dest.getFullPathName() + "\".");
            return;
        }
        if (! result.nonPortable.isEmpty())
            showInfo (this, "Some Content Won't Travel",
                      "This preset references sound content outside your library folder, "
                      "so it will not travel with the exported file:\n\n"
                          + result.nonPortable.joinIntoString ("\n"));
    });
}

void PresetBrowser::exportBankRow (int row)
{
    if (row < 0 || row >= (int) filtered.size())
        return;

    const auto bankName = presets[(size_t) filtered[(size_t) row]].category;

    fileChooser = std::make_unique<juce::FileChooser> (
        "Export bank", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                            .getChildFile (bankName + ".zip"),
        "*.zip");

    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode
                             | juce::FileBrowserComponent::canSelectFiles,
                              [this, bankName] (const juce::FileChooser& fc)
    {
        const auto dest = fc.getResult();
        if (dest == juce::File())
            return;

        const auto result = processor.getPresetManager().exportBank (bankName, dest);
        if (! result.ok)
        {
            showInfo (this, "Export Failed", "The bank could not be written to \""
                                              + dest.getFullPathName() + "\".");
            return;
        }
        if (! result.nonPortable.isEmpty())
            showInfo (this, "Some Content Won't Travel",
                      "This bank references sound content outside your library folder, "
                      "so it will not travel with the exported zip:\n\n"
                          + result.nonPortable.joinIntoString ("\n"));
    });
}

void PresetBrowser::promptNewTypeForRow (int row)
{
    if (row < 0 || row >= (int) filtered.size())
        return;
    const auto file = presets[(size_t) filtered[(size_t) row]].file;

    // A synchronous (foreground, user-initiated) text-input prompt -- this
    // drawer has no first-class "new type" input control of its own, and a
    // one-line name is all this ever needs.
    auto* aw = new juce::AlertWindow ("New Type", "Enter a name for the new sound type:",
                                      juce::MessageBoxIconType::NoIcon, this);
    aw->addTextEditor ("name", "");
    aw->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    aw->enterModalState (true,
        juce::ModalCallbackFunction::create ([safe = juce::Component::SafePointer<PresetBrowser> (this),
                                              aw, file] (int result)
        {
            const auto name = aw->getTextEditorContents ("name").trim();
            std::unique_ptr<juce::AlertWindow> owner (aw);   // dismissed either way
            if (result == 1 && name.isNotEmpty() && safe != nullptr)
                safe->processor.getPresetManager().setPresetType (file, name);
        }),
        false);
}

void PresetBrowser::applyTypeToRow (int row, const juce::String& newType)
{
    if (row < 0 || row >= (int) filtered.size())
        return;
    const auto file = presets[(size_t) filtered[(size_t) row]].file;
    processor.getPresetManager().setPresetType (file, newType);
}

void PresetBrowser::showImportChooser()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Import presets (.spasynth files, a folder, or a .zip pack)",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory));

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                             | juce::FileBrowserComponent::canSelectFiles
                             | juce::FileBrowserComponent::canSelectDirectories
                             | juce::FileBrowserComponent::canSelectMultipleItems,
                              [this] (const juce::FileChooser& fc)
    {
        importFromPaths (fc.getResults());
    });
}

bool PresetBrowser::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files)
    {
        const juce::File file (f);
        if (file.isDirectory()
            || file.hasFileExtension ("spasynth")
            || file.hasFileExtension ("zip"))
            return true;
    }
    return false;
}

void PresetBrowser::filesDropped (const juce::StringArray& files, int, int)
{
    draggingOver = false;
    repaint();

    juce::Array<juce::File> paths;
    for (const auto& f : files)
        paths.add (juce::File (f));
    importFromPaths (paths);
}

void PresetBrowser::importFromPaths (const juce::Array<juce::File>& paths)
{
    if (paths.isEmpty())
        return;

    // Fully async: the session is flattened up front (no writes yet), then
    // driven one file at a time by continueImport(). A blocking modal loop
    // here would be unsafe inside a plugin editor (see CLAUDE.md's
    // callOutParent/SafePointer rules) -- Logic's AUHostingService and
    // similar hosts don't tolerate the message thread stalling on a nested
    // event loop mid-render. The shared_ptr lets the session outlive this
    // browser (and even this editor) if the user closes either mid-import;
    // PresetManager (owned by the processor, which outlives the browser)
    // does the actual writes, so nothing is left half-applied.
    std::shared_ptr<library::PresetManager::ImportSession> session (
        processor.getPresetManager().beginImport (paths).release());
    continueImport (session);
}

void PresetBrowser::continueImport (std::shared_ptr<library::PresetManager::ImportSession> session)
{
    using Clash = library::PresetManager::ImportClash;

    const auto step = session->advance();

    if (step.finished)
    {
        const auto result = session->finish();

        juce::StringArray summary;
        if (! result.malformed.isEmpty())
            summary.add ("Skipped (could not be read as a preset):\n" + result.malformed.joinIntoString ("\n"));
        if (! result.rejectedZipSlip.isEmpty())
            summary.add ("Rejected (an entry tried to write outside its bank folder):\n"
                         + result.rejectedZipSlip.joinIntoString ("\n"));
        if (! result.needsLibraryPacks.isEmpty())
            summary.add ("Needs library packs you don't have (the sample content is missing):\n"
                         + result.needsLibraryPacks.joinIntoString ("\n"));

        if (result.imported > 0 || ! summary.isEmpty())
            showInfo (this, "Import Finished",
                      juce::String (result.imported) + " preset(s) imported."
                          + (summary.isEmpty() ? juce::String() : ("\n\n" + summary.joinIntoString ("\n\n"))));
        return;
    }

    // step.awaitingDecision: ask, then resume via decide() + another
    // advance() -- one decision per clashing name, with a "apply to this
    // whole import" toggle so the common case (several presets from the
    // same source) needs one click. Same button layout/order as the old
    // synchronous prompt, now via enterModalState instead of runModalLoop.
    auto* aw = new juce::AlertWindow ("Preset Already Exists",
                                      "\"" + step.clashName + "\" already exists in the destination.",
                                      juce::MessageBoxIconType::QuestionIcon, this);
    auto applyToAll = std::make_shared<juce::ToggleButton> ("Do this for every clash in this import");
    applyToAll->setSize (280, 22);
    aw->addCustomComponent (applyToAll.get());
    aw->addButton ("Replace", (int) Clash::replace + 1);
    aw->addButton ("Keep Both", (int) Clash::keepBoth + 1);
    aw->addButton ("Skip", (int) Clash::skip + 1);

    aw->enterModalState (true,
        juce::ModalCallbackFunction::create ([safe = juce::Component::SafePointer<PresetBrowser> (this),
                                              aw, applyToAll, session] (int pressed)
        {
            std::unique_ptr<juce::AlertWindow> owner (aw);   // dismissed either way
            const auto action = static_cast<Clash> (juce::jlimit (0, 2, pressed - 1));
            session->decide (action, applyToAll->getToggleState());

            // If the browser (or its editor) was closed while this prompt
            // was up, drop the session here rather than resuming: the files
            // already written stay written (PresetManager did that, and it
            // outlives us), but nothing further touches a freed `this`.
            if (safe != nullptr)
                safe->continueImport (session);
        }),
        false);
}

bool PresetBrowser::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        if (onClose)
            onClose();
        return true;
    }

    return false;
}

void PresetBrowser::paint (juce::Graphics& g)
{
    const auto& t = currentTheme();
    auto bounds = getLocalBounds();
    const auto shadow = bounds.removeFromRight (shadowWidth);

    // Cast shadow onto the modules behind — sells the "drawer over UI" read.
    g.setGradientFill (juce::ColourGradient (
        juce::Colours::black.withAlpha (0.35f), (float) shadow.getX(), 0.0f,
        juce::Colours::transparentBlack, (float) shadow.getRight(), 0.0f, false));
    g.fillRect (shadow);

    g.setColour (t.panel);
    g.fillRect (bounds);
    g.setColour (t.outline);
    g.drawVerticalLine (bounds.getRight() - 1, 0.0f, (float) getHeight());

    g.setColour (t.textSecondary);
    g.setFont (metrics::sectionFont().withHeight (metrics::sectionFont().getHeight() + 2.0f));
    g.drawText ("PRESETS", titleArea, juce::Justification::centredLeft);

    draw::displayWell (g, listWell.toFloat().expanded (2.0f), false);

    // Drop target highlight while a file/folder/zip drag hovers the drawer.
    if (draggingOver)
    {
        g.setColour (t.accent.withAlpha (0.5f));
        g.drawRect (getLocalBounds().reduced (1), 2);
    }
}

void PresetBrowser::resized()
{
    auto bounds = getLocalBounds();
    bounds.removeFromRight (shadowWidth);
    bounds.reduce (10, 10);

    auto header = bounds.removeFromTop (22);
    closeButton.setBounds (header.removeFromRight (22));
    titleArea = header;

    bounds.removeFromTop (8);
    searchBox.setBounds (bounds.removeFromTop (26));

    bounds.removeFromTop (6);
    auto chipRow = bounds.removeFromTop (20);
    const auto chipWidth = (chipRow.getWidth() - 3 * (int) typeChips.size() + 3)
                               / (int) typeChips.size();
    for (auto& chip : typeChips)
    {
        chip.setBounds (chipRow.removeFromLeft (chipWidth));
        chipRow.removeFromLeft (3);
    }

    bounds.removeFromTop (6);
    auto packRow = bounds.removeFromTop (24);
    favoritesChip.setBounds (packRow.removeFromRight (28));
    packRow.removeFromRight (4);
    // Pack/bank and sound-TYPE dropdowns split the remaining width evenly --
    // both filter axes are equally likely to hold the longer text (a pack
    // name vs. a sound-type name like "Modulated Bass").
    soundTypeBox.setBounds (packRow.removeFromRight ((packRow.getWidth() - 4) / 2));
    packRow.removeFromRight (4);
    categoryBox.setBounds (packRow);

    auto footer = bounds.removeFromBottom (24);
    rescanButton.setBounds (footer.removeFromRight (64));
    footer.removeFromRight (4);
    libraryButton.setBounds (footer.removeFromRight (96));
    footer.removeFromRight (4);
    importButton.setBounds (footer.removeFromRight (64));
    countLabel.setBounds (footer);

    bounds.removeFromBottom (8);
    bounds.removeFromTop (8);
    listWell = bounds;
    list.setBounds (bounds.reduced (1));
}

} // namespace spa::ui
