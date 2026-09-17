#pragma once

#include "Controls.h"
#include "Displays.h"
#include "SectionPanel.h"

namespace spa
{
class SPASynthProcessor;

namespace ui
{

// Extensions accepted for oscillator content -- ONE list read by both the
// LOAD file chooser and drag-and-drop, so the two routes never disagree.
// Wavetable mode excludes mp3 (matches the pre-existing chooser filter);
// every other file-shaped mode (sample/granular) accepts it.
juce::StringArray oscContentExtensions (bool wavetableMode);
juce::String oscContentWildcard (bool wavetableMode);
bool oscContentAccepts (const juce::String& filePathOrName, bool wavetableMode);

// One oscillator column: scope on top, curated mode-aware controls below —
// the panel reshapes itself for Wavetable / Sample / Granular like the
// reference synths do.
class OscStrip : public juce::Component,
                 public juce::FileDragAndDropTarget,
                 public juce::SettableTooltipClient,
                 private juce::AudioProcessorValueTreeState::Listener,
                 private juce::AsyncUpdater,
                 private juce::ChangeListener
{
public:
    OscStrip (SPASynthProcessor&, int slot);
    ~OscStrip() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    // juce::FileDragAndDropTarget -- lets a customer drag a file straight
    // from Finder/Explorer/a DAW browser onto this strip instead of using
    // LOAD. Does not grab keyboard focus at any point (see the
    // setMouseClickGrabsKeyboardFocus rule in Controls.h/CLAUDE.md).
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray& files, int x, int y) override;
    void fileDragExit (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    void parameterChanged (const juce::String&, float) override { triggerAsyncUpdate(); }
    void handleAsyncUpdate() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override { triggerAsyncUpdate(); }
    void chooseContent();
    params::OscMode currentMode() const;
    juce::String contentName() const;

    // In-pack quick-swap affordance in the header (Sample/Granular modes):
    // the sample name plus a caret opens a dropdown of the whole pack. Shown
    // only when the loaded sample is in a pack.
    juce::Rectangle<int> headerNameRect() const;
    bool sampleSwapAvailable() const;
    void paintSampleSwapper (juce::Graphics&);
    void openSampleMenu();

    // Right-click on the header title (not the quick-swap name widget, not a
    // knob, not the waveform display) offers Copy/Swap with the other two
    // slots -- see CLAUDE.md's brief for this feature. No clipboard: with
    // only three slots, naming a direct destination is one action instead
    // of a copy-then-paste pair with hidden state.
    juce::Rectangle<int> headerTitleRect() const;
    void openCopySwapMenu();

    SPASynthProcessor& processor;
    const int slot;

    WaveDisplay display;
    Toggle enable;
    Choice mode;
    juce::TextButton loadButton { "LOAD" };
    juce::TextButton factoryButton { "INIT" };

    std::vector<std::unique_ptr<Knob>> commonKnobs;      // coarse/fine/level/pan
    std::vector<std::unique_ptr<Knob>> wavetableKnobs;
    std::vector<std::unique_ptr<Knob>> sampleKnobs;
    std::vector<std::unique_ptr<Knob>> granularKnobs;
    std::vector<std::unique_ptr<Knob>> analogKnobs, fmKnobs, pluckKnobs;
    std::unique_ptr<Choice> phaseMode, table, analogShape, noiseColor;
    std::unique_ptr<Toggle> loop, keytrackSample, keytrackGranular;

    // Sample SYNC (time-stretch to host BPM, beat-locked loop). Sample mode
    // AND LOOP on only -- SYNC only makes sense as a loop feature, so it's
    // hidden the instant LOOP goes off. The per-oscillator time-signature
    // dropdown (see timeSig below) is shown only once SYNC is ALSO on --
    // Mike's call: it's meaningless while the loop free-runs unsynced.
    std::unique_ptr<Toggle> sync;

    // Per-oscillator time signature for the beat-locked loop ("Host", 4/4,
    // 3/4, 6/8, 2/4, 5/4, 7/8, 12/8 -- id::osc::timeSig). Replaces the old
    // BPM readout Mike found unhelpful; lets each sample oscillator run its
    // own meter against the project (or another oscillator) for
    // polyrhythms. Visible only Sample mode + LOOP on + SYNC on.
    std::unique_ptr<Choice> timeSig;

    // Loop start/end only mean anything while looping is on; kept as a
    // separate rule from the mode-driven setVisible() above so the two
    // states (visible-per-mode, enabled-per-loop) don't fight each other.
    std::unique_ptr<DependentEnable> loopRangeEnable;

    std::unique_ptr<juce::FileChooser> fileChooser;

    // True while an acceptable drag hovers this strip -- drives the paint()
    // highlight, cleared on exit/drop. Test-visible via isDragHighlighted().
    bool dragHighlight = false;
public:
    bool isDragHighlighted() const { return dragHighlight; }
private:

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OscStrip)
};

// Filter: response curve + type + cutoff/res/drive.
class FilterPanel : public juce::Component
{
public:
    FilterPanel (SPASynthProcessor&, int filterIndex);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    const int index;
    FilterDisplay display;
    Choice type;
    Knob cutoff, resonance, drive;
    Knob keytrack, envAmount, mix;
    std::unique_ptr<Toggle> enable;
    std::unique_ptr<Choice> routing;     // filter 2 only
};

// One ADSR page: curve + four knobs.
class EnvPanel : public juce::Component
{
public:
    EnvPanel (SPASynthProcessor&, const juce::String& idPrefix, int envIndex);
    void resized() override;

private:
    EnvDisplay display;
    Knob attack, decay, sustain, release;
};

// One LFO page: shape scope + controls.
class LFOPanel : public juce::Component
{
public:
    LFOPanel (SPASynthProcessor&, int lfoIndex);
    void resized() override;

private:
    LFODisplay display;
    Choice shape, division;
    Knob rate, phase;
    Toggle sync, retrig, unipolar;

    // Rate only means anything when free-running; division only means
    // anything when synced -- grey out whichever doesn't apply.
    DependentEnable rateEnable, divisionEnable;
};

// Organic Chaos: walker scope + master knobs + per-target drift strip.
class ChaosPanel : public juce::Component
{
public:
    explicit ChaosPanel (SPASynthProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;

    // Exposed for chaosSyncTest -- same idea as DraggableTabs::isTabEngaged.
    bool isSyncEngagedForTest() const { return syncTracker.isEngaged ("sync"); }

private:
    ChaosDisplay display;
    Toggle enable;
    Knob depth, rate, mix;
    Toggle sync;
    Choice division;

    // Rate only means anything when free-running; division only means
    // anything when synced -- same pattern as LFOPanel's rateEnable/
    // divisionEnable.
    DependentEnable rateEnable, divisionEnable;
    // Repaints the header when sync toggles, so paint() can switch the
    // title between "Organic Chaos" and "Organized Chaos".
    TabEngagementTracker syncTracker;

    struct Drift
    {
        std::unique_ptr<Toggle> on;
        std::unique_ptr<Knob> amount;
    };
    std::array<Drift, 6> drifts;   // pitch, phase, position, amp, sat, dist
};

// Arpeggiator: compact four-row layout (toggles+mode+rate / phrase+vel /
// octave-gate-swing knobs / chance-stutter-jump-humanize knobs).
class ArpPanel : public juce::Component
{
public:
    explicit ArpPanel (juce::AudioProcessorValueTreeState&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    Toggle enable, latch;
    Choice mode, division, phrase, velMode;
    Knob octaves, gate, swing;
    Knob chance, stutter, jump, humanize;
};

// One FX tab: character scope on top, the section's registry controls below.
class FXPanel : public juce::Component
{
public:
    FXPanel (juce::AudioProcessorValueTreeState&, FXDisplay::Kind,
             params::Section, const juce::String& title);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::String panelTitle;
    FXDisplay display;
    SectionPanel controls;   // bare: frame + header drawn by this panel

    // Delay tab only: time vs. division dimming, mirroring the LFO rule.
    // Null for every other section.
    std::unique_ptr<DependentEnable> delayTimeEnable, delayDivisionEnable;
};

} // namespace ui
} // namespace spa
