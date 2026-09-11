#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "../params/ParameterRegistry.h"
#include "../params/Randomizer.h"
#include "SPASynthLookAndFeel.h"
#include "DraggableTabs.h"
#include "ModulePanels.h"
#include "SectionPanel.h"
#include "MatrixPanel.h"
#include "PresetBrowser.h"
#include "AssignOverlay.h"

namespace spa
{

class SPASynthProcessor;

namespace ui
{

// Everything inside the plugin window at base size; the editor shell scales
// this whole component for resizing.
class ContentComponent : public juce::Component,
                         private juce::ChangeListener,
                         private juce::Timer
{
public:
    ContentComponent (SPASynthProcessor&, std::function<void()> onThemeChanged);
    ~ContentComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;   // right-click = MIDI Learn
    bool keyPressed (const juce::KeyPress&) override;    // Esc closes the preset browser, Z/X
                                                          // shift the QWERTY octave -- both work
                                                          // even when focus stayed on the
                                                          // on-screen keyboard (see .cpp)
    void refreshAll();

    // Base height grows by the keyboard strip when it is shown; the editor
    // shell reads this to drive the window aspect ratio and scale.
    int getContentBaseHeight() const;
    // Base width grows by the preset drawer's column when it is open AND the
    // host let us actually widen the window (see browserOverlays below); the
    // editor shell reads this the same way it reads getContentBaseHeight().
    int getContentBaseWidth() const;
    std::function<void()> onKeyboardToggled;   // shell re-sizes when this fires
    std::function<void()> onBrowserToggled;    // shell re-sizes (width) when this fires

    // The shell calls this if it asked the host to widen the window for a
    // newly opened drawer and the host didn't actually honor it (fixed-size
    // host view): switches to the old overlay-over-the-grid behaviour and
    // shrinks our own base size back down to match (see resized()).
    // Sticky for the life of this editor -- once a host has shown it can't
    // resize us, later opens don't try again.
    void setBrowserOverlayMode (bool shouldOverlay);
    bool isBrowserOverlayMode() const { return browserOverlays; }

    // Every juce::PopupMenu (context menus, right-click MIDI Learn, the
    // settings menu, Convolve's library browser, ...) must go through this
    // instead of calling menu.showMenuAsync directly. Reason, traced through
    // JUCE source the same way the VOICE call-out bug was (see VoicePanel's
    // ctor comment in the .cpp): a plain PopupMenu, unlike CallOutBox, never
    // calls enterModalState/grabKeyboardFocus on anything of its own, so if
    // nothing in the editor already holds real JUCE keyboard focus when it
    // opens (true everywhere here since the whole-tree QWERTY sweep turns
    // setMouseClickGrabsKeyboardFocus off on every clickable widget,
    // including the button that opened the menu), the menu's own dismiss-
    // on-focus-loss safety net (doesAnyJuceCompHaveFocus) falls through to a
    // racy native per-peer key-window check instead of the normal "the
    // clicked control already holds focus" case -- under a real AU/VST3
    // host (not the standalone) that races and the menu dismisses itself a
    // frame or two after opening (Mike: "flashes on screen for a split
    // second then goes away"). Fix: grab real focus onto something in the
    // editor BEFORE showing the menu -- the on-screen keyboard if it's
    // visible, otherwise a dedicated always-focusable invisible anchor
    // (popupFocusAnchor) that exists solely for this -- then hand focus
    // back to the keyboard afterwards, exactly like the VOICE call-out's
    // onDismissedCallback does.
    void showPopupAnchored (juce::PopupMenu& menu, const juce::PopupMenu::Options& options,
                            std::function<void (int)> callback);

    // The right-click MIDI Learn menu's callback body (1 = arm, 2 = remove
    // assignment, 3 = cancel), exposed so tests can drive the exact wiring
    // mouseDown() hooks up to the real PopupMenu, rather than reaching past
    // it into MidiLearnManager directly.
    void applyMidiLearnMenuResult (int result, const juce::String& paramID);

    // Test-only accessors for the MIDI Learn diagnostics badge: pollNow()
    // runs the same logic as the 10 Hz timerCallback synchronously (so a
    // test doesn't have to race the real timer), getText() reads what it
    // currently shows, getBounds()/getFont() are for the fits-inside-the-
    // band assertion, and the threshold setter lets a test collapse the
    // real 3s no-CC wait to something instant.
    void pollMidiLearnBadgeNow() { timerCallback(); }
    juce::String getMidiLearnBadgeText() const { return midiLearnBadge.getText(); }
    juce::Rectangle<int> getMidiLearnBadgeBounds() const { return midiLearnBadge.getBounds(); }
    juce::Font getMidiLearnBadgeFont() const { return midiLearnBadge.getFont(); }
    void setMidiLearnBadgeNoCcHintThresholdMsForTest (juce::uint32 ms) { midiLearnBadgeNoCcHintThresholdMs = ms; }

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;   // MIDI Learn badge diagnostics, see .cpp
    void togglePresetBrowser();
    void showAccentPicker();
    juce::Component* callOutParent();
    void showSettingsMenu();
    void setKeyboardVisible (bool shouldShow);
    void shiftKeyboardOctave (int delta);
    void chooseLibraryFolder();
    void rescanLibrary();
    void saveUserPreset();

    // Header button showing the two accents as a split circle; clicking
    // drops down the colour picker.
    struct AccentButton : juce::Button
    {
        AccentButton() : juce::Button ("accentColors") {}
        void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    };

    // The top-left logo doubles as the settings menu button; this overlay adds
    // a hover highlight and opens the menu (the logo itself is painted behind).
    struct SettingsButton : juce::Button
    {
        SettingsButton() : juce::Button ("settings") {}
        void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    };

    // Bottom-right keyboard-strip toggle (Kontakt-style); draws a small piano
    // icon that lights up in the accent colour while the keyboard is shown.
    struct KeyboardButton : juce::Button
    {
        KeyboardButton() : juce::Button ("keyboard") {}
        void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    };

    // Header panic button: an alert badge, muted red by default and bright red
    // on hover, so it reads as "the emergency stop".
    struct PanicButton : juce::Button
    {
        PanicButton() : juce::Button ("panic") {}
        void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    };

    SPASynthProcessor& processor;
    std::function<void()> onThemeChanged;   // LnF palette refresh + repaint

    std::unique_ptr<juce::Drawable> logoDark, logoLight;
    SettingsButton settingsButton;   // over the top-left logo

    // Octave shift buttons for computer-keyboard (QWERTY) playing, at the
    // strip's left edge (Z/X are the keyboard-shortcut equivalent -- see
    // ContentComponent::keyPressed). MidiKeyboardComponent has no getter for
    // its own key-mapping octave (setKeyPressBaseOctave has no counterpart),
    // so keyboardOctave (below) is our own record of it.
    struct OctaveButton : juce::Button
    {
        explicit OctaveButton (bool isUp) : juce::Button (isUp ? "octaveUp" : "octaveDown"), up (isUp) {}
        void paintButton (juce::Graphics&, bool highlighted, bool down) override;
        bool up;
    };

    // On-screen keyboard strip (toggled from the settings menu or the
    // bottom-right keyboard button).
    juce::MidiKeyboardComponent keyboard;
    KeyboardButton keyboardButton;
    bool keyboardVisible = false;
    OctaveButton octaveDownButton { false }, octaveUpButton { true };
    juce::Label octaveLabel;   // range readout between the two buttons, e.g. "C2–C4"
    // Non-interactive overlay (a child of `keyboard` itself, so its
    // coordinates line up for free) that tints the two mapped octaves' white
    // keys so it's visible at a glance which keys QWERTY plays.
    struct OctaveHighlight : juce::Component
    {
        juce::MidiKeyboardComponent* keyboard = nullptr;
        int baseNote = 0;   // lowest mapped note (keyboardOctave * 12)
        void paint (juce::Graphics&) override;
    };
    OctaveHighlight octaveHighlight;
    juce::String octaveRangeLabel() const;   // "C2–C4"-style readout, matching
                                              // the keyboard's own note-naming convention
    // This is JUCE's setKeyPressBaseOctave() parameter (N maps 'A' to MIDI
    // note N*12), NOT the octave number printed in a note name -- those
    // differ by MidiKeyboardComponent's getOctaveForMiddleC() offset (default
    // 3: note 60 = "C3"), which is why octaveRangeLabel() above goes through
    // juce::MidiMessage::getMidiNoteName rather than pasting this number
    // after a "C". Default 4 (base note 48) matches the strip's original
    // fixed opening view (setLowestVisibleKey (48)) exactly, so the default
    // QWERTY-mapped range is always the range visible on startup.
    int keyboardOctave = 4;

    PanicButton panicButton;   // header top-right: stop all sound
    std::unique_ptr<juce::Component> tempoBar;   // standalone only (brand band)

    // Header.
    juce::TextButton prevPresetButton { "<" }, nextPresetButton { ">" };
    juce::TextButton presetNameButton, savePresetButton { "SAVE" };
    juce::TextButton randomizeButton { "RANDOMIZE ALL" };
    juce::Slider wildnessSlider;
    juce::Label wildnessLabel;
    juce::ComboBox glideModeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> glideModeAttachment;
    juce::Slider glideSlider;
    juce::Label glideLabel;
    juce::TextButton voiceButton;   // opens the voice-mode call-out
    // The VOICE call-out's content panel while it is open. JUCE's modal
    // manager owns the call-out and deletes it asynchronously, so it can
    // outlive this editor; the destructor uses this to detach the panel from
    // the processor synchronously (see ~ContentComponent).
    juce::Component::SafePointer<juce::Component> openVoicePanel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> glideAttachment;
    // GLIDE knob only means anything once glideMode is off "Off".
    std::unique_ptr<DependentEnable> glideTimeEnable;
    AccentButton accentButton;
    juce::Slider masterSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> masterAttachment;
    std::array<juce::TextButton, params::numLockGroups> lockButtons;

    // Modules.
    std::array<std::unique_ptr<OscStrip>, params::numOscSlots> oscStrips;
    juce::TabbedComponent filterTabs { juce::TabbedButtonBar::TabsAtTop };
    juce::TabbedComponent envTabs { juce::TabbedButtonBar::TabsAtTop };
    juce::TabbedComponent lfoTabs { juce::TabbedButtonBar::TabsAtTop };
    ChaosPanel chaosPanel;
    ArpPanel arpPanel;
    DraggableTabs fxTabs;   // FX tabs are drag-reorderable -> chain order
    // Bolds a tab's label when its FX is enabled (fxTabs.isTabEngaged).
    std::unique_ptr<TabEngagementTracker> fxTabEngagement;
    MatrixPanel matrixPanel;
    OutputMeter outputMeter;
    // ASSIGN mode click-to-route overlay; added last (topmost), covers the
    // whole content area but is invisible/non-intercepting outside assign
    // mode (see AssignOverlay).
    std::unique_ptr<AssignOverlay> assignOverlay;

    // Focus anchor for showPopupAnchored() when the on-screen keyboard isn't
    // visible -- never shown, never clicked, exists only so grabKeyboardFocus()
    // has a real, always-focusable target (isShowing() is a hard requirement,
    // hence 1x1 rather than zero-size, and it must be a visible child, not
    // addChildComponent'd hidden). Intercepts nothing so it can't steal clicks.
    juce::Component popupFocusAnchor;

    // MIDI Learn diagnostics badge: while a learn is armed, shows a
    // breakdown of what's arrived since arming ("listening -- CC 0 - bend 3
    // - AT 0 - notes 12"), so Mike can tell not just THAT MIDI is reaching
    // the plugin but WHAT KIND -- notes incrementing while CC stays 0 means
    // the controller's knobs aren't sending CC at all. Once the manager
    // captures a CC, switches to "CC n (ch c) learned" for a couple of
    // seconds. See timerCallback.
    juce::Label midiLearnBadge;
    juce::String midiLearnBadgeParamID;   // which param the badge is tracking
    int midiLearnBadgeAssignedCC = -1;    // assignment seen last poll, to detect a fresh capture
    juce::uint32 midiLearnBadgeSeenAtCapture = 0;        // Telemetry::midiCcSeen snapshot at arm time
    juce::uint32 midiLearnBadgeNoteOnAtCapture = 0;      // Telemetry::midiNoteOnSeen snapshot at arm time
    juce::uint32 midiLearnBadgePitchWheelAtCapture = 0;  // Telemetry::midiPitchWheelSeen snapshot at arm time
    juce::uint32 midiLearnBadgeAftertouchAtCapture = 0;  // Telemetry::midiChannelPressureSeen + midiAftertouchSeen snapshot at arm time
    juce::uint32 midiLearnBadgeArmedAtMs = 0;            // arm time, for the no-CC hint
    juce::uint32 midiLearnBadgeNoCcHintThresholdMs = 3000;  // real threshold; test-settable, see setter above
    juce::uint32 midiLearnBadgeHideAtMs = 0;             // 0 = not counting down

    // Preset drawer: normally widens the window and sits in a left column of
    // its own, beside (never over) the module grid -- see
    // getContentBaseWidth()/resized(). Falls back to the old overlay-over-
    // the-grid behaviour (browserOverlays) if a host refuses to actually
    // resize the editor for it.
    std::unique_ptr<PresetBrowser> presetBrowser;
    bool presetBrowserOpen = false;
    bool browserOverlays = false;
    // Bumped on every togglePresetBrowser() call; a deferred close-completion
    // callback (see togglePresetBrowser) captures the value current at its
    // own call and checks it still matches before shrinking the window, so a
    // later toggle that supersedes it is a no-op instead of a stale clobber.
    int browserAnimSeq = 0;

    std::unique_ptr<juce::FileChooser> fileChooser;

    juce::String licenseLine;   // footer ownership stamp (refreshed with the library)

    // Faceplate restyle: geometry captured in resized() so paint() can draw
    // the recessed vertical seams + horizontal shadow bands from the live
    // module grid rather than hardcoded pixel positions.
    std::array<int, 4> rowShadowYs {};               // y of each row-transition shadow band
    std::vector<juce::Rectangle<int>> moduleGutters; // gap rects between adjacent modules in a row
                                                      // (x/width only -- paint() stretches each to its
                                                      // full row band via moduleGutterRows/rowShadowYs)
    std::vector<int> moduleGutterRows;               // row index (into rowShadowYs) each gutter belongs to
    int topNavRuleY = 0;                             // bottom edge of the lock-strip (top nav) band --
                                                      // separate from rowShadowYs[0] (which stays the true
                                                      // top of row 1, for the vertical seams) so the top nav
                                                      // row's own recessed-band rule can sit flush against
                                                      // it with no extra shadow doubling up nearby
    juce::Image noiseTexture;                        // cached fine-grain texture tile (seeded once)
    int moduleOriginX = 0;                           // left edge of the module area in resized()/
                                                      // paint() -- 0 normally, presetBrowserWidth
                                                      // when the drawer occupies its own column

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ContentComponent)
};

} // namespace ui

// Shell: hosts the fixed-layout content at base size and scales it
// proportionally — industry-standard plugin resizing. Window scale is
// remembered across sessions.
class SPASynthEditor : public juce::AudioProcessorEditor
{
public:
    explicit SPASynthEditor (SPASynthProcessor&);
    ~SPASynthEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void parentHierarchyChanged() override;

    // Largest scale <= 1.0 (quantised to 0.05 steps, floored so it still
    // fits) such that content at baseW x baseH, including the host-chrome
    // allowance (140 logical px tall, 40 wide -- title bar/menu/plugin
    // chrome), fits inside userArea; never below the constrainer's own
    // minimum (0.4, see configureConstrainer). Exposed for testing.
    static float scaleThatFits (juce::Rectangle<int> userArea, int baseW, int baseH);

private:
    void applyTheme();
    void configureConstrainer();   // aspect + size limits from content base size
    void keyboardToggled();        // resize the shell when the keyboard strip toggles
    void browserToggled();         // resize (width) the shell when the drawer toggles

    SPASynthProcessor& arsenalProcessor;
    ui::SPASynthLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltips { this };
    std::unique_ptr<ui::ContentComponent> content;
    bool hostViewWakeupDone = false;
    // Set true only while a resize is our OWN auto-fit (construction, or the
    // one-shot post-peer re-check below) -- resized() skips persisting
    // uiScale while this is set, so an automatic fit is never mistaken for
    // the user's chosen size (CLAUDE.md: remembered scale is only what the
    // user actually resized to).
    bool suppressScaleSave = false;
    // One-shot: re-checks screen fit once the editor has a real peer/is
    // actually on screen (a host may construct the editor off-screen first),
    // and only refits if the window as it stands does not actually fit.
    bool screenFitCheckDone = false;
    // The x delta (logical px, positive = moved left) actually applied by
    // the last successful native-window shift on drawer open -- see
    // NativeWindowShift.h. May be less than the requested width if clamped
    // at a screen edge; the close path undoes exactly this, not the nominal
    // drawer width, so a clamped open/close cycle never creeps the window.
    int nativeWindowShiftApplied = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SPASynthEditor)
};

} // namespace spa
