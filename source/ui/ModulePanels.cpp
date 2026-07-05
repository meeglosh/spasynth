#include "ModulePanels.h"
#include "../ArsenalProcessor.h"
#include "../library/Library.h"

namespace arsenal::ui
{

namespace id = params::id;

// ============================== OscStrip ===================================

OscStrip::OscStrip (ArsenalProcessor& p, int slotIndex)
    : processor (p), slot (slotIndex),
      display (p, slotIndex),
      enable (p.getAPVTS(), id::oscSlot (slotIndex, id::osc::enable), "ON"),
      mode (p.getAPVTS(), id::oscSlot (slotIndex, id::osc::mode))
{
    auto& apvts = processor.getAPVTS();
    const auto pid = [this] (const char* key) { return id::oscSlot (slot, key); };

    addAndMakeVisible (display);
    addAndMakeVisible (enable);
    addAndMakeVisible (mode);

    loadButton.onClick = [this] { chooseContent(); };
    addAndMakeVisible (loadButton);
    factoryButton.onClick = [this] { processor.setFactoryWavetable (slot); };
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

    sampleKnobs.push_back (knob (pid (id::osc::sampleStart), "START"));
    sampleKnobs.push_back (knob (pid (id::osc::loopStart), "LOOP ST"));
    sampleKnobs.push_back (knob (pid (id::osc::loopEnd), "LOOP END"));
    sampleKnobs.push_back (knob (pid (id::osc::rootNote), "ROOT"));
    loop = std::make_unique<Toggle> (apvts, pid (id::osc::loop), "LOOP");
    keytrackSample = std::make_unique<Toggle> (apvts, pid (id::osc::keytrack), "KEY");

    granularKnobs.push_back (knob (pid (id::osc::grainSize), "SIZE"));
    granularKnobs.push_back (knob (pid (id::osc::grainDensity), "DENSITY"));
    granularKnobs.push_back (knob (pid (id::osc::grainPos), "POSITION"));
    granularKnobs.push_back (knob (pid (id::osc::grainSpray), "SPRAY"));
    granularKnobs.push_back (knob (pid (id::osc::grainPitch), "PITCH"));
    granularKnobs.push_back (knob (pid (id::osc::rootNote), "ROOT"));
    keytrackGranular = std::make_unique<Toggle> (apvts, pid (id::osc::keytrack), "KEY");

    analogKnobs.push_back (knob (pid (id::osc::pulseWidth), "PW"));
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
    addChildComponent (*loop);
    addChildComponent (*keytrackSample);
    addChildComponent (*keytrackGranular);
    addChildComponent (*analogShape);
    addChildComponent (*noiseColor);

    apvts.addParameterListener (pid (id::osc::mode), this);
    processor.addChangeListener (this);
    handleAsyncUpdate();
}

OscStrip::~OscStrip()
{
    processor.removeChangeListener (this);
    processor.getAPVTS().removeParameterListener (id::oscSlot (slot, id::osc::mode), this);
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
            return processor.getWavetableName (slot);

        case params::OscMode::sample:
        case params::OscMode::granular:
        {
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
    loop->setVisible (m == params::OscMode::sample);
    keytrackSample->setVisible (m == params::OscMode::sample);
    keytrackGranular->setVisible (m == params::OscMode::granular);
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
    const auto libraryRoot = library::findLibraryRoot();

    fileChooser = std::make_unique<juce::FileChooser> (
        wavetableMode ? "Load wavetable" : "Load SFX / sample",
        ! wavetableMode && libraryRoot.isDirectory()
            ? libraryRoot : juce::File::getSpecialLocation (juce::File::userHomeDirectory),
        wavetableMode ? "*.wav;*.aif;*.aiff;*.flac" : "*.wav;*.aif;*.aiff;*.flac;*.mp3");

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                            | juce::FileBrowserComponent::canSelectFiles,
                              [this, wavetableMode] (const juce::FileChooser& fc)
    {
        if (! fc.getResult().existsAsFile())
            return;
        if (wavetableMode)
            processor.loadWavetableFromFile (slot, fc.getResult());
        else
            processor.loadSampleFromFile (slot, fc.getResult());
    });
}

void OscStrip::paint (juce::Graphics& g)
{
    draw::panel (g, getLocalBounds().toFloat());
    draw::sectionHeader (g, getLocalBounds(),
                         "Oscillator " + id::oscSlotLetter (slot), contentName(),
                         currentTheme().accent);
}

void OscStrip::resized()
{
    auto area = getLocalBounds().withTrimmedTop (20).reduced (7, 3);

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
        phaseMode->setBounds (extraRow.removeFromLeft (extraRow.getWidth() / 2).reduced (2, 1));
    else if (m == params::OscMode::sample)
    {
        loop->setBounds (extraRow.removeFromLeft (70));
        keytrackSample->setBounds (extraRow.removeFromLeft (70));
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

FilterPanel::FilterPanel (ArsenalProcessor& p)
    : display (p),
      type (p.getAPVTS(), id::filter1Type),
      cutoff (p.getAPVTS(), id::filter1Cutoff, "CUTOFF"),
      resonance (p.getAPVTS(), id::filter1Resonance, "RES"),
      drive (p.getAPVTS(), id::filter1Drive, "DRIVE")
{
    addAndMakeVisible (display);
    addAndMakeVisible (type);
    addAndMakeVisible (cutoff);
    addAndMakeVisible (resonance);
    addAndMakeVisible (drive);
}

void FilterPanel::paint (juce::Graphics& g)
{
    draw::panel (g, getLocalBounds().toFloat());
    draw::sectionHeader (g, getLocalBounds(), "Filter", {}, currentTheme().accent);
}

void FilterPanel::resized()
{
    auto area = getLocalBounds().withTrimmedTop (20).reduced (7, 3);
    display.setBounds (area.removeFromTop (86));
    area.removeFromTop (4);
    type.setBounds (area.removeFromTop (22).reduced (2, 0));

    const auto cellW = area.getWidth() / 3;
    auto knobRow = area.withHeight (juce::jmin (area.getHeight(), 66));
    cutoff.setBounds (knobRow.removeFromLeft (cellW));
    resonance.setBounds (knobRow.removeFromLeft (cellW));
    drive.setBounds (knobRow);
}

// =============================== EnvPanel ==================================

EnvPanel::EnvPanel (ArsenalProcessor& p, const juce::String& idPrefix, int envIndex)
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

LFOPanel::LFOPanel (ArsenalProcessor& p, int lfoIndex)
    : display (p, lfoIndex),
      shape (p.getAPVTS(), id::lfoParam (lfoIndex, id::lfo::shape)),
      division (p.getAPVTS(), id::lfoParam (lfoIndex, id::lfo::division)),
      rate (p.getAPVTS(), id::lfoParam (lfoIndex, id::lfo::rate), "RATE", true),
      phase (p.getAPVTS(), id::lfoParam (lfoIndex, id::lfo::phase), "PHASE", true),
      sync (p.getAPVTS(), id::lfoParam (lfoIndex, id::lfo::sync), "SYNC"),
      retrig (p.getAPVTS(), id::lfoParam (lfoIndex, id::lfo::retrig), "RETRIG"),
      unipolar (p.getAPVTS(), id::lfoParam (lfoIndex, id::lfo::unipolar), "UNI")
{
    addAndMakeVisible (display);
    addAndMakeVisible (shape);
    addAndMakeVisible (division);
    addAndMakeVisible (rate);
    addAndMakeVisible (phase);
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

    const auto cellW = knobRow.getWidth() / 4;
    rate.setBounds (knobRow.removeFromLeft (cellW));
    phase.setBounds (knobRow.removeFromLeft (cellW));
    shape.setBounds (knobRow.removeFromLeft (cellW).withSizeKeepingCentre (cellW - 6, 22));
    division.setBounds (knobRow.withSizeKeepingCentre (knobRow.getWidth() - 6, 22));
}

// ============================== ChaosPanel =================================

ChaosPanel::ChaosPanel (ArsenalProcessor& p)
    : display (p),
      enable (p.getAPVTS(), id::chaos::enable, "ON"),
      depth (p.getAPVTS(), id::chaos::depth, "DEPTH", true),
      rate (p.getAPVTS(), id::chaos::rate, "RATE", true),
      mix (p.getAPVTS(), id::chaos::mix, "MIX", true)
{
    addAndMakeVisible (display);
    addAndMakeVisible (enable);
    addAndMakeVisible (depth);
    addAndMakeVisible (rate);
    addAndMakeVisible (mix);

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
}

void ChaosPanel::paint (juce::Graphics& g)
{
    draw::panel (g, getLocalBounds().toFloat());
    draw::sectionHeader (g, getLocalBounds(), "Organic Chaos", {},
                         currentTheme().accentMod);
}

void ChaosPanel::resized()
{
    auto area = getLocalBounds().withTrimmedTop (20).reduced (7, 3);

    auto top = area.removeFromTop (juce::jmax (78, area.getHeight() - 84));
    auto scope = top.removeFromLeft (juce::jmin (200, top.getWidth() / 2));
    display.setBounds (scope.reduced (0, 2));

    enable.setBounds (top.removeFromLeft (50).withSizeKeepingCentre (50, 20));
    auto masters = top.withSizeKeepingCentre (top.getWidth(), juce::jmin (top.getHeight(), 72));
    const auto masterW = juce::jmax (1, masters.getWidth() / 3);
    depth.setBounds (masters.removeFromLeft (masterW));
    rate.setBounds (masters.removeFromLeft (masterW));
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
      swing (apvts, id::arp::swing, "SWING", true)
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
}

void ArpPanel::paint (juce::Graphics& g)
{
    draw::panel (g, getLocalBounds().toFloat());
    draw::sectionHeader (g, getLocalBounds(), "Arpeggiator", {},
                         currentTheme().accentMod);
}

void ArpPanel::resized()
{
    auto area = getLocalBounds().withTrimmedTop (20).reduced (7, 3);

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
    const auto cellW = area.getWidth() / 3;
    auto knobRow = area.withHeight (juce::jmin (area.getHeight(), 68));
    octaves.setBounds (knobRow.removeFromLeft (cellW));
    gate.setBounds (knobRow.removeFromLeft (cellW));
    swing.setBounds (knobRow);
}

// =============================== FXPanel ===================================

FXPanel::FXPanel (juce::AudioProcessorValueTreeState& apvts, FXDisplay::Kind kind,
                  params::Section section, const juce::String& title)
    : panelTitle (title),
      display (apvts, kind),
      controls (apvts, section, title, {}, false)
{
    addAndMakeVisible (display);
    addAndMakeVisible (controls);
}

void FXPanel::paint (juce::Graphics& g)
{
    draw::panel (g, getLocalBounds().toFloat());
    draw::sectionHeader (g, getLocalBounds(), panelTitle, {}, currentTheme().accent);
}

void FXPanel::resized()
{
    auto area = getLocalBounds().withTrimmedTop (20).reduced (7, 3);

    // Controls take exactly the height their grid needs (they wrap by
    // width); the scope gets whatever remains, with a survivable minimum.
    const auto controlsNeeded = controls.heightForWidth (area.getWidth());
    const auto controlsH = juce::jmin (controlsNeeded,
                                       juce::jmax (60, area.getHeight() - 44));
    controls.setBounds (area.removeFromBottom (controlsH));
    area.removeFromBottom (4);
    display.setBounds (area);
}

} // namespace arsenal::ui
