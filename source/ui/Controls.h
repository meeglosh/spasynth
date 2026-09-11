#pragma once

#include "Theme.h"
#include "../params/ParameterRegistry.h"
#include "../dsp/Telemetry.h"
#include "../SPASynthProcessor.h"

namespace spa::ui
{

class Knob;

// One shared 30 Hz timer polling every live Knob for its modulation-viz
// state, instead of each of the 100+ knobs on screen owning its own Timer.
// Message-thread only (registration and the callback both run there), so no
// locking. See Knob::pollModViz() below for what it actually reads/writes.
namespace detail
{
    class ModVizClock : private juce::Timer
    {
    public:
        static ModVizClock& get()
        {
            static ModVizClock instance;
            return instance;
        }

        void add (Knob* k)
        {
            knobs.addIfNotAlreadyThere (k);
            if (! isTimerRunning())
                startTimerHz (30);
        }

        void remove (Knob* k)
        {
            knobs.removeAllInstancesOf (k);
            if (knobs.isEmpty())
                stopTimer();
        }

    private:
        void timerCallback() override;   // defined below Knob (needs the full type)

        juce::Array<Knob*> knobs;
    };
}

// Recursively clears setMouseClickGrabsKeyboardFocus on a component and
// every current descendant. setWantsKeyboardFocus(false) alone does NOT
// stop a click from grabbing focus -- JUCE grabs keyboard focus on every
// mouse click unconditionally (Component::internalMouseDown ->
// grabKeyboardFocusInternal), walking up the parent chain, re-checking each
// ancestor's OWN dontFocusOnMouseClickFlag in turn, until it finds one that
// either wants focus (and takes it) or blocks the attempt outright.
// setMouseClickGrabsKeyboardFocus(false) is the flag that's actually
// checked first at each step and short-circuits the walk. Shared here so
// every part of the UI that assembles composite JUCE widgets (the preset
// browser's ListBox, SectionPanel's auto-built controls, etc.) can sweep a
// whole subtree -- including JUCE-internal helpers like ListBox's
// RowComponent/viewport/scrollbars or a ComboBox's internal text Label --
// in one call instead of hand-tracking every leaf.
inline void disableMouseClickFocusGrab (juce::Component& c)
{
    c.setMouseClickGrabsKeyboardFocus (false);

    for (int i = 0; i < c.getNumChildComponents(); ++i)
        if (auto* child = c.getChildComponent (i))
            disableMouseClickFocusGrab (*child);
}

// A thin-ring knob with its label underneath — the atomic control of the UI.
// Set modColoured for modulation-domain knobs (cyan ring).
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID,
          const juce::String& labelText, bool modColoured = false)
        : parameter (apvts.getParameter (paramID)),
          restingText (labelText.toUpperCase()),
          modAccent (modColoured)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        // setWantsKeyboardFocus(false) alone does NOT stop a click from
        // grabbing focus -- JUCE grabs it unconditionally on mouseDown via a
        // SEPARATE flag, walking up to a parent if the clicked component
        // itself doesn't want focus. setMouseClickGrabsKeyboardFocus(false)
        // is the actual switch: it makes grabKeyboardFocusInternal() return
        // immediately on a mouse-click-triggered grab, so touching a knob
        // can never steal focus from the on-screen keyboard's QWERTY input.
        slider.setWantsKeyboardFocus (false);
        slider.setMouseClickGrabsKeyboardFocus (false);
        slider.getProperties().set ("paramID", paramID);      // for MIDI Learn
        if (modColoured)
            slider.setComponentID ("mod");
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            apvts, paramID, slider);
        addAndMakeVisible (slider);

        label.setText (restingText, juce::dontSendNotification);
        label.setFont (metrics::smallFont());
        label.setJustificationType (juce::Justification::centred);
        label.setInterceptsMouseClicks (false, false);
        label.setMinimumHorizontalScale (0.6f);
        addAndMakeVisible (label);

        // Press state: the live value readout.
        wireDragReadout (slider, label, parameter, restingText, modAccent);

        // Modulation-viz: only mod-destination params poll (modDestIndex is
        // -1 for anything that isn't one, e.g. the knob's own label text or
        // a matrix depth knob's target).
        modDestIdx = params::modDestIndex (paramID);
        if (modDestIdx >= 0)
            detail::ModVizClock::get().add (this);
    }

    ~Knob() override
    {
        if (modDestIdx >= 0)
            detail::ModVizClock::get().remove (this);
    }

    // Shared wiring: while dragging, `label` shows the parameter's value in
    // the accent colour; on release it reverts. Guarded so programmatic
    // value changes never hijack the label.
    static void wireDragReadout (juce::Slider& s, juce::Label& l,
                                 juce::RangedAudioParameter* param,
                                 const juce::String& restingText, bool modAccent)
    {
        const auto showValue = [&l, param, modAccent]
        {
            if (param == nullptr)
                return;
            const auto& t = currentTheme();
            l.setColour (juce::Label::textColourId, modAccent ? t.accentMod : t.accent);
            auto text = param->getCurrentValueAsText();
            if (param->getLabel().isNotEmpty())
                text << " " << param->getLabel();
            l.setText (text, juce::dontSendNotification);
        };

        s.onDragStart = showValue;
        s.onValueChange = [&s, showValue]
        {
            if (s.isMouseButtonDown())
                showValue();
        };
        s.onDragEnd = [&l, restingText]
        {
            l.removeColour (juce::Label::textColourId);
            l.setText (restingText, juce::dontSendNotification);
        };
    }

    void resized() override
    {
        auto area = getLocalBounds();
        label.setBounds (area.removeFromBottom (13));
        slider.setBounds (area);
    }

    juce::Slider slider;
    juce::Label label;

    // Drop the parameter attachment early, for controls that can outlive
    // the editor (the VOICE call-out's panel, owned by JUCE's modal manager
    // and deleted asynchronously) and so must not touch the processor's
    // APVTS from a destructor that may run after the processor is gone.
    void detach() { attachment.reset(); }

    // Called by detail::ModVizClock at 30 Hz for mod-destination knobs only
    // (modDestIdx >= 0). Defined below the class -- needs dsp::Telemetry's
    // full definition, already included above.
    void pollModViz();

private:
    juce::RangedAudioParameter* parameter = nullptr;
    juce::String restingText;
    bool modAccent = false;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    int modDestIdx = -1;
    dsp::Telemetry* modTelemetry = nullptr;   // resolved lazily, then cached -- see pollModViz()
};

inline void detail::ModVizClock::timerCallback()
{
    for (auto* k : knobs)
        k->pollModViz();
}

inline void Knob::pollModViz()
{
    // Resolve THIS knob's own owning processor's Telemetry, not some
    // process-wide "current" instance -- Mike routinely runs several
    // SPASynth instances in one Logic session, and every knob must only
    // ever animate to its own instance's modulation. The knob's editor
    // never changes once parented, so this resolves once and is cached;
    // until the knob is actually inside an editor (e.g. mid-construction),
    // just skip this tick.
    if (modTelemetry == nullptr)
    {
        if (auto* editor = findParentComponentOfClass<juce::AudioProcessorEditor>())
            if (auto* proc = dynamic_cast<SPASynthProcessor*> (&editor->processor))
                modTelemetry = &proc->getTelemetry();
    }
    if (modTelemetry == nullptr)
        return;

    auto* tel = modTelemetry;
    const auto active = tel->modDestActive[(size_t) modDestIdx].load (std::memory_order_relaxed);
    const auto dim = tel->activeVoices.load (std::memory_order_relaxed) == 0;
    const auto value = tel->modDestValue[(size_t) modDestIdx].load (std::memory_order_relaxed);

    auto& props = slider.getProperties();
    const bool wasActive = (bool) props.getWithDefault ("modActive", false);
    const bool wasDim = (bool) props.getWithDefault ("modDim", false);
    const float wasValue = (float) (double) props.getWithDefault ("modValue", 0.0);

    // Repaint only when something visible actually changed, and only when
    // active (an inactive dest holding a stale value doesn't need repaints)
    // -- keeps this from becoming a second per-block invalidation source
    // (see paintRegionRegressionTest).
    const bool valueMoved = active && std::abs (value - wasValue) > (1.0f / 256.0f);
    if (active == wasActive && dim == wasDim && ! valueMoved)
        return;

    props.set ("modActive", active);
    props.set ("modDim", dim);
    props.set ("modValue", value);
    slider.repaint();
}

// Labelled combo box row.
class Choice : public juce::Component
{
public:
    Choice (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID)
    {
        combo.setWantsKeyboardFocus (false);            // see Knob's comment
        combo.setMouseClickGrabsKeyboardFocus (false);  // the actual fix -- see Knob's comment
        combo.getProperties().set ("paramID", paramID);
        if (const auto* def = params::find (paramID))
            combo.addItemList (def->choices, 1);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            apvts, paramID, combo);
        addAndMakeVisible (combo);
    }

    void resized() override { combo.setBounds (getLocalBounds()); }

    juce::ComboBox combo;

    void detach() { attachment.reset(); }   // see Knob::detach

private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
};

// Pill toggle bound to a bool parameter.
class Toggle : public juce::Component
{
public:
    Toggle (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID,
            const juce::String& text)
        : button (text)
    {
        button.setWantsKeyboardFocus (false);            // see Knob's comment
        button.setMouseClickGrabsKeyboardFocus (false);  // the actual fix -- see Knob's comment
        button.getProperties().set ("paramID", paramID);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            apvts, paramID, button);
        addAndMakeVisible (button);
    }

    void resized() override { button.setBounds (getLocalBounds()); }

    juce::ToggleButton button;

private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};

// Grays out (and disables interaction on) a set of target components
// whenever another parameter's value makes them irrelevant -- e.g. the LFO
// rate knob while sync is on, or the delay time knob while delay sync is on.
// APVTS listener callbacks can fire off the message thread, so this defers
// through AsyncUpdater before touching any Component, same pattern OscStrip
// already uses for its mode-driven visibility.
//
// setEnabled(false) on a target propagates through JUCE's parent-enablement
// chain (Component::isEnabled() walks up to its parent and ANDs them), so
// passing a Knob/Choice/Toggle wrapper as the single target is enough -- its
// inner slider/combo/button (and the Knob's caption label) dim themselves
// via the LookAndFeel's isEnabled() checks. For SectionPanel-built controls,
// where the label is a sibling rather than a child, pass both explicitly.
class DependentEnable : private juce::AudioProcessorValueTreeState::Listener,
                        private juce::AsyncUpdater
{
public:
    DependentEnable (juce::AudioProcessorValueTreeState& apvtsIn, const juce::String& gateParamID,
                     std::function<bool (float)> predicateIn,
                     std::vector<juce::Component*> targetsIn)
        : apvts (apvtsIn), gateID (gateParamID), predicate (std::move (predicateIn)),
          targets (std::move (targetsIn))
    {
        if (auto* raw = apvts.getRawParameterValue (gateID))
            lastValue.store (raw->load());
        applyState();                          // initial state, before the first repaint
        apvts.addParameterListener (gateID, this);
    }

    ~DependentEnable() override
    {
        apvts.removeParameterListener (gateID, this);
    }

private:
    void parameterChanged (const juce::String&, float newValue) override
    {
        lastValue.store (newValue);
        triggerAsyncUpdate();
    }

    void handleAsyncUpdate() override { applyState(); }

    void applyState()
    {
        const auto enabled = predicate (lastValue.load());
        for (auto* c : targets)
            if (c != nullptr)
                c->setEnabled (enabled);
    }

    juce::AudioProcessorValueTreeState& apvts;
    juce::String gateID;
    std::function<bool (float)> predicate;
    std::vector<juce::Component*> targets;
    std::atomic<float> lastValue { 0.0f };
};

// Tracks a set of "engaged" gate parameters (e.g. FX enable toggles) keyed by
// a tab/label name, and repaints a target component whenever any of them
// changes -- same listener+AsyncUpdater idiom as DependentEnable above, so UI
// never gets touched off the parameter-callback thread. isEngaged() is a
// plain synchronous read (juce::AudioProcessorValueTreeState::getRawParameterValue
// is safe to read from any thread at any time), so it can be queried directly
// from paint(). A tab can map to more than one param id (e.g. one tab
// covering two effects); it reads as engaged if ANY of them is on.
class TabEngagementTracker : private juce::AudioProcessorValueTreeState::Listener,
                             private juce::AsyncUpdater
{
public:
    TabEngagementTracker (juce::AudioProcessorValueTreeState& apvtsIn,
                          std::vector<std::pair<juce::String, std::vector<juce::String>>> paramsByTabIn,
                          juce::Component& repaintTargetIn)
        : apvts (apvtsIn), paramsByTab (std::move (paramsByTabIn)), repaintTarget (repaintTargetIn)
    {
        for (auto& entry : paramsByTab)
            for (auto& id : entry.second)
            {
                allIds.push_back (id);
                apvts.addParameterListener (id, this);
            }
    }

    ~TabEngagementTracker() override
    {
        for (auto& id : allIds)
            apvts.removeParameterListener (id, this);
    }

    bool isEngaged (const juce::String& tabName) const
    {
        for (auto& entry : paramsByTab)
        {
            if (entry.first != tabName)
                continue;
            for (auto& id : entry.second)
                if (auto* raw = apvts.getRawParameterValue (id))
                    if (raw->load() > 0.5f)
                        return true;
            return false;
        }
        return false;
    }

private:
    void parameterChanged (const juce::String&, float) override { triggerAsyncUpdate(); }
    void handleAsyncUpdate() override { repaintTarget.repaint(); }

    juce::AudioProcessorValueTreeState& apvts;
    std::vector<std::pair<juce::String, std::vector<juce::String>>> paramsByTab;
    std::vector<juce::String> allIds;
    juce::Component& repaintTarget;
};

} // namespace spa::ui
