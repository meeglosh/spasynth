#pragma once

#include "Theme.h"
#include "Controls.h"
#include "../library/PresetManager.h"

namespace spa
{

class SPASynthProcessor;

namespace ui
{

// UAD-style preset drawer: slides in from the left over the module grid.
// Live search, type chips (factory presets come in Keys/Texture/Pulse
// flavours, plus User), a pack filter, and persisted favourites. Clicking a
// row loads it; the star toggles favourite. Esc or the X closes the drawer.
class PresetBrowser : public juce::Component,
                      private juce::ChangeListener,
                      private juce::ListBoxModel,
                      private juce::ComponentListener,
                      public juce::FileDragAndDropTarget
{
public:
    // onRequestKeyboardFocus: called after a preset row click loads a preset
    // if the search box had keyboard focus at click time, so the caller can
    // hand focus to the on-screen keyboard (if visible) and resume QWERTY
    // note-play -- see listBoxItemClicked.
    PresetBrowser (SPASynthProcessor&,
                   std::function<void()> onClose,
                   std::function<void()> onChooseLibrary,
                   std::function<void()> onRequestKeyboardFocus);
    ~PresetBrowser() override;

    // --- pure filtering (testable without a UI) ------------------------------
    struct Filter
    {
        juce::String search;        // case-insensitive substring of name/pack/sound type
        juce::String type;          // "", "Keys", "Texture", "Pulse", "User" -- the
                                     // existing quick-filter chips (pack/bank flavour)
        juce::String category;      // "" = all packs
        bool favoritesOnly = false;
        // Kept LAST (not alongside the other String fields above) so every
        // existing positional Filter{...} test/call site -- written before
        // this field existed -- keeps compiling and keeps its meaning.
        juce::String soundType;     // "" = all sound types -- see soundTypeOf(); a
                                     // SECOND, independent axis from `type` above
    };

    static juce::String typeOf (const library::PresetManager::PresetInfo&);

    // Sound-type category (Bass/Pad/Lead/...) derived from the first whole
    // word of the preset's name against a small prefix table (see the .cpp),
    // with a fallback for un-prefixed factory presets ("<Pack> Keys/Texture/
    // Pulse" -> Keys/Soundscape/Rhythmic). "" if nothing matches (unmatched
    // presets have no sound type and only show under "All types").
    static juce::String soundTypeOf (const library::PresetManager::PresetInfo&);

    static juce::String favoriteKey (const library::PresetManager::PresetInfo&);

    // The sound-type dropdown's built-in table, display names only, in
    // table order -- exposed so the save dialog's TYPE combo (SPASynthEditor.cpp)
    // and this drawer's own "Set type..." submenu build from the same one list.
    static juce::StringArray builtInSoundTypeNames();
    static std::vector<int> filterIndices (
        const std::vector<library::PresetManager::PresetInfo>&,
        const Filter&, const juce::StringArray& favoriteKeys);

    // The sound types actually present among presets matching every filter
    // field EXCEPT soundType (its own value, if any, is ignored) -- what the
    // TYPE dropdown should offer, alphabetical, never including "".
    static juce::StringArray availableSoundTypes (
        const std::vector<library::PresetManager::PresetInfo>&,
        const Filter& filterExcludingSoundType, const juce::StringArray& favoriteKeys);

    // The parent stores the drawer's on-screen bounds every resized(); the
    // open/close animation slides between these and off-screen left.
    void setOpenBounds (juce::Rectangle<int> b) { openBounds = b; }
    juce::Rectangle<int> getOpenBounds() const { return openBounds; }
    void openImmediately();   // no animation — snapshot tests front the drawer

    void refresh();   // re-pull presets, categories, theme colours

    // --- row context menu (right-click) --------------------------------------
    // Right-clicking a row opens a one-item "Move to Trash" menu. The item is
    // present but DISABLED on a factory row, so a factory preset can never be
    // removed while the right-click still gives visible feedback.
    //
    // The global right-click MIDI Learn handler (ContentComponent::mouseDown,
    // installed with addMouseListener(this, true) over the whole editor) and
    // this menu cannot collide: that handler walks up from the clicked
    // component looking for a "paramID" component property and returns
    // without showing anything when it finds none, and nothing in this drawer
    // carries one. So a right-click on a row reaches both, and only this one
    // puts up a menu.
    static constexpr int deleteMenuItemId = 1;
    static constexpr int exportPresetMenuItemId = 2;
    static constexpr int exportBankMenuItemId = 3;
    static constexpr int newTypeMenuItemId = 4;
    static constexpr int firstTypeMenuItemId = 1000;   // one id per offered type, see buildRowMenu

    bool canDeleteRow (int row) const;            // false for factory/out-of-range rows

    // typeMenuNamesOut, if given, is filled in the SAME order the "Set
    // type..." submenu's items were added, so a caller holding onto a
    // selected id (>= firstTypeMenuItemId) can look up
    // typeMenuNamesOut[id - firstTypeMenuItemId] afterwards. Kept as an out
    // parameter (not a mutable member) so this stays a pure function --
    // also the test surface for the item's state.
    juce::PopupMenu buildRowMenu (int row, juce::StringArray* typeMenuNamesOut = nullptr) const;
    void showRowMenu (int row);                    // goes through showPopupAnchored -- never showMenuAsync
    bool deleteRow (int row);                      // the menu action; trashes + cleans the favourite

    // --- export / import (1.0.26) --------------------------------------------
    void exportPresetRow (int row);
    void exportBankRow (int row);
    void promptNewTypeForRow (int row);
    void applyTypeToRow (int row, const juce::String& newType);

    void showImportChooser();
    void importFromPaths (const juce::Array<juce::File>& paths);

    // Drives one step of an async import session (see
    // PresetManager::ImportSession): advances until finished (shows the
    // summary) or a clash needs an async decision (shows the prompt, then
    // resumes via decide() + another continueImport() from the callback).
    // Public so tests can drive/inspect the flow without going through the
    // UI's file chooser or drag-and-drop.
    void continueImport (std::shared_ptr<library::PresetManager::ImportSession> session);

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { draggingOver = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { draggingOver = false; repaint(); }

    // Test helpers: the filtered (visible) row list.
    int getNumVisibleRows() const { return (int) filtered.size(); }
    int findVisibleRow (const juce::String& presetName) const;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void applyFilter();

    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int width, int height,
                           bool rowIsSelected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;

    // ListBox rows are created lazily while scrolling -- new RowComponents
    // introduce fresh children after construction that would otherwise
    // revert to JUCE's click-grabs-focus default. Re-sweep whenever the
    // watched row container's children change. (ComboBox's internal Label
    // rebuild -- the other source of fresh post-construction children --
    // is handled once, application-wide, by SPASynthLookAndFeel::
    // createComboBoxTextBox instead of a listener here.)
    void componentChildrenChanged (juce::Component&) override;

    SPASynthProcessor& processor;
    std::function<void()> onClose, onChooseLibrary, onRequestKeyboardFocus;

    juce::TextButton closeButton { juce::String::fromUTF8 ("\xc3\x97") };   // ×
    juce::TextEditor searchBox;
    std::array<juce::TextButton, 5> typeChips;
    juce::ComboBox categoryBox;
    juce::ComboBox soundTypeBox;   // the new TYPE (sound-category) filter
    juce::TextButton favoritesChip { juce::String::fromUTF8 ("\xe2\x98\x85") };  // ★
    juce::ListBox list { {}, this };
    juce::Label countLabel;
    juce::TextButton libraryButton { "SET LIBRARY..." }, rescanButton { "RESCAN" };
    juce::TextButton importButton { "IMPORT..." };
    std::unique_ptr<juce::FileChooser> fileChooser;   // import picker + the two export save dialogs
    bool draggingOver = false;   // paints a highlighted drop target while a drag hovers

    std::vector<library::PresetManager::PresetInfo> presets;   // snapshot
    std::vector<int> filtered;                                 // indices into presets
    juce::StringArray favoriteKeys;                            // cached settings
    juce::Rectangle<int> openBounds, titleArea, listWell;

    static constexpr int shadowWidth = 10;   // cast onto the content behind

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetBrowser)
};

} // namespace ui
} // namespace spa
