#include "ModulePanels.h"
#include "../SPASynthProcessor.h"
#include "../library/Library.h"
#include "../dsp/SamplePlayer.h"
#include "AssignOverlay.h"   // free spa::ui::showPopupAnchored -- see its declaration comment
#include <juce_audio_utils/juce_audio_utils.h>

namespace spa::ui
{

namespace id = params::id;

juce::StringArray oscContentExtensions (bool wavetableMode)
{
    juce::StringArray ext { "wav", "aif", "aiff", "flac" };
    if (! wavetableMode)
        ext.add ("mp3");
    return ext;
}

juce::String oscContentWildcard (bool wavetableMode)
{
    juce::StringArray patterns;
    for (auto& e : oscContentExtensions (wavetableMode))
        patterns.add ("*." + e);
    return patterns.joinIntoString (";");
}

bool oscContentAccepts (const juce::String& filePathOrName, bool wavetableMode)
{
    return oscContentExtensions (wavetableMode)
             .contains (juce::File (filePathOrName).getFileExtension().substring (1), true);
}

// ============================== OscStrip ===================================

OscStrip::OscStrip (SPASynthProcessor& p, int slotIndex)
    : processor (p), slot (slotIndex),
      display (p, slotIndex),
      enable (p.getAPVTS(), id::oscSlot (slotIndex, id::osc::enable), "ON"),
      mode (p.getAPVTS(), id::oscSlot (slotIndex, id::osc::mode)),
      powerTracker (p.getAPVTS(),
                    { { "on", { id::oscSlot (slotIndex, id::osc::enable) } } }, *this)
{
    // Clicking the header to open the sample-swap menu shouldn't steal focus
    // from the on-screen keyboard's QWERTY note input -- see Controls.h's
    // Knob for the full explanation.
    setMouseClickGrabsKeyboardFocus (false);
    setTooltip ("Right-click the header to copy or swap this oscillator with another slot.");

    auto& apvts = processor.getAPVTS();
    const auto pid = [this] (const char* key) { return id::oscSlot (slot, key); };

    addAndMakeVisible (display);
    addAndMakeVisible (enable);
    addAndMakeVisible (mode);

    loadButton.onClick = [this] { chooseContent(); };
    loadButton.setMouseClickGrabsKeyboardFocus (false);   // see Controls.h's Knob
    addAndMakeVisible (loadButton);
    factoryButton.onClick = [this]
    {
        // Set the Table choice back to 0 (Basic Shapes); the processor's
        // parameter listener rebuilds/installs it in response (also
        // discards any loaded file, same as picking a table from the menu).
        if (auto* param = processor.getAPVTS().getParameter (id::oscSlot (slot, id::osc::table)))
            param->setValueNotifyingHost (0.0f);
    };
    factoryButton.setMouseClickGrabsKeyboardFocus (false);   // see Controls.h's Knob
    addAndMakeVisible (factoryButton);

    const auto knob = [&apvts] (const juce::String& paramID, const juce::String& label)
    {
        return std::make_unique<Knob> (apvts, paramID, label);
    };

    commonKnobs.push_back (knob (pid (id::osc::coarse), "COARSE"));
    commonKnobs.push_back (knob (pid (id::osc::fine), "FINE"));
    commonKnobs.push_back (knob (pid (id::osc::level), "LEVEL"));
    commonKnobs.push_back (knob (pid (id::osc::pan), "PAN"));

    wavetableKnobs.push_back (knob (pid (id::osc::position), "POSITION"));
    wavetableKnobs.push_back (knob (pid (id::osc::unisonCount), "UNISON"));
    wavetableKnobs.push_back (knob (pid (id::osc::unisonDetune), "DETUNE"));
    wavetableKnobs.push_back (knob (pid (id::osc::unisonBlend), "BLEND"));
    wavetableKnobs.push_back (knob (pid (id::osc::unisonWidth), "WIDTH"));
    wavetableKnobs.push_back (knob (pid (id::osc::phase), "PHASE"));
    phaseMode = std::make_unique<Choice> (apvts, pid (id::osc::phaseMode));
    table = std::make_unique<Choice> (apvts, pid (id::osc::table));

    sampleKnobs.push_back (knob (pid (id::osc::sampleStart), "START"));
    auto loopStartKnob = knob (pid (id::osc::loopStart), "LOOP ST");
    auto loopEndKnob = knob (pid (id::osc::loopEnd), "LOOP END");
    auto* loopStartPtr = loopStartKnob.get();
    auto* loopEndPtr = loopEndKnob.get();
    sampleKnobs.push_back (std::move (loopStartKnob));
    sampleKnobs.push_back (std::move (loopEndKnob));
    auto xfadeKnob = knob (pid (id::osc::loopXfade), "XFADE");
    xfadeKnob->slider.setTooltip ("Crossfades the loop point so the seam is inaudible. The "
                                  "crossfade borrows audio from outside the loop, so the loop "
                                  "length never changes.");
    auto* loopXfadePtr = xfadeKnob.get();
    sampleKnobs.push_back (std::move (xfadeKnob));
    sampleKnobs.push_back (knob (pid (id::osc::rootNote), "ROOT"));
    loop = std::make_unique<Toggle> (apvts, pid (id::osc::loop), "LOOP");
    keytrackSample = std::make_unique<Toggle> (apvts, pid (id::osc::keytrack), "KEY");
    sync = std::make_unique<Toggle> (apvts, pid (id::osc::syncToBpm), "SYNC");
    sync->button.setTooltip ("Beat-lock the loop to the project tempo; the loop snaps to "
                             "the sample's beat grid.");

    // Per-oscillator time signature -- shown only once SYNC is on (it's
    // meaningless while the loop free-runs unsynced). "Host" (index 0)
    // follows the project/host signature; the rest let this one oscillator
    // run its own meter for polyrhythms against the project or other slots.
    timeSig = std::make_unique<Choice> (apvts, pid (id::osc::timeSig));
    timeSig->combo.setTooltip ("This oscillator's own time signature for its beat-locked "
                               "loop -- set it differently from the project (or other "
                               "oscillators) for polyrhythms.");

    // LOOP ST/END/XFADE only matter while looping is on -- independent of
    // the mode-driven visibility switch below, so it survives mode
    // round-trips. XFADE is meaningful in BOTH sync states (the SYNC
    // stretcher applies the same crossfade per-grain -- see SamplePlayer's
    // readLoopCrossfaded), so it dims with LOOP alone, exactly like ST/END.
    loopRangeEnable = std::make_unique<DependentEnable> (
        apvts, pid (id::osc::loop), [] (float v) { return v >= 0.5f; },
        std::vector<juce::Component*> { loopStartPtr, loopEndPtr, loopXfadePtr });

    granularKnobs.push_back (knob (pid (id::osc::grainSize), "SIZE"));
    granularKnobs.push_back (knob (pid (id::osc::grainDensity), "DENSITY"));
    granularKnobs.push_back (knob (pid (id::osc::grainPos), "POSITION"));
    granularKnobs.push_back (knob (pid (id::osc::grainSpray), "SPRAY"));
    granularKnobs.push_back (knob (pid (id::osc::grainPitch), "PITCH"));
    granularKnobs.push_back (knob (pid (id::osc::rootNote), "ROOT"));
    keytrackGranular = std::make_unique<Toggle> (apvts, pid (id::osc::keytrack), "KEY");

    analogKnobs.push_back (knob (pid (id::osc::pulseWidth), "PW"));
    analogKnobs.push_back (knob (pid (id::osc::sub), "SUB"));
    analogShape = std::make_unique<Choice> (apvts, pid (id::osc::analogShape));

    fmKnobs.push_back (knob (pid (id::osc::fmRatio), "RATIO"));
    fmKnobs.push_back (knob (pid (id::osc::fmIndex), "INDEX"));

    noiseColor = std::make_unique<Choice> (apvts, pid (id::osc::noiseColor));

    pluckKnobs.push_back (knob (pid (id::osc::pluckDamp), "DAMP"));

    for (auto* set : { &commonKnobs, &wavetableKnobs, &sampleKnobs, &granularKnobs,
                       &analogKnobs, &fmKnobs, &pluckKnobs })
        for (auto& k : *set)
            addChildComponent (*k);
    addChildComponent (*phaseMode);
    addChildComponent (*table);
    addChildComponent (*loop);
    addChildComponent (*keytrackSample);
    addChildComponent (*keytrackGranular);
    addChildComponent (*sync);
    addChildComponent (*timeSig);
    addChildComponent (*analogShape);
    addChildComponent (*noiseColor);

    apvts.addParameterListener (pid (id::osc::mode), this);
    // LOOP gates SYNC's visibility (SYNC only makes sense as a loop feature).
    apvts.addParameterListener (pid (id::osc::loop), this);
    // SYNC gates the per-oscillator time-signature dropdown's visibility.
    apvts.addParameterListener (pid (id::osc::syncToBpm), this);
    processor.addChangeListener (this);
    handleAsyncUpdate();
}

OscStrip::~OscStrip()
{
    processor.removeChangeListener (this);
    processor.getAPVTS().removeParameterListener (id::oscSlot (slot, id::osc::mode), this);
    processor.getAPVTS().removeParameterListener (id::oscSlot (slot, id::osc::loop), this);
    processor.getAPVTS().removeParameterListener (id::oscSlot (slot, id::osc::syncToBpm), this);
}

params::OscMode OscStrip::currentMode() const
{
    auto* param = processor.getAPVTS().getParameter (id::oscSlot (slot, id::osc::mode));
    return (params::OscMode) (int) param->convertFrom0to1 (param->getValue());
}

juce::String OscStrip::contentName() const
{
    switch (currentMode())
    {
        case params::OscMode::wavetable:
            if (processor.isWavetableLoading (slot))
                return "loading...";
            return processor.getWavetableName (slot);

        case params::OscMode::sample:
        case params::OscMode::granular:
        {
            // While a new file is in flight, neither the previous name nor a
            // stale error is the truth — say so.
            if (processor.isSampleLoading (slot))
                return "loading...";
            const auto error = processor.getSampleError (slot);
            if (error.isNotEmpty())
                return "! " + error;
            const auto name = processor.getSampleName (slot);
            return name.isNotEmpty() ? name : "no SFX";
        }

        case params::OscMode::analog: return "virtual analog";
        case params::OscMode::fm:     return "2-op FM";
        case params::OscMode::noise:  return "noise";
        case params::OscMode::pluck:  return "karplus-strong";
    }
    return {};
}

void OscStrip::handleAsyncUpdate()
{
    const auto m = currentMode();

    for (auto& k : commonKnobs)
        k->setVisible (true);
    for (auto& k : wavetableKnobs)
        k->setVisible (m == params::OscMode::wavetable);
    for (auto& k : sampleKnobs)
        k->setVisible (m == params::OscMode::sample);
    for (auto& k : granularKnobs)
        k->setVisible (m == params::OscMode::granular);
    for (auto& k : analogKnobs)
        k->setVisible (m == params::OscMode::analog);
    for (auto& k : fmKnobs)
        k->setVisible (m == params::OscMode::fm);
    for (auto& k : pluckKnobs)
        k->setVisible (m == params::OscMode::pluck);

    phaseMode->setVisible (m == params::OscMode::wavetable);
    table->setVisible (m == params::OscMode::wavetable);
    loop->setVisible (m == params::OscMode::sample);
    keytrackSample->setVisible (m == params::OscMode::sample);
    keytrackGranular->setVisible (m == params::OscMode::granular);

    // SYNC only exists as a LOOP feature -- Mike's call: "SYNC should only
    // be available ... when LOOP is on". Turning LOOP off hides it
    // immediately; the underlying param value is untouched (the engine
    // itself is gated on loop&&sync, see SPASynthProcessor). The per-
    // oscillator time-signature dropdown goes one step further and only
    // shows once SYNC is ALSO on (Mike: hidden, not dimmed, while SYNC is
    // off -- it's meaningless until the loop is actually beat-locked).
    const auto loopOn = processor.getAPVTS()
                            .getRawParameterValue (id::oscSlot (slot, id::osc::loop))->load() >= 0.5f;
    const auto syncOn = processor.getAPVTS()
                            .getRawParameterValue (id::oscSlot (slot, id::osc::syncToBpm))->load() >= 0.5f;
    const auto showSync = m == params::OscMode::sample && loopOn;
    sync->setVisible (showSync);
    timeSig->setVisible (showSync && syncOn);
    analogShape->setVisible (m == params::OscMode::analog);
    noiseColor->setVisible (m == params::OscMode::noise);
    factoryButton.setVisible (m == params::OscMode::wavetable);
    loadButton.setVisible (m == params::OscMode::wavetable
                        || m == params::OscMode::sample
                        || m == params::OscMode::granular);

    resized();
    repaint();
}

void OscStrip::chooseContent()
{
    const auto wavetableMode = currentMode() == params::OscMode::wavetable;
    const auto contentKind = wavetableMode ? library::ContentKind::wavetable
                                          : library::ContentKind::sample;
    const auto lastFolder = library::getLastContentFolder (contentKind);
    const auto libraryRoot = library::findLibraryRoot();

    fileChooser = std::make_unique<juce::FileChooser> (
        wavetableMode ? "Load wavetable" : "Load SFX / sample",
        lastFolder.isDirectory() ? lastFolder
            : ! wavetableMode && libraryRoot.isDirectory()
                ? libraryRoot : juce::File::getSpecialLocation (juce::File::userHomeDirectory),
        oscContentWildcard (wavetableMode));

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                            | juce::FileBrowserComponent::canSelectFiles,
                              [this, wavetableMode, contentKind] (const juce::FileChooser& fc)
    {
        if (! fc.getResult().existsAsFile())
            return;
        library::setLastContentFolder (contentKind, fc.getResult());
        if (wavetableMode)
            processor.loadWavetableFromFile (slot, fc.getResult());
        else
            processor.loadSampleFromFile (slot, fc.getResult());
    });
}

bool OscStrip::isInterestedInFileDrag (const juce::StringArray& files)
{
    // A slot in a mode that plays no file at all still accepts a drop --
    // that means "put this here", handled by switching to Sample in
    // filesDropped() -- so the accepted set is always the sample/granular
    // (non-wavetable) extensions unless the slot IS in Wavetable mode.
    const auto wavetableMode = currentMode() == params::OscMode::wavetable;
    for (auto& f : files)
        if (oscContentAccepts (f, wavetableMode))
            return true;
    return false;
}

void OscStrip::fileDragEnter (const juce::StringArray&, int, int)
{
    dragHighlight = true;
    repaint();
}

void OscStrip::fileDragExit (const juce::StringArray&)
{
    dragHighlight = false;
    repaint();
}

void OscStrip::filesDropped (const juce::StringArray& files, int, int)
{
    dragHighlight = false;
    repaint();

    const auto oscMode = currentMode();
    const auto wavetableMode = oscMode == params::OscMode::wavetable;

    juce::File chosen;
    for (auto& path : files)
    {
        if (oscContentAccepts (path, wavetableMode))
        {
            chosen = juce::File (path);
            break;
        }
    }
    if (! chosen.existsAsFile())
        return;

    // Non-file-shaped modes (analog/fm/noise/pluck): a dropped audio file
    // clearly means "put this here" -- switch to Sample first, through the
    // APVTS on the message thread (never write the raw value directly).
    const bool fileShaped = oscMode == params::OscMode::wavetable
                          || oscMode == params::OscMode::sample
                          || oscMode == params::OscMode::granular;
    if (! fileShaped)
    {
        if (auto* param = processor.getAPVTS().getParameter (id::oscSlot (slot, id::osc::mode)))
            param->setValueNotifyingHost (param->convertTo0to1 ((float) (int) params::OscMode::sample));
    }

    library::setLastContentFolder (wavetableMode ? library::ContentKind::wavetable
                                                 : library::ContentKind::sample, chosen);
    if (wavetableMode)
        processor.loadWavetableFromFile (slot, chosen);
    else
        processor.loadSampleFromFile (slot, chosen);
}

void OscStrip::paint (juce::Graphics& g)
{
    draw::panel (g, getLocalBounds().toFloat());

    const auto m = currentMode();
    const bool swap = (m == params::OscMode::sample || m == params::OscMode::granular);

    // In swap modes the header's right side becomes the quick-swap widget, so
    // suppress the plain readout there and paint the widget instead.
    draw::sectionHeader (g, getLocalBounds(),
                         "Oscillator " + id::oscSlotLetter (slot),
                         swap ? juce::String() : contentName(),
                         draw::moduleHeaderColour (currentTheme(), powerTracker.isEngaged ("on")));
    if (swap)
        paintSampleSwapper (g);

    // Drag-and-drop highlight: an outline-only glow (same colour token and
    // layered-falloff idea as AssignOverlay's halo) so it flags the whole
    // strip as the drop target without a fill that would wash out the
    // waveform or the header text underneath.
    if (dragHighlight)
    {
        const auto& t = currentTheme();
        auto bounds = getLocalBounds().toFloat().reduced (1.0f);
        for (int i = 3; i >= 0; --i)
        {
            const float inflate = (float) i * 1.5f;
            g.setColour (t.assignGlow.withAlpha (i == 0 ? 0.9f : 0.14f));
            g.drawRoundedRectangle (bounds.expanded (inflate),
                                     metrics::cornerRadius + inflate,
                                     i == 0 ? 2.0f : 1.5f);
        }
    }
}

juce::Rectangle<int> OscStrip::headerNameRect() const
{
    // Must reconstruct the exact same rect draw::sectionHeader() computes for
    // its readout text -- this is the click/popup-anchor hit-test for the
    // quick-swap widget painted in that same spot (paintSampleSwapper()), so
    // it shares the metrics constants rather than its own copy (iteration 3
    // restyle: iteration 2's fix here just relocated the duplication when the
    // header height changed, it didn't remove it).
    auto header = getLocalBounds().removeFromTop (metrics::sectionHeaderHeight);
    header.removeFromTop (metrics::sectionHeaderTopInset);
    auto text = header.reduced (metrics::sectionHeaderLeftInset, 0);
    const int w = juce::jlimit (80, 240, juce::roundToInt (text.getWidth() * 0.62f));
    return text.removeFromRight (w);
}

bool OscStrip::sampleSwapAvailable() const
{
    const auto f = processor.getSampleFile (slot);
    if (f == juce::File())
        return false;
    const auto root = library::findLibraryRoot();
    return root.isDirectory() && f.isAChildOf (root);
}

void OscStrip::paintSampleSwapper (juce::Graphics& g)
{
    const auto& t = currentTheme();
    const bool avail = sampleSwapAvailable();

    auto nameArea = headerNameRect();
    const auto caretArea = avail ? nameArea.removeFromRight (12) : juce::Rectangle<int>();

    const auto stringWidth = [] (const juce::String& s)
    {
        juce::GlyphArrangement ga;
        ga.addLineOfText (metrics::labelFont(), s, 0.0f, 0.0f);
        return (int) std::ceil (ga.getBoundingBox (0, -1, true).getWidth());
    };
    auto fitted = contentName();
    if (stringWidth (fitted) > nameArea.getWidth())
    {
        while (fitted.length() > 4 && stringWidth (fitted + "...") > nameArea.getWidth())
            fitted = fitted.dropLastCharacters (1).trimEnd();
        fitted += "...";
    }

    g.setFont (metrics::labelFont());
    g.setColour (t.textSecondary);
    g.drawText (fitted, nameArea, juce::Justification::centredRight);

    if (! avail)
        return;

    // Caret: the name opens the in-pack dropdown.
    auto c = caretArea.toFloat().withSizeKeepingCentre (7.0f, 4.0f);
    juce::Path caret;
    caret.addTriangle (c.getX(), c.getY(), c.getRight(), c.getY(), c.getCentreX(), c.getBottom());
    g.setColour (t.textSecondary);
    g.fillPath (caret);
}

juce::Rectangle<int> OscStrip::headerTitleRect() const
{
    // The whole header band minus whatever the quick-swap widget claims on
    // its right (headerNameRect(), only actually painted in sample/granular
    // modes) -- i.e. the "Oscillator A" title area itself, never a knob or
    // the waveform display below it.
    auto header = getLocalBounds().removeFromTop (metrics::sectionHeaderHeight);
    const auto m = currentMode();
    if (m == params::OscMode::sample || m == params::OscMode::granular)
        header.removeFromRight (headerNameRect().getWidth());
    return header;
}

void OscStrip::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        // A plain PopupMenu here would also propagate up to
        // ContentComponent::mouseDown (every mouseDown does, via its
        // addMouseListener(this, true)) -- harmless, since this component
        // carries no "paramID" property, so that handler no-ops. This is
        // OUR menu, not MIDI Learn's, so it must go through the anchored
        // helper itself (see AssignOverlay.h's declaration comment) or it
        // will flash and close with no keyboard focus target to anchor to.
        if (headerTitleRect().contains (e.getPosition()))
            openCopySwapMenu();
        return;
    }

    const auto m = currentMode();
    if ((m != params::OscMode::sample && m != params::OscMode::granular)
        || ! sampleSwapAvailable())
        return;

    if (headerNameRect().contains (e.getPosition()))
        openSampleMenu();
}

void OscStrip::openCopySwapMenu()
{
    // The other two slots, named by letter, whatever slot this is.
    juce::Array<int> others;
    for (int s = 0; s < params::numOscSlots; ++s)
        if (s != slot)
            others.add (s);

    juce::Component::SafePointer<OscStrip> safe (this);
    juce::PopupMenu menu;
    for (auto other : others)
        menu.addItem ("Copy to Oscillator " + params::id::oscSlotLetter (other),
                      [safe, other]
                      { if (safe != nullptr) safe->processor.copyOscSlot (safe->slot, other); });
    menu.addSeparator();
    for (auto other : others)
        menu.addItem ("Swap with Oscillator " + params::id::oscSlotLetter (other),
                      [safe, other]
                      { if (safe != nullptr) safe->processor.swapOscSlots (safe->slot, other); });

    showPopupAnchored (*this, menu,
        juce::PopupMenu::Options().withTargetScreenArea (headerTitleRect() + getScreenPosition()),
        nullptr);
}

void OscStrip::openSampleMenu()
{
    const auto sibs = processor.getPackSiblings (slot);
    if (sibs.isEmpty())
        return;

    const auto cur = processor.getSampleFile (slot);
    juce::PopupMenu menu;
    for (int i = 0; i < sibs.size(); ++i)
        menu.addItem (i + 1, sibs[i].getFileNameWithoutExtension(),
                      true, sibs[i] == cur);   // tick the current sample

    juce::Component::SafePointer<OscStrip> safe (this);
    menu.showMenuAsync (
        juce::PopupMenu::Options().withTargetScreenArea (headerNameRect() + getScreenPosition()),
        [safe, sibs] (int r)
        {
            if (safe != nullptr && r > 0)
                safe->processor.loadSampleFromFile (safe->slot, sibs[r - 1]);
        });
}

void OscStrip::resized()
{
    auto area = getLocalBounds().withTrimmedTop (metrics::sectionHeaderHeight).reduced (7, 3);

    display.setBounds (area.removeFromTop (86));
    area.removeFromTop (4);

    auto controlRow = area.removeFromTop (22);
    enable.setBounds (controlRow.removeFromLeft (52));
    controlRow.removeFromLeft (4);
    factoryButton.setBounds (controlRow.removeFromRight (44));
    loadButton.setBounds (controlRow.removeFromRight (52).reduced (2, 0));
    mode.setBounds (controlRow.reduced (2, 0));

    // Mode-specific extras (combo/toggles) share a slim row.
    auto extraRow = area.removeFromTop (22);
    const auto m = currentMode();
    if (m == params::OscMode::wavetable)
    {
        // Table menu on the left, phase-reset mode on the right -- the row
        // was previously half-empty here (phaseMode only took the left half).
        table->setBounds (extraRow.removeFromLeft (extraRow.getWidth() / 2).reduced (2, 1));
        phaseMode->setBounds (extraRow.reduced (2, 1));
    }
    else if (m == params::OscMode::sample)
    {
        // SYNC only shows while LOOP is on -- when it's off, give LOOP/KEY
        // the row instead of leaving dead space where SYNC was. The
        // time-signature dropdown only shows once SYNC is ALSO on, taking
        // the remaining width; when SYNC is on but timeSig is hidden
        // (SYNC off), that space just stays empty -- the row lays out from
        // sync->isVisible() either way.
        if (sync->isVisible())
        {
            loop->setBounds (extraRow.removeFromLeft (70));
            keytrackSample->setBounds (extraRow.removeFromLeft (70));
            sync->setBounds (extraRow.removeFromLeft (60));
            if (timeSig->isVisible())
            {
                extraRow.removeFromLeft (4);
                timeSig->setBounds (extraRow);
            }
        }
        else
        {
            loop->setBounds (extraRow.removeFromLeft (extraRow.getWidth() / 2));
            keytrackSample->setBounds (extraRow);
        }
    }
    else if (m == params::OscMode::granular)
        keytrackGranular->setBounds (extraRow.removeFromLeft (70));
    else if (m == params::OscMode::analog)
        analogShape->setBounds (extraRow.removeFromLeft (extraRow.getWidth() / 2).reduced (2, 1));
    else if (m == params::OscMode::noise)
        noiseColor->setBounds (extraRow.removeFromLeft (extraRow.getWidth() / 2).reduced (2, 1));

    // Knobs: mode-specific first, then common, wrapped in rows of 5.
    std::vector<Knob*> visibleKnobs;
    const auto collect = [&visibleKnobs] (std::vector<std::unique_ptr<Knob>>& set)
    {
        for (auto& k : set)
            if (k->isVisible())
                visibleKnobs.push_back (k.get());
    };
    collect (wavetableKnobs);
    collect (sampleKnobs);
    collect (granularKnobs);
    collect (analogKnobs);
    collect (fmKnobs);
    collect (pluckKnobs);
    collect (commonKnobs);

    constexpr int columns = 5;
    const auto cellW = area.getWidth() / columns;
    const auto rows = ((int) visibleKnobs.size() + columns - 1) / columns;
    const auto cellH = juce::jmin (66, area.getHeight() / juce::jmax (1, rows));

    for (size_t i = 0; i < visibleKnobs.size(); ++i)
    {
        const auto col = (int) i % columns;
        const auto row = (int) i / columns;
        visibleKnobs[i]->setBounds (area.getX() + col * cellW,
                                    area.getY() + row * cellH, cellW, cellH);
    }
}

// ============================= FilterPanel =================================

FilterPanel::FilterPanel (SPASynthProcessor& p, int filterIndex)
    : index (filterIndex),
      display (p, filterIndex),
      type (p.getAPVTS(), filterIndex == 1 ? id::filter1Type : id::filter2Type),
      cutoff (p.getAPVTS(), filterIndex == 1 ? id::filter1Cutoff : id::filter2Cutoff, "CUTOFF"),
      resonance (p.getAPVTS(), filterIndex == 1 ? id::filter1Resonance : id::filter2Resonance, "RES"),
      drive (p.getAPVTS(), filterIndex == 1 ? id::filter1Drive : id::filter2Drive, "DRIVE"),
      keytrack (p.getAPVTS(), filterIndex == 1 ? id::filter1Keytrack : id::filter2Keytrack, "KEYTRK"),
      envAmount (p.getAPVTS(), filterIndex == 1 ? id::filter1EnvAmount : id::filter2EnvAmount, "ENV 2", true),
      mix (p.getAPVTS(), filterIndex == 1 ? id::filter1Mix : id::filter2Mix, "MIX")
{
    addAndMakeVisible (display);
    addAndMakeVisible (type);
    addAndMakeVisible (cutoff);
    addAndMakeVisible (resonance);
    addAndMakeVisible (drive);
    addAndMakeVisible (keytrack);
    addAndMakeVisible (envAmount);
    addAndMakeVisible (mix);

    enable = std::make_unique<Toggle> (p.getAPVTS(),
                                       index == 1 ? id::filter1Enable : id::filter2Enable, "ON");
    addAndMakeVisible (*enable);

    powerTracker = std::make_unique<TabEngagementTracker> (p.getAPVTS(),
        std::vector<std::pair<juce::String, std::vector<juce::String>>> {
            { "on", { index == 1 ? id::filter1Enable : id::filter2Enable } } }, *this);

    if (index == 2)
    {
        routing = std::make_unique<Choice> (p.getAPVTS(), id::filterRouting);
        addAndMakeVisible (*routing);
    }
}

void FilterPanel::paint (juce::Graphics& g)
{
    draw::panel (g, getLocalBounds().toFloat());
    // recess=false: FilterPanel is always embedded inside filterTabs, whose
    // FILTER 1/FILTER 2 tab strip already casts the recessed-channel shadow
    // (and rule) above this header -- a second rule/shadow here would stack
    // two recessed tiers, so the FILTER 1/2 band is title-only.
    draw::sectionHeader (g, getLocalBounds(),
                         index == 1 ? "Filter 1" : "Filter 2", {},
                         draw::moduleHeaderColour (currentTheme(), powerTracker->isEngaged ("on")),
                         false);
}

void FilterPanel::resized()
{
    auto area = getLocalBounds().withTrimmedTop (metrics::sectionHeaderHeight).reduced (7, 3);
    display.setBounds (area.removeFromTop (index == 2 ? 64 : 86));
    area.removeFromTop (4);

    {
        auto enableRow = area.removeFromTop (22);
        enable->setBounds (enableRow.removeFromLeft (52));
        if (routing != nullptr)
        {
            enableRow.removeFromLeft (4);
            routing->setBounds (enableRow.reduced (0, 1));
        }
        area.removeFromTop (2);
    }

    type.setBounds (area.removeFromTop (22).reduced (2, 0));

    const auto cellW = area.getWidth() / 3;
    const auto rowH = juce::jmin (72, area.getHeight() / 2);

    auto row1 = area.removeFromTop (rowH);
    cutoff.setBounds (row1.removeFromLeft (cellW));
    resonance.setBounds (row1.removeFromLeft (cellW));
    drive.setBounds (row1);

    auto row2 = area.removeFromTop (rowH);
    keytrack.setBounds (row2.removeFromLeft (cellW));
    envAmount.setBounds (row2.removeFromLeft (cellW));
    mix.setBounds (row2);
}

// =============================== EnvPanel ==================================

EnvPanel::EnvPanel (SPASynthProcessor& p, const juce::String& idPrefix, int envIndex)
    : display (p, idPrefix, envIndex),
      attack (p.getAPVTS(), idPrefix + ".attack", "ATTACK", true),
      decay (p.getAPVTS(), idPrefix + ".decay", "DECAY", true),
      sustain (p.getAPVTS(), idPrefix + ".sustain", "SUSTAIN", true),
      release (p.getAPVTS(), idPrefix + ".release", "RELEASE", true)
{
    addAndMakeVisible (display);
    addAndMakeVisible (attack);
    addAndMakeVisible (decay);
    addAndMakeVisible (sustain);
    addAndMakeVisible (release);
}

void EnvPanel::resized()
{
    auto area = getLocalBounds().reduced (4, 2);
    auto knobRow = area.removeFromBottom (juce::jmin (64, area.getHeight() / 2));
    display.setBounds (area.reduced (0, 2));

    const auto cellW = knobRow.getWidth() / 4;
    attack.setBounds (knobRow.removeFromLeft (cellW));
    decay.setBounds (knobRow.removeFromLeft (cellW));
    sustain.setBounds (knobRow.removeFromLeft (cellW));
    release.setBounds (knobRow);
}

// =============================== LFOPanel ==================================

LFOPanel::LFOPanel (SPASynthProcessor& p, int lfoIndex)
    : display (p, lfoIndex),
      shape (p.getAPVTS(), id::lfoParam (lfoIndex, id::lfo::shape)),
      division (p.getAPVTS(), id::lfoParam (lfoIndex, id::lfo::division)),
      rate (p.getAPVTS(), id::lfoParam (lfoIndex, id::lfo::rate), "RATE", true),
      phase (p.getAPVTS(), id::lfoParam (lfoIndex, id::lfo::phase), "PHASE", true),
      smooth (p.getAPVTS(), id::lfoParam (lfoIndex, id::lfo::smooth), "SMOOTH", true),
      jitter (p.getAPVTS(), id::lfoParam (lfoIndex, id::lfo::jitter), "JITTER", true),
      sync (p.getAPVTS(), id::lfoParam (lfoIndex, id::lfo::sync), "SYNC"),
      retrig (p.getAPVTS(), id::lfoParam (lfoIndex, id::lfo::retrig), "RETRIG"),
      unipolar (p.getAPVTS(), id::lfoParam (lfoIndex, id::lfo::unipolar), "UNI"),
      rateEnable (p.getAPVTS(), id::lfoParam (lfoIndex, id::lfo::sync),
                 [] (float v) { return v < 0.5f; }, { &rate }),
      divisionEnable (p.getAPVTS(), id::lfoParam (lfoIndex, id::lfo::sync),
                      [] (float v) { return v >= 0.5f; }, { &division })
{
    smooth.slider.setTooltip ("Rounds off the LFO's edges, so stepped shapes like Square "
                              "and S&H stop clicking.");
    jitter.slider.setTooltip ("Blends a random value into the shape, renewed once per LFO "
                              "cycle. Turn both this and SMOOTH up for a smooth random drift.");

    addAndMakeVisible (display);
    addAndMakeVisible (shape);
    addAndMakeVisible (division);
    addAndMakeVisible (rate);
    addAndMakeVisible (phase);
    addAndMakeVisible (smooth);
    addAndMakeVisible (jitter);
    addAndMakeVisible (sync);
    addAndMakeVisible (retrig);
    addAndMakeVisible (unipolar);
}

void LFOPanel::resized()
{
    auto area = getLocalBounds().reduced (4, 2);

    auto knobRow = area.removeFromBottom (juce::jmin (64, area.getHeight() / 2));
    auto toggles = area.removeFromBottom (20);
    display.setBounds (area.reduced (0, 2));

    sync.setBounds (toggles.removeFromLeft (62));
    retrig.setBounds (toggles.removeFromLeft (72));
    unipolar.setBounds (toggles.removeFromLeft (56));

    // Five cells: RATE, PHASE, SMOOTH, JITTER, then a fifth cell holding
    // SHAPE stacked above DIVISION (each combo keeps its existing height;
    // stacking uses the knob row's spare vertical space instead of giving
    // the combos their own inline cells). The combo cell is a fixed width,
    // not an even fifth -- an even split leaves the combos too narrow to
    // show their longest entries ("Triangle", "1/16T") at their existing
    // 22px height (see comboTextFitsCellTest); 92px leaves ~59px of real
    // text room, comfortably more than "Triangle"'s ~37px. The four knob
    // cells then split whatever's left evenly.
    constexpr int comboCellW = 92;
    auto comboCell = knobRow.removeFromRight (comboCellW);
    const auto cellW = knobRow.getWidth() / 4;
    rate.setBounds (knobRow.removeFromLeft (cellW));
    phase.setBounds (knobRow.removeFromLeft (cellW));
    smooth.setBounds (knobRow.removeFromLeft (cellW));
    jitter.setBounds (knobRow);

    const auto comboW = comboCell.getWidth() - 2;
    auto shapeHalf = comboCell.removeFromTop (comboCell.getHeight() / 2);
    auto divisionHalf = comboCell;
    shape.setBounds (shapeHalf.withSizeKeepingCentre (comboW, 22));
    division.setBounds (divisionHalf.withSizeKeepingCentre (comboW, 22));
}

// ============================== MacroPanel =================================

namespace
{
    // One sentence, used both as the panel caption and as every macro
    // knob's tooltip, so the two can never drift apart.
    const char* const macroExplanation =
        "A macro is a hands-on control. Route it to anything in the mod matrix, "
        "then move it yourself: from this knob, from your host's automation, or "
        "from a MIDI controller.";
}

MacroPanel::MacroPanel (juce::AudioProcessorValueTreeState& apvts)
{
    caption.setText (macroExplanation, juce::dontSendNotification);
    caption.setFont (metrics::smallFont());
    caption.setJustificationType (juce::Justification::topLeft);
    caption.setColour (juce::Label::textColourId, currentTheme().textSecondary);
    caption.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (caption);

    for (int m = 0; m < params::numMacros; ++m)
    {
        auto knob = std::make_unique<Knob> (apvts, id::macro (m),
                                            "MACRO " + juce::String (m + 1), true);
        knob->slider.setTooltip (macroExplanation);

        // ASSIGN-mode source target. The property goes on the SLIDER, not on
        // the Knob wrapper, so the component the overlay collects is a rotary
        // juce::Slider and its paint() takes the ring-halo path every other
        // knob in the UI gets (a wrapper is a plain Component, so it would
        // fall through to the rectangular halo around the whole knob cell).
        // This relies on AssignOverlay::rebuildTargets checking "modSource"
        // BEFORE "paramID" -- every Knob stamps "paramID" on its slider for
        // MIDI Learn, and the macros are the only control carrying both.
        knob->slider.getProperties().set ("modSource", (int) params::ModSource::macro1 + m);

        addAndMakeVisible (*knob);
        knobs[(size_t) m] = std::move (knob);
    }
}

void MacroPanel::resized()
{
    auto area = getLocalBounds().reduced (4, 2);

    // Caption first, knobs in what's left -- same "labels reserve their
    // height before the rest of the panel is laid out" order the FX panels
    // use (see fxPanelLabelClippingTest), so a narrow window clips nothing.
    caption.setBounds (area.removeFromTop (juce::jmin (34, area.getHeight() / 3)));

    // The four knobs get an even quarter each. The band's height is capped
    // so the rings stay the size of every other knob in the UI instead of
    // bloating to fill this page, which has far fewer controls on it than
    // its LFO neighbours.
    auto row = area.withSizeKeepingCentre (area.getWidth(), juce::jmin (area.getHeight(), 78));
    const auto cellW = row.getWidth() / params::numMacros;
    for (int m = 0; m < params::numMacros; ++m)
        knobs[(size_t) m]->setBounds (m == params::numMacros - 1 ? row
                                                                 : row.removeFromLeft (cellW));
}

// ============================== ChaosPanel =================================

ChaosPanel::ChaosPanel (SPASynthProcessor& p)
    : apvts (p.getAPVTS()),
      display (p),
      enable (p.getAPVTS(), id::chaos::enable, "ON"),
      depth (p.getAPVTS(), id::chaos::depth, "DEPTH", true),
      rate (p.getAPVTS(), id::chaos::rate, "RATE", true),
      mix (p.getAPVTS(), id::chaos::mix, "MIX", true),
      sync (p.getAPVTS(), id::chaos::syncToBpm, "SYNC"),
      division (p.getAPVTS(), id::chaos::division),
      rateEnable (p.getAPVTS(), id::chaos::syncToBpm,
                 [] (float v) { return v < 0.5f; }, { &rate }),
      divisionEnable (p.getAPVTS(), id::chaos::syncToBpm,
                      [] (float v) { return v >= 0.5f; }, { &division }),
      syncTracker (p.getAPVTS(), { { "sync", { id::chaos::syncToBpm } } }, *this),
      powerTracker (p.getAPVTS(), { { "on", { id::chaos::enable } } }, *this)
{
    addAndMakeVisible (display);
    addAndMakeVisible (enable);
    addAndMakeVisible (depth);
    addAndMakeVisible (rate);
    addAndMakeVisible (mix);
    addAndMakeVisible (sync);
    addAndMakeVisible (division);

    divisionLabel.setText ("RATE", juce::dontSendNotification);
    divisionLabel.setFont (metrics::smallFont());
    divisionLabel.setJustificationType (juce::Justification::centred);
    divisionLabel.setInterceptsMouseClicks (false, false);
    divisionLabel.setMinimumHorizontalScale (0.6f);
    addChildComponent (divisionLabel);

    const std::array<std::tuple<const char*, const char*, const char*>, 6> defs = { {
        { id::chaos::pitchOn, id::chaos::pitchAmount, "PITCH" },
        { id::chaos::phaseOn, id::chaos::phaseAmount, "PHASE" },
        { id::chaos::positionOn, id::chaos::positionAmount, "POS" },
        { id::chaos::ampOn, id::chaos::ampAmount, "AMP" },
        { id::chaos::satOn, id::chaos::saturation, "SAT" },
        { id::chaos::distOn, id::chaos::distortion, "DIST" },
    } };

    for (size_t i = 0; i < defs.size(); ++i)
    {
        drifts[i].on = std::make_unique<Toggle> (p.getAPVTS(), std::get<0> (defs[i]), "");
        drifts[i].amount = std::make_unique<Knob> (p.getAPVTS(), std::get<1> (defs[i]),
                                                   std::get<2> (defs[i]), true);
        addAndMakeVisible (*drifts[i].on);
        addAndMakeVisible (*drifts[i].amount);
    }

    apvts.addParameterListener (id::chaos::syncToBpm, this);
    handleAsyncUpdate();   // initial rate/division visibility, before the first layout
}

ChaosPanel::~ChaosPanel()
{
    apvts.removeParameterListener (id::chaos::syncToBpm, this);
}

void ChaosPanel::paint (juce::Graphics& g)
{
    draw::panel (g, getLocalBounds().toFloat());
    // The section renames itself while synced -- polyrhythmic-but-quantised
    // movement earns "Organized", free drift stays "Organic" (product
    // owner's call). syncTracker keeps this repainting on toggle. paint()
    // only chooses what to draw -- the rate/division visibility swap is
    // handled by handleAsyncUpdate() (see the parameterChanged/AsyncUpdater
    // pair in the header), never here.
    const auto isSynced = syncTracker.isEngaged ("sync");
    draw::sectionHeader (g, getLocalBounds(), isSynced ? "Organized Chaos" : "Organic Chaos", {},
                         draw::moduleHeaderColour (currentTheme(), powerTracker.isEngaged ("on")));
}

void ChaosPanel::handleAsyncUpdate()
{
    // Rate only means anything when free-running; division (plus its RATE
    // caption) only means anything when synced -- shown, not just enabled,
    // so the row keeps the same shape either way and the label row is never
    // stolen from underneath a knob. Same param this fires off of also
    // drives DependentEnable's enabled/disabled state on the attachments
    // themselves; visibility here is purely cosmetic on top of that.
    const auto isSynced = isSyncEngagedForTest();
    rate.setVisible (! isSynced);
    division.setVisible (isSynced);
    divisionLabel.setVisible (isSynced);
}

void ChaosPanel::resized()
{
    // Sync toggle lives at the top right of the recessed header strip, next
    // to the title it renames -- same idea as MatrixPanel's ASSIGN button.
    {
        auto headerArea = getLocalBounds().removeFromTop (metrics::sectionHeaderHeight);
        sync.setBounds (headerArea.removeFromRight (62).reduced (2, 6));
    }

    auto area = getLocalBounds().withTrimmedTop (metrics::sectionHeaderHeight).reduced (7, 3);

    auto top = area.removeFromTop (juce::jmax (78, area.getHeight() - 84));
    // Cap fixed at 130px UNCONDITIONALLY (never `top.getWidth() / 2`, and
    // never dependent on SYNC state) -- 130px is comfortably wider than the
    // ~35px "TRAJECTORY"-scale content this meter actually draws, and
    // leaves the DEPTH/RATE/MIX row enough width for the division combo to
    // show its longest entry ("1/16T") without clipping (see
    // comboTextFitsCellTest). A width that depended on SYNC would make the
    // meter and all three knobs jump sideways every time SYNC is toggled.
    auto scope = top.removeFromLeft (juce::jmin (130, top.getWidth() / 2));
    display.setBounds (scope.reduced (0, 2));

    enable.setBounds (top.removeFromLeft (50).withSizeKeepingCentre (50, 20));
    auto masters = top.withSizeKeepingCentre (top.getWidth(), juce::jmin (top.getHeight(), 72));
    const auto masterW = juce::jmax (1, masters.getWidth() / 3);
    depth.setBounds (masters.removeFromLeft (masterW));
    // Rate cell shows EITHER the rate knob (which draws its own label) OR
    // the division dropdown plus divisionLabel, occupying the same rect --
    // actual show/hide is handleAsyncUpdate()'s job, this only ever lays
    // both out so whichever one handleAsyncUpdate() has made visible is
    // already positioned correctly.
    auto rateCell = masters.removeFromLeft (masterW);
    rate.setBounds (rateCell);
    auto divisionArea = rateCell;
    divisionLabel.setBounds (divisionArea.removeFromBottom (13));
    // Was `- 6`; the combo needs the cell it has to show its longest entry
    // ("1/16T") without the text clipping under the arrow.
    division.setBounds (divisionArea.withSizeKeepingCentre (divisionArea.getWidth() - 2, 22));
    mix.setBounds (masters);

    // Drift strip: toggle above each amount knob.
    auto strip = area;
    const auto cellW = strip.getWidth() / (int) drifts.size();
    for (auto& d : drifts)
    {
        auto cell = strip.removeFromLeft (cellW);
        d.on->setBounds (cell.removeFromTop (16).withSizeKeepingCentre (28, 16));
        d.amount->setBounds (cell);
    }
}

// =============================== ArpPanel ==================================

ArpPanel::ArpPanel (juce::AudioProcessorValueTreeState& apvts)
    : enable (apvts, id::arp::enable, "ON"),
      latch (apvts, id::arp::latch, "LATCH"),
      mode (apvts, id::arp::mode),
      division (apvts, id::arp::division),
      phrase (apvts, id::arp::phrase),
      velMode (apvts, id::arp::velMode),
      octaves (apvts, id::arp::octaves, "OCTAVES", true),
      gate (apvts, id::arp::gate, "GATE", true),
      swing (apvts, id::arp::swing, "SWING", true),
      chance (apvts, id::arp::chance, "CHANCE", true),
      stutter (apvts, id::arp::stutter, "STUTTER", true),
      jump (apvts, id::arp::jump, "JUMP", true),
      humanize (apvts, id::arp::humanize, "HUMAN", true),
      powerTracker (apvts, { { "on", { id::arp::enable } } }, *this)
{
    addAndMakeVisible (enable);
    addAndMakeVisible (latch);
    addAndMakeVisible (mode);
    addAndMakeVisible (division);
    addAndMakeVisible (phrase);
    addAndMakeVisible (velMode);
    addAndMakeVisible (octaves);
    addAndMakeVisible (gate);
    addAndMakeVisible (swing);
    addAndMakeVisible (chance);
    addAndMakeVisible (stutter);
    addAndMakeVisible (jump);
    addAndMakeVisible (humanize);
}

void ArpPanel::paint (juce::Graphics& g)
{
    draw::panel (g, getLocalBounds().toFloat());
    draw::sectionHeader (g, getLocalBounds(), "Arpeggiator", {},
                         draw::moduleHeaderColour (currentTheme(), powerTracker.isEngaged ("on")));
}

void ArpPanel::resized()
{
    auto area = getLocalBounds().withTrimmedTop (metrics::sectionHeaderHeight).reduced (7, 3);

    auto row1 = area.removeFromTop (24);
    enable.setBounds (row1.removeFromLeft (48));
    latch.setBounds (row1.removeFromLeft (62));
    row1.removeFromLeft (2);
    mode.setBounds (row1.removeFromLeft ((row1.getWidth() - 4) * 58 / 100).reduced (0, 1));
    row1.removeFromLeft (4);
    division.setBounds (row1.reduced (0, 1));

    area.removeFromTop (4);
    auto row2 = area.removeFromTop (24);
    phrase.setBounds (row2.removeFromLeft ((row2.getWidth() - 4) * 58 / 100).reduced (0, 1));
    row2.removeFromLeft (4);
    velMode.setBounds (row2.reduced (0, 1));

    area.removeFromTop (2);
    const auto knobH = juce::jmin (area.getHeight() / 2, 64);

    auto knobRow = area.removeFromTop (knobH);
    const auto cellW = knobRow.getWidth() / 3;
    octaves.setBounds (knobRow.removeFromLeft (cellW));
    gate.setBounds (knobRow.removeFromLeft (cellW));
    swing.setBounds (knobRow);

    auto chanceRow = area.withHeight (knobH);
    const auto chanceW = chanceRow.getWidth() / 4;
    chance.setBounds (chanceRow.removeFromLeft (chanceW));
    stutter.setBounds (chanceRow.removeFromLeft (chanceW));
    jump.setBounds (chanceRow.removeFromLeft (chanceW));
    humanize.setBounds (chanceRow);
}

// =============================== FXPanel ===================================

FXPanel::FXPanel (juce::AudioProcessorValueTreeState& apvts, FXDisplay::Kind kind,
                  params::Section section, const juce::String& title,
                  const juce::StringArray& enableParamIds)
    : panelTitle (title),
      display (apvts, kind),
      controls (apvts, section, title, {}, false)
{
    addAndMakeVisible (display);
    addAndMakeVisible (controls);

    // Delay time only means anything free-running; division only means
    // anything synced -- same rule as the LFO panels, wired post-hoc here
    // since SectionPanel auto-builds its grid with no dependency concept.
    if (section == params::Section::fxDelay)
    {
        delayTimeEnable = std::make_unique<DependentEnable> (
            apvts, id::fx::delaySync, [] (float v) { return v < 0.5f; },
            controls.findControlComponents (id::fx::delayTime));
        delayDivisionEnable = std::make_unique<DependentEnable> (
            apvts, id::fx::delaySync, [] (float v) { return v >= 0.5f; },
            controls.findControlComponents (id::fx::delayDivision));
    }

    // House-voice tooltips for the two controls the stereo chorus engine
    // added (1.0.22): the registry-built grid gives them knobs and a combo
    // but no explanation, and WIDTH in particular needs one -- it is the
    // control that stops the chorus imaging as mono.
    if (section == params::Section::fxChorus)
    {
        auto tip = [this] (const juce::String& paramID, const juce::String& text)
        {
            for (auto* c : controls.findControlComponents (paramID))
                if (auto* t = dynamic_cast<juce::SettableTooltipClient*> (c))
                    t->setTooltip (text);
        };
        tip (id::fx::chorusWidth,
             "Spreads the left and right sides apart. At zero both sides move "
             "together and the chorus sits in the middle; turn it up and the "
             "sides sweep against each other for a wide stereo image.");
        tip (id::fx::chorusMode,
             "Vintage is the warm, slightly dark bucket-brigade sound of a "
             "classic 80s polysynth. Modern is clean and digital, with a "
             "longer, deeper sweep.");
    }

    if (! enableParamIds.isEmpty())
        powerTracker = std::make_unique<TabEngagementTracker> (apvts,
            std::vector<std::pair<juce::String, std::vector<juce::String>>> {
                { "on", std::vector<juce::String> (enableParamIds.begin(), enableParamIds.end()) } },
            *this);
}

void FXPanel::paint (juce::Graphics& g)
{
    draw::panel (g, getLocalBounds().toFloat());
    // recess=false: every FXPanel is embedded inside fxTabs (DIST/CHORUS/...
    // strip), which already casts the recessed-channel shadow (and rule)
    // above this header -- a second rule/shadow here would stack two
    // recessed tiers, so the DISTORTION/etc. band is title-only. The
    // embedded `controls` (SectionPanel, drawFrame=false) draws no header of
    // its own, so this stays the only header painted per FX tab.
    draw::sectionHeader (g, getLocalBounds(), panelTitle, {},
                         draw::moduleHeaderColour (currentTheme(),
                             powerTracker == nullptr || powerTracker->isEngaged ("on")),
                         false);
}

void FXPanel::resized()
{
    auto area = getLocalBounds().withTrimmedTop (metrics::sectionHeaderHeight).reduced (7, 3);

    // Layout priority: caption labels must never clip, so the control grid
    // always gets the FULL height its rows need (heightForWidth) -- never
    // capped down. The scope/display shrinks into whatever remains, down to
    // nothing if the panel is that short (Mike's call: visualizers may
    // shrink, captions never do). This used to cap controlsH at
    // area.getHeight()-44 to guarantee the display a minimum, which at base
    // size squeezed TREM/VIB's two-row grid short enough that its bottom
    // row's labels rendered partially off the bottom of the panel.
    const auto controlsNeeded = controls.heightForWidth (area.getWidth());
    const auto controlsH = juce::jmin (controlsNeeded, area.getHeight());
    controls.setBounds (area.removeFromBottom (controlsH));
    if (area.getHeight() > 4)
    {
        area.removeFromBottom (4);
        display.setBounds (area);
    }
    else
    {
        display.setBounds ({});
    }
}

} // namespace spa::ui
