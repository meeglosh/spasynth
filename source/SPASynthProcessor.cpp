#include "SPASynthProcessor.h"
#include "dsp/WavetableLoader.h"
#include "params/Randomizer.h"
#include "ui/SPASynthEditor.h"
#include <cmath>
#include <mutex>

namespace spa
{

namespace
{
    constexpr const char* wavetableStateType = "WAVETABLES";
    constexpr const char* sampleStateType = "SAMPLES";
    constexpr const char* lfoCustomStateType = "LFOCUSTOM";

    juce::Identifier lfoCustomChildType (int lfoIndex)
    {
        return { "LFO" + juce::String (lfoIndex) };
    }

    // CSV-encodes a CustomLFOShape's point/curve arrays (only the first
    // `count` entries of each -- the rest of the fixed-size arrays are
    // padding and not meaningful). Human-readable in a saved .spasynth file,
    // same spirit as the rest of the state tree.
    juce::ValueTree customLfoShapeToValueTree (const dsp::CustomLFOShape& shape, int lfoIndex)
    {
        juce::ValueTree t (lfoCustomChildType (lfoIndex));
        t.setProperty ("count", shape.count, nullptr);

        juce::StringArray xs, ys, cs;
        for (int i = 0; i < shape.count; ++i)
        {
            xs.add (juce::String (shape.x[(size_t) i], 6));
            ys.add (juce::String (shape.y[(size_t) i], 6));
            if (i < shape.count - 1)
                cs.add (juce::String (shape.curve[(size_t) i], 6));
        }
        t.setProperty ("x", xs.joinIntoString (","), nullptr);
        t.setProperty ("y", ys.joinIntoString (","), nullptr);
        t.setProperty ("curve", cs.joinIntoString (","), nullptr);
        return t;
    }

    // Missing/malformed data (old preset, hand-edited file) falls back to
    // the default triangle -- CustomLFOShape's own default constructor --
    // rather than half-applying a corrupt shape.
    dsp::CustomLFOShape customLfoShapeFromValueTree (const juce::ValueTree& t)
    {
        dsp::CustomLFOShape shape;   // default triangle
        if (! t.isValid())
            return shape;

        const auto count = (int) t.getProperty ("count", 0);
        if (count < 2 || count > dsp::CustomLFOShape::maxPoints)
            return shape;

        const auto xs = juce::StringArray::fromTokens (t.getProperty ("x").toString(), ",", "");
        const auto ys = juce::StringArray::fromTokens (t.getProperty ("y").toString(), ",", "");
        const auto cs = juce::StringArray::fromTokens (t.getProperty ("curve").toString(), ",", "");
        if (xs.size() != count || ys.size() != count || cs.size() != count - 1)
            return shape;

        shape.count = count;
        for (int i = 0; i < count; ++i)
        {
            shape.x[(size_t) i] = xs[i].getFloatValue();
            shape.y[(size_t) i] = ys[i].getFloatValue();
            if (i < count - 1)
                shape.curve[(size_t) i] = cs[i].getFloatValue();
        }
        return shape;
    }

    juce::Identifier slotPathProperty (int slot)
    {
        return { "slot" + juce::String (slot) };
    }

    const juce::Identifier wildnessProperty { "randomWildness" };
    const juce::Identifier lockMaskProperty { "randomLockMask" };

    // Machine/window UI properties that must NOT change when a preset loads
    // (a preset is a sound, not a window layout) but DO apply on a real host
    // session restore. Listed once so buildStateTree/restoreStateTree agree.
    const juce::Identifier uiScaleProperty { "uiScale" };
    const juce::Identifier uiKeyboardOctaveProperty { "uiKeyboardOctave" };
    const juce::Identifier uiKeyboardVisibleProperty { "uiKeyboardVisible" };

    // AudioProcessorValueTreeState's own PARAM/id/value identifiers -- not
    // exposed publicly, but stable (visible in every saved .spasynth file:
    // <PARAM id="..." value="..."/>). Used to fill in any parameter the
    // incoming state is missing with its registry default, so a param that
    // predates a preset (or is simply absent from a host chunk) resets
    // rather than keeping whatever the previous state had.
    const juce::Identifier paramValueType { "PARAM" };
    const juce::Identifier paramIdProperty { "id" };
    const juce::Identifier paramValueProperty { "value" };

    void fillMissingParamsWithDefaults (juce::ValueTree& state)
    {
        juce::StringArray present;
        for (int i = 0; i < state.getNumChildren(); ++i)
        {
            const auto child = state.getChild (i);
            if (child.hasType (paramValueType))
                present.add (child.getProperty (paramIdProperty).toString());
        }

        for (const auto& def : spa::params::all())
        {
            if (present.contains (def.id))
                continue;

            juce::ValueTree missing (paramValueType);
            missing.setProperty (paramIdProperty, def.id, nullptr);
            missing.setProperty (paramValueProperty, (double) def.defaultValue, nullptr);
            state.appendChild (missing, nullptr);
        }
    }
}

SPASynthProcessor::SPASynthProcessor()
    : juce::AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", params::createLayout())
{
    raw.masterGain = apvts.getRawParameterValue (params::id::masterGain);
    raw.glideMode = apvts.getRawParameterValue (params::id::glideMode);
    raw.glideTime = apvts.getRawParameterValue (params::id::glideTime);
    raw.voiceMode = apvts.getRawParameterValue (params::id::voiceMode);
    raw.notePriority = apvts.getRawParameterValue (params::id::notePriority);
    raw.unisonVoices = apvts.getRawParameterValue (params::id::unisonVoices);
    raw.unisonDetune = apvts.getRawParameterValue (params::id::unisonDetune);
    raw.unisonWidth = apvts.getRawParameterValue (params::id::unisonWidth);
    raw.ampAttack = apvts.getRawParameterValue (params::id::ampAttack);
    raw.ampDecay = apvts.getRawParameterValue (params::id::ampDecay);
    raw.ampSustain = apvts.getRawParameterValue (params::id::ampSustain);
    raw.ampRelease = apvts.getRawParameterValue (params::id::ampRelease);
    raw.oversampling = apvts.getRawParameterValue (params::id::oversampling);
    raw.timeSig = apvts.getRawParameterValue (params::id::timeSig);
    raw.filter1Enable = apvts.getRawParameterValue (params::id::filter1Enable);
    raw.filterType = apvts.getRawParameterValue (params::id::filter1Type);
    raw.filterKeytrack = apvts.getRawParameterValue (params::id::filter1Keytrack);
    raw.filter2Enable = apvts.getRawParameterValue (params::id::filter2Enable);
    raw.filter2Type = apvts.getRawParameterValue (params::id::filter2Type);
    raw.filter2Keytrack = apvts.getRawParameterValue (params::id::filter2Keytrack);
    raw.filterRouting = apvts.getRawParameterValue (params::id::filterRouting);

    for (int s = 0; s < params::numOscSlots; ++s)
    {
        auto& rs = raw.slots[(size_t) s];
        const auto pid = [s] (const char* key) { return params::id::oscSlot (s, key); };
        rs.enable      = apvts.getRawParameterValue (pid (params::id::osc::enable));
        rs.mode        = apvts.getRawParameterValue (pid (params::id::osc::mode));
        rs.phase       = apvts.getRawParameterValue (pid (params::id::osc::phase));
        rs.phaseMode   = apvts.getRawParameterValue (pid (params::id::osc::phaseMode));
        rs.unisonCount = apvts.getRawParameterValue (pid (params::id::osc::unisonCount));
        rs.sampleStart = apvts.getRawParameterValue (pid (params::id::osc::sampleStart));
        rs.loop        = apvts.getRawParameterValue (pid (params::id::osc::loop));
        rs.loopStart   = apvts.getRawParameterValue (pid (params::id::osc::loopStart));
        rs.loopEnd     = apvts.getRawParameterValue (pid (params::id::osc::loopEnd));
        rs.loopXfade   = apvts.getRawParameterValue (pid (params::id::osc::loopXfade));
        rs.keytrack    = apvts.getRawParameterValue (pid (params::id::osc::keytrack));
        rs.rootNote    = apvts.getRawParameterValue (pid (params::id::osc::rootNote));
        rs.syncToBpm         = apvts.getRawParameterValue (pid (params::id::osc::syncToBpm));
        rs.syncBeatsOverride = apvts.getRawParameterValue (pid (params::id::osc::syncBeatsOverride));
        rs.timeSig           = apvts.getRawParameterValue (pid (params::id::osc::timeSig));
        rs.grainPitch  = apvts.getRawParameterValue (pid (params::id::osc::grainPitch));
        rs.analogShape = apvts.getRawParameterValue (pid (params::id::osc::analogShape));
        rs.sub         = apvts.getRawParameterValue (pid (params::id::osc::sub));
        rs.fmRatio     = apvts.getRawParameterValue (pid (params::id::osc::fmRatio));
        rs.noiseColor  = apvts.getRawParameterValue (pid (params::id::osc::noiseColor));
        rs.filterRoute = apvts.getRawParameterValue (pid (params::id::osc::filterRoute));

    }

    // Edited/dirty indicator (1.0.26): one listener registration per
    // parameter so parameterChanged() sees every one of them (osc mode/table
    // included -- no separate registration needed for those any more, since
    // this single generic pass now covers the whole layout).
    for (auto* p : getParameters())
        if (auto* withID = dynamic_cast<juce::AudioProcessorParameterWithID*> (p))
            apvts.addParameterListener (withID->paramID, this);

    for (int i = 0; i < params::numLFOs; ++i)
    {
        auto& rl = raw.lfos[(size_t) i];
        const auto pid = [i] (const char* key) { return params::id::lfoParam (i, key); };
        rl.shape    = apvts.getRawParameterValue (pid (params::id::lfo::shape));
        rl.rate     = apvts.getRawParameterValue (pid (params::id::lfo::rate));
        rl.sync     = apvts.getRawParameterValue (pid (params::id::lfo::sync));
        rl.division = apvts.getRawParameterValue (pid (params::id::lfo::division));
        rl.phase    = apvts.getRawParameterValue (pid (params::id::lfo::phase));
        rl.retrig   = apvts.getRawParameterValue (pid (params::id::lfo::retrig));
        rl.unipolar = apvts.getRawParameterValue (pid (params::id::lfo::unipolar));
        rl.smooth   = apvts.getRawParameterValue (pid (params::id::lfo::smooth));
        rl.jitter   = apvts.getRawParameterValue (pid (params::id::lfo::jitter));
    }

    for (int m = 0; m < params::numMacros; ++m)
        raw.macros[(size_t) m] = apvts.getRawParameterValue (params::id::macro (m));

    for (int r = 0; r < params::numModRoutes; ++r)
    {
        auto& rr = raw.routes[(size_t) r];
        rr.source = apvts.getRawParameterValue (params::id::routeParam (r, params::id::route::source));
        rr.dest   = apvts.getRawParameterValue (params::id::routeParam (r, params::id::route::dest));
        rr.depth  = apvts.getRawParameterValue (params::id::routeParam (r, params::id::route::depth));
    }

    {
        namespace ch = params::id::chaos;
        raw.chaos.enable         = apvts.getRawParameterValue (ch::enable);
        raw.chaos.pitchOn        = apvts.getRawParameterValue (ch::pitchOn);
        raw.chaos.pitchAmount    = apvts.getRawParameterValue (ch::pitchAmount);
        raw.chaos.phaseOn        = apvts.getRawParameterValue (ch::phaseOn);
        raw.chaos.phaseAmount    = apvts.getRawParameterValue (ch::phaseAmount);
        raw.chaos.positionOn     = apvts.getRawParameterValue (ch::positionOn);
        raw.chaos.positionAmount = apvts.getRawParameterValue (ch::positionAmount);
        raw.chaos.ampOn          = apvts.getRawParameterValue (ch::ampOn);
        raw.chaos.ampAmount      = apvts.getRawParameterValue (ch::ampAmount);
        raw.chaos.satOn          = apvts.getRawParameterValue (ch::satOn);
        raw.chaos.saturation     = apvts.getRawParameterValue (ch::saturation);
        raw.chaos.distOn         = apvts.getRawParameterValue (ch::distOn);
        raw.chaos.distortion     = apvts.getRawParameterValue (ch::distortion);
        raw.chaos.syncToBpm      = apvts.getRawParameterValue (ch::syncToBpm);
        raw.chaos.division       = apvts.getRawParameterValue (ch::division);
    }

    {
        namespace arpid = params::id::arp;
        raw.arp.enable   = apvts.getRawParameterValue (arpid::enable);
        raw.arp.mode     = apvts.getRawParameterValue (arpid::mode);
        raw.arp.division = apvts.getRawParameterValue (arpid::division);
        raw.arp.octaves  = apvts.getRawParameterValue (arpid::octaves);
        raw.arp.gate     = apvts.getRawParameterValue (arpid::gate);
        raw.arp.swing    = apvts.getRawParameterValue (arpid::swing);
        raw.arp.latch    = apvts.getRawParameterValue (arpid::latch);
        raw.arp.phrase   = apvts.getRawParameterValue (arpid::phrase);
        raw.arp.velMode  = apvts.getRawParameterValue (arpid::velMode);
        raw.arp.chance   = apvts.getRawParameterValue (arpid::chance);
        raw.arp.stutter  = apvts.getRawParameterValue (arpid::stutter);
        raw.arp.jump     = apvts.getRawParameterValue (arpid::jump);
        raw.arp.humanize = apvts.getRawParameterValue (arpid::humanize);
    }

    raw.dests.reserve ((size_t) params::numModDests());
    for (const auto& dest : params::modDestinations())
        raw.dests.push_back (apvts.getRawParameterValue (dest.def->id));

    {
        namespace fx = params::id::fx;
        auto& rf = raw.fx;
        rf.distEnable     = apvts.getRawParameterValue (fx::distEnable);
        rf.distType       = apvts.getRawParameterValue (fx::distType);
        rf.distDrive      = apvts.getRawParameterValue (fx::distDrive);
        rf.distTone       = apvts.getRawParameterValue (fx::distTone);
        rf.distMix        = apvts.getRawParameterValue (fx::distMix);
        rf.chorusEnable   = apvts.getRawParameterValue (fx::chorusEnable);
        rf.chorusRate     = apvts.getRawParameterValue (fx::chorusRate);
        rf.chorusDepth    = apvts.getRawParameterValue (fx::chorusDepth);
        rf.chorusFeedback = apvts.getRawParameterValue (fx::chorusFeedback);
        rf.chorusWidth    = apvts.getRawParameterValue (fx::chorusWidth);
        rf.chorusMode     = apvts.getRawParameterValue (fx::chorusMode);
        rf.chorusMix      = apvts.getRawParameterValue (fx::chorusMix);
        rf.delayEnable    = apvts.getRawParameterValue (fx::delayEnable);
        rf.delaySync      = apvts.getRawParameterValue (fx::delaySync);
        rf.delayTime      = apvts.getRawParameterValue (fx::delayTime);
        rf.delayDivision  = apvts.getRawParameterValue (fx::delayDivision);
        rf.delayFeedback  = apvts.getRawParameterValue (fx::delayFeedback);
        rf.delayPingPong  = apvts.getRawParameterValue (fx::delayPingPong);
        rf.delayWidth     = apvts.getRawParameterValue (fx::delayWidth);
        rf.delayMix       = apvts.getRawParameterValue (fx::delayMix);
        rf.reverbEnable   = apvts.getRawParameterValue (fx::reverbEnable);
        rf.reverbMode     = apvts.getRawParameterValue (fx::reverbMode);
        rf.reverbPreDelay = apvts.getRawParameterValue (fx::reverbPreDelay);
        rf.reverbSize     = apvts.getRawParameterValue (fx::reverbSize);
        rf.reverbDecay    = apvts.getRawParameterValue (fx::reverbDecay);
        rf.reverbDamping  = apvts.getRawParameterValue (fx::reverbDamping);
        rf.reverbModDepth = apvts.getRawParameterValue (fx::reverbModDepth);
        rf.reverbLowCut   = apvts.getRawParameterValue (fx::reverbLowCut);
        rf.reverbHighCut  = apvts.getRawParameterValue (fx::reverbHighCut);
        rf.reverbWidth    = apvts.getRawParameterValue (fx::reverbWidth);
        rf.reverbMix      = apvts.getRawParameterValue (fx::reverbMix);
        rf.eqEnable       = apvts.getRawParameterValue (fx::eqEnable);
        rf.eqCharacter    = apvts.getRawParameterValue (fx::eqCharacter);
        for (int b = 0; b < 8; ++b)
        {
            auto& bp = rf.eqBands[(size_t) b];
            bp.enable = apvts.getRawParameterValue (params::id::eqBand (b, fx::eqband::enable));
            bp.type   = apvts.getRawParameterValue (params::id::eqBand (b, fx::eqband::type));
            bp.slope  = apvts.getRawParameterValue (params::id::eqBand (b, fx::eqband::slope));
            bp.freq   = apvts.getRawParameterValue (params::id::eqBand (b, fx::eqband::freq));
            bp.gain   = apvts.getRawParameterValue (params::id::eqBand (b, fx::eqband::gain));
            bp.q      = apvts.getRawParameterValue (params::id::eqBand (b, fx::eqband::q));
        }

        rf.modEnable      = apvts.getRawParameterValue (fx::modEnable);
        rf.modType        = apvts.getRawParameterValue (fx::modType);
        rf.modRate        = apvts.getRawParameterValue (fx::modRate);
        rf.modSync        = apvts.getRawParameterValue (fx::modSync);
        rf.modDivision    = apvts.getRawParameterValue (fx::modDivision);
        rf.modDepth       = apvts.getRawParameterValue (fx::modDepth);
        rf.modFeedback    = apvts.getRawParameterValue (fx::modFeedback);
        rf.modStages      = apvts.getRawParameterValue (fx::modStages);
        rf.modCentre      = apvts.getRawParameterValue (fx::modCentre);
        rf.modManual      = apvts.getRawParameterValue (fx::modManual);
        rf.modWidth       = apvts.getRawParameterValue (fx::modWidth);
        rf.modMix         = apvts.getRawParameterValue (fx::modMix);

        rf.tremEnable     = apvts.getRawParameterValue (fx::tremEnable);
        rf.tremRate       = apvts.getRawParameterValue (fx::tremRate);
        rf.tremSync       = apvts.getRawParameterValue (fx::tremSync);
        rf.tremDivision   = apvts.getRawParameterValue (fx::tremDivision);
        rf.tremDepth      = apvts.getRawParameterValue (fx::tremDepth);
        rf.tremShape      = apvts.getRawParameterValue (fx::tremShape);
        rf.tremStereo     = apvts.getRawParameterValue (fx::tremStereo);
        rf.tremMix        = apvts.getRawParameterValue (fx::tremMix);
        rf.vibEnable      = apvts.getRawParameterValue (fx::vibEnable);
        rf.vibRate        = apvts.getRawParameterValue (fx::vibRate);
        rf.vibSync        = apvts.getRawParameterValue (fx::vibSync);
        rf.vibDivision    = apvts.getRawParameterValue (fx::vibDivision);
        rf.vibDepth       = apvts.getRawParameterValue (fx::vibDepth);
        rf.vibMix         = apvts.getRawParameterValue (fx::vibMix);

        rf.limEnable      = apvts.getRawParameterValue (fx::limEnable);
        rf.limDrive       = apvts.getRawParameterValue (fx::limDrive);
        rf.limCeiling     = apvts.getRawParameterValue (fx::limCeiling);
        rf.limRelease     = apvts.getRawParameterValue (fx::limRelease);
        rf.limAutoRelease = apvts.getRawParameterValue (fx::limAutoRelease);
        rf.limCharacter   = apvts.getRawParameterValue (fx::limCharacter);
        rf.limStereoLink  = apvts.getRawParameterValue (fx::limStereoLink);
        rf.limTruePeak    = apvts.getRawParameterValue (fx::limTruePeak);
        rf.limLookahead   = apvts.getRawParameterValue (fx::limLookahead);
        rf.limAutoGain    = apvts.getRawParameterValue (fx::limAutoGain);

        rf.convEnable     = apvts.getRawParameterValue (fx::convEnable);
        rf.convMix        = apvts.getRawParameterValue (fx::convMix);
        rf.convWidth      = apvts.getRawParameterValue (fx::convWidth);
        rf.convPreDelay   = apvts.getRawParameterValue (fx::convPreDelay);
        rf.convDecay      = apvts.getRawParameterValue (fx::convDecay);
        rf.convDamping    = apvts.getRawParameterValue (fx::convDamping);
        rf.convStart      = apvts.getRawParameterValue (fx::convStart);
    }

    factoryTable = std::make_shared<const dsp::Wavetable> (dsp::Wavetable::createBasicShapes());
    // Choice 0 (Basic Shapes) is the only built-in Table-menu entry built
    // here in the ctor -- it's also the pre-1.0.15 default, so it must stay
    // instant. Measured: building all numWavetableTableChoices tables here
    // cost ~76ms, unacceptable for a ctor that runs on every plugin
    // instantiation (repeatedly under auval/pluginval too), so every other
    // choice is instead built lazily on first selection (setBuiltInWavetable)
    // via the same background-thread + latest-wins + pendingLoads bookkeeping
    // as loadWavetableFromFile, and cached in builtInTables for the
    // processor's lifetime so later selections of the same choice are instant.
    builtInTables[0] = factoryTable;   // choice 0 == Basic Shapes, same table
    for (int s = 0; s < params::numOscSlots; ++s)
    {
        slotTables[(size_t) s].current = factoryTable;
        slotTables[(size_t) s].live.store (factoryTable.get());
    }

    // Each buffer's default constructor already sets a default triangle
    // (dsp::CustomLFOShape's own ctor); point `live` at buffer 0 so
    // shared.lfo[i].custom is never null even before any edit or state
    // restore publishes anything.
    for (auto& c : customLfo)
        c.live.store (&c.buffers[0]);

    shared.telemetry = &telemetry;
    shared.glide = &glideState;
    midiLearn = std::make_unique<MidiLearnManager> (apvts);

    synth.addSound (new dsp::SPASynthSound());
    for (int i = 0; i < numVoices; ++i)
        synth.addVoice (new dsp::SPASynthVoice (shared, i));

    presetManager = std::make_unique<library::PresetManager> (
        [this] { return buildStateTree (false); },   // presets carry no MIDI map
        [this] (const juce::ValueTree& state) { restoreStateTree (state, true); },   // preset/reset load
        library::defaultPresetsRoot());

    // Auto-discover the library and make sure factory presets exist — no
    // user setup required when content sits in a standard install location.
    juce::MessageManager::callAsync ([weak = juce::WeakReference<SPASynthProcessor> (this)]
    {
        if (weak != nullptr)
            weak->refreshLibrary();
    });

    startTimer (150);  // purges retired wavetables; applies oversampling changes
}

SPASynthProcessor::~SPASynthProcessor()
{
    stopTimer();

    // Wait for every background sample/wavetable load or built-in-wavetable
    // build this processor has launched (juce::Thread::launch, self-deleting
    // LambdaThreads) to finish before tearing anything else down. Found via
    // ASan on a chained-RANDOMIZE-ALL repro (filterResonanceWriteCrashReproTest):
    // calling randomizeAll() repeatedly (every roll can reselect a slot's
    // built-in Table, each triggering a fresh background build) queues
    // background threads far faster than each can finish; the test process
    // exited (and, along the way, JUCE's MessageManager began shutting down)
    // with dozens of those threads still mid-flight, which surfaced as
    // JUCE's LeakedObjectDetector reporting live LambdaThread/Thread/
    // WaitableEvent/FFT instances and, separately, a
    // juce_MessageManager_mac.mm:435 assertion consistent with a background
    // thread calling MessageManager::callAsync() after/during MessageManager
    // teardown. The existing WeakReference guard in each load lambda already
    // makes touching `this` from the async completion callback race-free
    // against THIS processor's own destruction, but it does nothing to stop
    // the underlying OS thread from still being alive (and about to touch
    // the message manager) once the processor -- and, in a real host, quite
    // possibly the whole plugin instance shortly after -- is gone. A bounded
    // wait here (builds/loads are documented elsewhere as "a handful of ms")
    // closes that window; the queued MessageManager::callAsync completions
    // are harmless no-ops once weak == nullptr, so nothing here needs their
    // result, just their thread to have finished running.
    {
        constexpr int maxWaitMs = 3000;
        int waited = 0;
        while (activeBackgroundThreads.load (std::memory_order_acquire) > 0 && waited < maxWaitMs)
        {
            juce::Thread::sleep (2);
            waited += 2;
        }
        jassert (activeBackgroundThreads.load (std::memory_order_acquire) == 0);
    }

    for (auto* p : getParameters())
        if (auto* withID = dynamic_cast<juce::AudioProcessorParameterWithID*> (p))
            apvts.removeParameterListener (withID->paramID, this);
}

void SPASynthProcessor::parameterChanged (const juce::String& parameterID, float)
{
    // Only the osc-slot mode parameters are registered (see the ctor).
    // AudioProcessorParameter::setValueNotifyingHost() -- the sole path that
    // reaches an AudioProcessorValueTreeState::Listener -- is, by JUCE
    // convention, only ever called from the UI/message thread (a user
    // twiddling a control) or from a message-thread preset restore
    // (restoreStateTree()'s apvts.replaceState()). Host automation instead
    // writes parameters via the plain setValue() path, which does NOT notify
    // listeners, so this callback can never arrive from the audio thread --
    // safe to allocate here. (getCallbackLock() is JUCE's CriticalSection,
    // which is recursive, so this is also safe to call reentrantly from
    // inside restoreStateTree()'s own ScopedLock on the same thread.)
    for (int s = 0; s < params::numOscSlots; ++s)
    {
        if (parameterID == params::id::oscSlot (s, params::id::osc::mode))
            ensurePluckAllocatedForSlot (s);
        else if (parameterID == params::id::oscSlot (s, params::id::osc::table))
            applyBuiltInWavetableFromParam (s);
    }

    // Every parameter is now registered (see the ctor's generic sweep), so
    // this fires for the whole layout, including automation. markPresetDirty()
    // itself no-ops during a preset/reset load (presetLoadGuard).
    markPresetDirty();
}

void SPASynthProcessor::markPresetDirty()
{
    if (presetLoadGuard.load (std::memory_order_relaxed))
        return;

    if (! presetDirty.exchange (true, std::memory_order_relaxed))
        dirtyNotifier.triggerAsyncUpdate();
}

void SPASynthProcessor::clearPresetDirty()
{
    if (presetDirty.exchange (false, std::memory_order_relaxed))
        dirtyNotifier.triggerAsyncUpdate();
}

void SPASynthProcessor::ensurePluckAllocatedForSlot (int s)
{
    if (s < 0 || s >= params::numOscSlots || pluckAllocated[(size_t) s])
        return;
    if (raw.slots[(size_t) s].mode == nullptr
        || (params::OscMode) (int) raw.slots[(size_t) s].mode->load() != params::OscMode::pluck)
        return;

    const juce::ScopedLock sl (getCallbackLock());
    for (int v = 0; v < synth.getNumVoices(); ++v)
        if (auto* voice = dynamic_cast<dsp::SPASynthVoice*> (synth.getVoice (v)))
            voice->ensurePluckAllocated (s);
    pluckAllocated[(size_t) s] = true;
}

void SPASynthProcessor::timerCallback()
{
    // Apply a pending oversampling-factor change: rebuild the engine at the new
    // rate under the callback lock so no processBlock touches it mid-rebuild.
    if (const int pf = pendingOsFactor.load (std::memory_order_relaxed);
        pf != currentOsFactor)
    {
        const juce::ScopedLock sl (getCallbackLock());
        rebuildOversampling (pf);
    }

    // Service a non-finite-output flush request (processBlock already silenced
    // the offending block and set the flag): reset the FX chain's stateful DSP
    // under the callback lock so a poisoned feedback ring (delay/reverb/mod/
    // eq/limiter/convolution) can't keep re-emitting garbage on every repeat.
    // Voices are intentionally NOT reset here: they are per-note and their
    // filter/envelope state is re-primed on the next startNote/noteOn, so any
    // poisoned voice state ages out naturally rather than recirculating
    // indefinitely the way a feedback structure would.
    if (fxStateFlushPending.exchange (false, std::memory_order_relaxed))
    {
        const juce::ScopedLock sl (getCallbackLock());
        fxChain.reset();
    }

    // Report latency: the limiter's lookahead (engine samples -> host) plus the
    // oversampler's own near-zero IIR latency.
    const int lat = desiredLatency.load (std::memory_order_relaxed) / juce::jmax (1, currentOsFactor)
                  + osLatencyHost;
    if (lat != getLatencySamples())
        setLatencySamples (lat);

    // Convolution IR shaping (decay/damping) reshapes + reloads the IR; do it
    // here (message thread), debounced to the timer, only when the values move.
    if (raw.fx.convDecay != nullptr)
        fxChain.setConvolutionShaping (raw.fx.convDecay->load(), raw.fx.convDamping->load(),
                                       raw.fx.convStart->load());

    // Fallback safety net for the lazy Pluck-buffer allocation normally done
    // synchronously in parameterChanged(): catches a slot that was already in
    // Pluck mode before a listener was registered (e.g. a state restore path
    // that bypasses parameterChanged) or any other edge case. Cheap no-op
    // once a slot is allocated (pluckAllocated guards it).
    for (int s = 0; s < params::numOscSlots; ++s)
        ensurePluckAllocatedForSlot (s);

    // Anything retired more than one timer period ago can no longer be in
    // use by the audio thread (it re-reads `live` every block).
    retiredTables.clear();
    retiredSamples.clear();

    // Library auto-refresh: cheap (folder-listing-only) fingerprint check,
    // throttled to ~3s/~10s -- see tickLibraryWatch() and the class comment
    // on the watcher fields for the debounce rationale.
    if (libraryWatchEnabled)
    {
        const auto now = juce::Time::getMillisecondCounter();
        if (now >= nextLibraryWatchMs)
        {
            const int interval = getActiveEditor() != nullptr
                                    ? libraryWatchActiveIntervalMs : libraryWatchIdleIntervalMs;
            nextLibraryWatchMs = now + (juce::uint32) juce::jmax (0, interval);
            tickLibraryWatch();
        }
    }
}

juce::String SPASynthProcessor::computeLibraryFingerprint() const
{
    // Configured root only -- NEVER run discovery here. This runs every
    // ~3s while an editor is open (see the timer above); findLibraryRoot()
    // used to be called here and, with no root configured, would run its
    // full multi-hundred-directory auto-discovery probe on every tick (and
    // persist whatever it found) -- see tickLibraryWatch() for the throttled
    // discovery path that replaces that behaviour.
    const auto root = library::getLibraryRoot();
    if (! root.isDirectory())
        return "<missing>";

    auto subfolders = root.findChildFiles (juce::File::findDirectories, false);
    std::sort (subfolders.begin(), subfolders.end(),
               [] (const juce::File& a, const juce::File& b)
               { return a.getFileName().compareIgnoreCase (b.getFileName()) < 0; });

    juce::StringArray parts;
    parts.add (juce::String (root.getLastModificationTime().toMilliseconds()));
    for (const auto& f : subfolders)
        parts.add (f.getFileName() + ":" + juce::String (f.getLastModificationTime().toMilliseconds()));

    return parts.joinIntoString ("|");
}

void SPASynthProcessor::tickLibraryWatch()
{
    if (! library::getLibraryRoot().isDirectory())
    {
        // No root configured: computeLibraryFingerprint() deliberately never
        // runs discovery (see its comment), so on its own the watcher would
        // never notice a library appearing in a default install location.
        // Run full discovery (refreshLibrary(), which calls
        // library::findLibraryRoot()'s multi-hundred-directory probe and
        // persists whatever it finds) here instead -- but throttled hard,
        // since that's the exact expensive walk this fix exists to stop
        // running every tick.
        const auto now = juce::Time::getMillisecondCounter();
        if (now < nextLibraryDiscoveryMs)
            return;

        nextLibraryDiscoveryMs = now + (juce::uint32) juce::jmax (0, libraryWatchDiscoveryIntervalMs);
        ++libraryDiscoveryAttemptCountForTest;
        if (! skipActualDiscoveryForTest)
            refreshLibrary();   // no-op fingerprint side effects if still nothing found
        return;
    }

    const auto fp = computeLibraryFingerprint();

    if (fp == lastScannedLibraryFingerprint)
    {
        // Already in sync (e.g. right after refreshLibrary() ran, manually
        // or from a previous watcher tick) -- nothing pending.
        pendingLibraryFingerprint = fp;
        libraryFingerprintStableTicks = 0;
        return;
    }

    if (fp == pendingLibraryFingerprint)
    {
        // Seen this exact (changed) listing before -- one more identical
        // read confirms it's stable, i.e. not a pack still mid-copy.
        if (++libraryFingerprintStableTicks >= 2)
        {
            libraryFingerprintStableTicks = 0;
            refreshLibrary();   // also updates lastScannedLibraryFingerprint
        }
    }
    else
    {
        // First sighting of this listing -- start the debounce over.
        pendingLibraryFingerprint = fp;
        libraryFingerprintStableTicks = 1;
    }
}

void SPASynthProcessor::installSample (int slot, std::shared_ptr<const dsp::SampleData> sample,
                                      juce::String path, juce::String error)
{
    auto& ss = slotSamples[(size_t) slot];
    ss.error = std::move (error);

    if (sample != nullptr)
    {
        if (ss.current != nullptr)
            retiredSamples.push_back (std::move (ss.current));
        ss.current = std::move (sample);
        ss.live.store (ss.current.get());
        ss.path = std::move (path);
    }

    sendChangeMessage();
}

void SPASynthProcessor::loadSampleFromFile (int slot, const juce::File& file, bool markEdited)
{
    // A content change (1.0.26 edited indicator) -- see markPresetDirty().
    // markEdited is false only for restoreStateTree()'s own deferred
    // content restore -- see its declaration comment.
    if (markEdited)
        markPresetDirty();

    // Count the in-flight load (and broadcast) so the UI can show a loading
    // state; the decrement lives in the completion lambda — NOT in
    // installSample — so direct installs never underflow the counter.
    // requestSerial (message thread) makes the newest request win: rapid
    // quick-swap auditioning fires many background loads that finish out of
    // order, and a slow earlier analysis must not stomp the latest pick.
    auto& ss = slotSamples[(size_t) slot];
    const int serial = ++ss.requestSerial;
    ss.pendingLoads.fetch_add (1);
    sendChangeMessage();

    // Capture a weak ref, not raw `this` — the host can destroy the processor
    // while this background load is in flight (the loader retries with sleeps
    // up to ~360ms). The Thread::launch lambda below never touches `this`
    // after the load returns; only the callAsync completion may, and it
    // null-checks the weak ref first. That check-then-use is race-free because
    // JUCE::WeakReference nulls out (and the processor's destructor runs) on
    // the message thread, the same thread callAsync runs on — so there's no
    // concurrent check-vs-delete to race against, even though WeakReference
    // itself isn't thread-safe in general.
    juce::WeakReference<SPASynthProcessor> weak (this);
    ++activeBackgroundThreads;
    juce::Thread::launch ([weak, slot, file, serial]
    {
        // 1.0.25: skip the (potentially slow -- the loader retries with
        // sleeps up to ~360ms) load entirely if a newer request for this
        // slot already landed. See setBuiltInWavetable's identical check
        // for the full rationale.
        if (weak == nullptr || serial != weak->slotSamples[(size_t) slot].requestSerial.load())
        {
            if (weak != nullptr)
            {
                weak->slotSamples[(size_t) slot].pendingLoads.fetch_sub (1);
                --weak->activeBackgroundThreads;
            }
            return;
        }

        auto result = dsp::loadSampleFromFile (file);

        juce::MessageManager::callAsync ([weak, slot, serial, loaded = std::move (result),
                                          path = file.getFullPathName()]() mutable
        {
            if (weak == nullptr)
                return;   // processor was destroyed while this load was in flight
            auto& s = weak->slotSamples[(size_t) slot];
            s.pendingLoads.fetch_sub (1);
            if (serial != s.requestSerial)
                return;   // superseded by a newer request — drop this stale result
            weak->installSample (slot, std::move (loaded.sample), path, loaded.error);
        });

        // Only the counter decrement below touches `this` directly (no
        // WeakReference dereference of processor members) -- see the
        // destructor's comment: it must NEVER decrement below zero on a
        // still-valid processor, so `this` here is safe exactly because the
        // destructor's wait loop guarantees no processor is destroyed while
        // this count is still nonzero.
        if (weak != nullptr)
            --weak->activeBackgroundThreads;
    });
}

juce::Array<juce::File> SPASynthProcessor::getPackSiblings (int slot) const
{
    const auto file = getSampleFile (slot);
    const auto root = library::findLibraryRoot();
    if (! root.isDirectory() || ! file.isAChildOf (root))
        return {};

    auto wavs = file.getParentDirectory()
                    .findChildFiles (juce::File::findFiles, false, "*.wav;*.WAV");
    std::sort (wavs.begin(), wavs.end(),
               [] (const juce::File& a, const juce::File& b)
               { return a.getFileName().compareIgnoreCase (b.getFileName()) < 0; });
    return wavs;
}

juce::String SPASynthProcessor::getSampleName (int slot) const
{
    const auto& current = slotSamples[(size_t) slot].current;
    return current != nullptr ? current->name : juce::String();
}

juce::String SPASynthProcessor::getSampleError (int slot) const
{
    return slotSamples[(size_t) slot].error;
}

void SPASynthProcessor::copyOscSlot (int fromSlot, int toSlot)
{
    if (fromSlot == toSlot)
        return;

    // Parameters: enumerate the source slot's Section from the registry --
    // never a hand-written key list, so a new osc param is picked up for
    // free. Both slots' definitions for the same key share identical
    // ranges (they're the same ParamDef replicated per slot), so copying
    // the normalised [0,1] value is exact and simplest.
    const auto section = params::oscSection (fromSlot);
    const auto destLetter = params::id::oscSlotLetter (toSlot);
    for (auto& def : params::all())
    {
        if (def.section != section)
            continue;
        auto* src = apvts.getParameter (def.id);
        if (src == nullptr)
            continue;
        const auto key = def.id.fromFirstOccurrenceOf (".", false, false);
        const auto destId = "osc" + destLetter + "." + key;
        if (auto* dst = apvts.getParameter (destId))
            dst->setValueNotifyingHost (src->getValue());
    }

    // Content is not a parameter -- a slot with the right knob values but no
    // reloaded file plays nothing. Reload through the normal async loaders
    // (never a raw path/pointer copy) so this obeys the same threading
    // contract as LOAD/drag-drop.
    const auto sampleFile = getSampleFile (fromSlot);
    if (sampleFile.existsAsFile())
        loadSampleFromFile (toSlot, sampleFile);
    const auto tableFile = getWavetableFile (fromSlot);
    if (tableFile.existsAsFile())
        loadWavetableFromFile (toSlot, tableFile);

    // Deliberately NOT touching the mod matrix: routes name a specific
    // slot's parameter as their destination, so copying them would mean
    // hunting for free rows and rewriting destinations -- and would consume
    // matrix rows the user never asked for. The oscillator copies, its
    // wiring does not.
}

void SPASynthProcessor::swapOscSlots (int slotA, int slotB)
{
    if (slotA == slotB)
        return;

    // Snapshot BOTH slots' parameters and content paths completely before
    // writing either one -- writing slotA from slotB while still reading
    // slotA for the slotB write would overwrite the source mid-swap.
    struct SlotSnapshot
    {
        std::vector<std::pair<juce::String, float>> paramValues;   // full id -> normalised value
        juce::File sampleFile, tableFile;
    };

    const auto snapshot = [this] (int slot)
    {
        SlotSnapshot s;
        const auto section = params::oscSection (slot);
        for (auto& def : params::all())
        {
            if (def.section != section)
                continue;
            if (auto* p = apvts.getParameter (def.id))
                s.paramValues.push_back ({ def.id, p->getValue() });
        }
        s.sampleFile = getSampleFile (slot);
        s.tableFile = getWavetableFile (slot);
        return s;
    };

    const auto a = snapshot (slotA);
    const auto b = snapshot (slotB);

    const auto letterA = params::id::oscSlotLetter (slotA);
    const auto letterB = params::id::oscSlotLetter (slotB);

    const auto applyTo = [this] (const SlotSnapshot& src, const juce::String& destLetter)
    {
        for (auto& entry : src.paramValues)
        {
            const auto key = entry.first.fromFirstOccurrenceOf (".", false, false);
            const auto destId = "osc" + destLetter + "." + key;
            if (auto* dst = apvts.getParameter (destId))
                dst->setValueNotifyingHost (entry.second);
        }
    };

    applyTo (a, letterB);
    applyTo (b, letterA);

    if (a.sampleFile.existsAsFile())
        loadSampleFromFile (slotB, a.sampleFile);
    if (a.tableFile.existsAsFile())
        loadWavetableFromFile (slotB, a.tableFile);
    if (b.sampleFile.existsAsFile())
        loadSampleFromFile (slotA, b.sampleFile);
    if (b.tableFile.existsAsFile())
        loadWavetableFromFile (slotA, b.tableFile);

    // Mod matrix left untouched -- see copyOscSlot()'s comment.
}

float SPASynthProcessor::getRandomWildness() const
{
    return (float) (double) apvts.state.getProperty (wildnessProperty, 0.5);
}

void SPASynthProcessor::setRandomWildness (float wildness)
{
    apvts.state.setProperty (wildnessProperty, (double) wildness, nullptr);
    markPresetDirty();   // WILD rides an apvts.state PROPERTY, not a parameter
}

bool SPASynthProcessor::isLockGroupLocked (int group) const
{
    const auto mask = (juce::uint32) (int) apvts.state.getProperty (lockMaskProperty, 0);
    return (mask & (1u << group)) != 0;
}

void SPASynthProcessor::setLockGroupLocked (int group, bool locked)
{
    auto mask = (juce::uint32) (int) apvts.state.getProperty (lockMaskProperty, 0);
    mask = locked ? (mask | (1u << group)) : (mask & ~(1u << group));
    apvts.state.setProperty (lockMaskProperty, (int) mask, nullptr);
}

void SPASynthProcessor::randomizeAll()
{
    // Unconditional (1.0.26 edited indicator): every unlocked param rolled
    // below already marks dirty itself via parameterChanged(), but this
    // guarantees the invariant even in the edge case every lock group is
    // engaged (so nothing else below would fire it).
    markPresetDirty();

    {
        // Stuck-note safety net (tester report, 1.0.21): clicking RANDOMIZE ALL
        // while a QWERTY key is held left that note sounding forever, because
        // the click moved keyboard focus and the matching key-UP never reached
        // the on-screen keyboard. Same hard reset the panic path in
        // processBlock uses; keyboardState.allNotesOff matters as much as the
        // synth's, or the strip keeps believing the key is down and can emit a
        // stray note-off later. fxChain is deliberately NOT reset: unlike a
        // preset load this replaces no state wholesale, and killing reverb and
        // delay tails on every roll would be a behaviour change nobody asked
        // for. Consequence, matching preset load: rolling the dice while
        // holding a note cuts that note.
        //
        // randomizeAll() runs on the message thread, so the voices/arp it
        // touches here need the callback lock that processBlock holds --
        // mirroring restoreStateTree()'s reset. The lock is scoped to these
        // three lines so no parameter listener can ever run underneath it.
        const juce::ScopedLock sl (getCallbackLock());
        synth.allNotesOff (0, false);   // channel <= 0 = all voices, no tail-off
        arp.reset();
        keyboardState.allNotesOff (0);
    }

    auto& rng = juce::Random::getSystemRandom();
    const auto wildness = getRandomWildness();
    const auto lockedMask = (juce::uint32) (int) apvts.state.getProperty (lockMaskProperty, 0);

    params::randomizeAll (apvts, wildness, lockedMask, rng);

    // --- Musicality post-pass ------------------------------------------------
    const auto setNorm = [this] (const juce::String& id, float norm)
    {
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, norm));
    };
    const auto realValue = [this] (const juce::String& id)
    {
        auto* param = apvts.getParameter (id);
        return param != nullptr ? param->convertFrom0to1 (param->getValue()) : 0.0f;
    };

    const bool oscsUnlocked = (lockedMask & (1u << (int) params::LockGroup::oscillators)) == 0;
    const bool filterUnlocked = (lockedMask & (1u << (int) params::LockGroup::filter)) == 0;

    if (oscsUnlocked)
    {
        namespace osc = params::id::osc;
        for (int s = 0; s < params::numOscSlots; ++s)
        {
            // A slot with no sample loaded can't be in a sample mode (the
            // synthesis engines are always valid).
            const auto mode = (params::OscMode) (int) realValue (
                params::id::oscSlot (s, osc::mode));
            if (slotSamples[(size_t) s].current == nullptr
                && (mode == params::OscMode::sample || mode == params::OscMode::granular))
                setNorm (params::id::oscSlot (s, osc::mode), 0.0f);

            // Keep loop points ordered with a usable window.
            const auto loopStart = realValue (params::id::oscSlot (s, osc::loopStart));
            const auto loopEnd = realValue (params::id::oscSlot (s, osc::loopEnd));
            if (loopEnd < loopStart + 0.05f)
                setNorm (params::id::oscSlot (s, osc::loopEnd),
                         juce::jlimit (0.0f, 1.0f, loopStart + 0.2f));

            // Cause 5 (1.0.25 -- revised twice now). Round 1 was a blind
            // 0.85 ceiling on sampleStart/grainPos; a ceiling can't know
            // where a real file's audible content actually is. Round 2
            // replaced it with a FIXED safe window (PresetManager's
            // writeSafeSampleLoop/safeGrainPosBase idea: a short loop near
            // the file start) -- which closed the silence gap but killed
            // all RANDOMIZE ALL variety on sample start/loop/grain position,
            // since every roll landed on the exact same spot. This round:
            // energy-aware RANDOM choice instead of a fixed spot. SampleData
            // already computes an RMS-like ampCurve at load time (background
            // thread, peak-of-file normalized to 1.0, see SampleData.h) for
            // the SFX-follower mod sources -- reused here rather than
            // computing a second envelope. A candidate hop qualifies if it's
            // within ~20 dB of the file's peak (0.1 linear, since ampCurve
            // is already peak-normalized), and sampleStart/loopStart is
            // drawn uniformly from whichever qualifying hops still leave the
            // loopCapSeconds-wide loop window inside the file; grainPos is a
            // separate independent draw from every qualifying hop (grains
            // don't loop a window the same way, so no fit constraint).
            // Falls back to the round-2 fixed safe window when no envelope
            // is available (very short/edge-case files) or no hop clears
            // the bar (a uniformly quiet/flat file), and further back to
            // the original blind ceiling when no content is loaded yet.
            constexpr float safeSampleWindowBase = 0.12f;       // mirrors safeGrainPosBase
            constexpr double safeSampleLoopCapSeconds = 1.2;    // mirrors loopCapSeconds
            constexpr float withinPeakLinear = 0.1f;            // -20 dB, ampCurve is peak-normalized
            if (const auto* sampleData = slotSamples[(size_t) s].current.get())
            {
                const auto duration = sampleData->lengthSeconds();
                const auto fixedSafeStartNorm = duration > safeSampleLoopCapSeconds ? safeSampleWindowBase : 0.0f;

                float startNorm = fixedSafeStartNorm;
                float grainPosNorm = fixedSafeStartNorm;
                if (! sampleData->ampCurve.empty() && sampleData->hopSeconds > 0.0 && duration > 0.0)
                {
                    const auto numHops = (int) sampleData->ampCurve.size();
                    const auto loopWidthSeconds = juce::jmin (duration, safeSampleLoopCapSeconds);
                    const auto maxStartHop = juce::jmax (0,
                        (int) ((duration - loopWidthSeconds) / sampleData->hopSeconds));
                    const auto windowHops = juce::jmax (1,
                        (int) (loopWidthSeconds / sampleData->hopSeconds));

                    std::vector<int> loudStartHops, loudGrainHops;
                    for (int h = 0; h < numHops; ++h)
                        if (sampleData->ampCurve[(size_t) h] >= withinPeakLinear)
                            loudGrainHops.push_back (h);

                    // sampleStart/loopStart aren't just a point -- the whole
                    // loopCapSeconds window that starts there is what
                    // actually plays on every loop repeat. Qualify a start
                    // hop on the window's AVERAGE level (not just its own
                    // hop), so a start that looks loud but immediately dips
                    // into near-silence for the rest of the loop doesn't
                    // qualify.
                    for (int h = 0; h <= maxStartHop; ++h)
                    {
                        float sum = 0.0f;
                        int n = 0;
                        for (int w = h; w < juce::jmin (numHops, h + windowHops); ++w)
                        {
                            sum += sampleData->ampCurve[(size_t) w];
                            ++n;
                        }
                        if (n > 0 && (sum / (float) n) >= withinPeakLinear)
                            loudStartHops.push_back (h);
                    }

                    if (! loudStartHops.empty())
                    {
                        const auto hop = loudStartHops[(size_t) rng.nextInt ((int) loudStartHops.size())];
                        startNorm = (float) juce::jlimit (0.0, 0.999,
                            (hop * sampleData->hopSeconds) / duration);
                    }
                    if (! loudGrainHops.empty())
                    {
                        const auto hop = loudGrainHops[(size_t) rng.nextInt ((int) loudGrainHops.size())];
                        grainPosNorm = (float) juce::jlimit (0.0, 0.999,
                            (hop * sampleData->hopSeconds) / duration);
                    }
                }

                const float loopEndNorm = duration > 0.0
                    ? juce::jlimit (0.0f, 0.999f,
                          startNorm + (float) juce::jmin (1.0, safeSampleLoopCapSeconds / duration))
                    : 0.999f;

                setNorm (params::id::oscSlot (s, osc::sampleStart), startNorm);
                setNorm (params::id::oscSlot (s, osc::loopStart), startNorm);
                setNorm (params::id::oscSlot (s, osc::loopEnd), loopEndNorm);
                setNorm (params::id::oscSlot (s, osc::grainPos), grainPosNorm);
            }
            else
            {
                constexpr float maxSafeSampleStart = 0.85f;
                const auto sampleStart = realValue (params::id::oscSlot (s, osc::sampleStart));
                if (sampleStart > maxSafeSampleStart)
                    setNorm (params::id::oscSlot (s, osc::sampleStart), maxSafeSampleStart);

                constexpr float maxSafeGrainPos = 0.85f;
                const auto grainPos = realValue (params::id::oscSlot (s, osc::grainPos));
                if (grainPos > maxSafeGrainPos)
                    setNorm (params::id::oscSlot (s, osc::grainPos), maxSafeGrainPos);
            }

            // Cause 6: at high wildness grainDensity's full range (1-100/s)
            // opens all the way down to ~1 grain/sec, which combined with a
            // short grainSize leaves multi-hundred-ms gaps between grains --
            // audible as near-total silence over a held-note test window,
            // even though the engine is technically firing. A density floor
            // guarantees overlap without capping the density knob's usable
            // range from above (still 1-100/s by hand).
            constexpr float minSafeGrainDensity = 4.0f;
            const auto grainDensity = realValue (params::id::oscSlot (s, osc::grainDensity));
            if (grainDensity < minSafeGrainDensity)
                if (auto* param = apvts.getParameter (params::id::oscSlot (s, osc::grainDensity)))
                    param->setValueNotifyingHost (param->convertTo0to1 (minSafeGrainDensity));
        }
    }

    if (filterUnlocked)
    {
        // High-passing everything above the note range means silence: pull
        // HP/notch cutoffs back into a musical zone. Trigger threshold
        // dropped 2000 -> 1200 Hz (1.0.25, randomizeAllChainedRealLibraryTest
        // against the real library, playing notes across MIDI 36-84 rather
        // than the old test's note-60-only coverage): the old 2000 Hz
        // threshold left plenty of room for an HP/notch sitting between
        // ~1050 and 2000 Hz to remove a high-note fundamental (MIDI 84 is
        // ~1046 Hz) without ever tripping the guard. The clamp target's top
        // end also drops 1000 -> 800 Hz so a re-rolled cutoff stays below
        // that fundamental with margin.
        const auto type = (params::FilterType) (int) realValue (params::id::filter1Type);
        const auto cutoff = realValue (params::id::filter1Cutoff);
        const bool subtractive = type == params::FilterType::hp12
                              || type == params::FilterType::hp24
                              || type == params::FilterType::notch12
                              || type == params::FilterType::notch24;

        if (subtractive && cutoff > 1200.0f)
            if (auto* param = apvts.getParameter (params::id::filter1Cutoff))
                param->setValueNotifyingHost (
                    param->convertTo0to1 (150.0f + rng.nextFloat() * 650.0f));

        // Same guard for filter 2 when it rolls enabled + subtractive-high.
        const auto f2Enabled = realValue (params::id::filter2Enable) >= 0.5f;
        const auto f2Type = (params::FilterType) (int) realValue (params::id::filter2Type);
        const auto f2Cutoff = realValue (params::id::filter2Cutoff);
        const bool f2Subtractive = f2Type == params::FilterType::hp12
                                || f2Type == params::FilterType::hp24
                                || f2Type == params::FilterType::notch12
                                || f2Type == params::FilterType::notch24;
        if (f2Enabled && f2Subtractive && f2Cutoff > 1200.0f)
            if (auto* param = apvts.getParameter (params::id::filter2Cutoff))
                param->setValueNotifyingHost (
                    param->convertTo0to1 (150.0f + rng.nextFloat() * 650.0f));

        // Bandpass centred far from a played note's fundamental/harmonics
        // silences it just as completely as an over-eager HP/notch -- traced
        // two "genuinely zero forever" sweep seeds (filter type BP12/BP24,
        // cutoff ~4.5-12.9 kHz) all the way to SPASynthVoice's oscillator sum
        // itself measuring 0 at the sample level once that filter's mix blend
        // was engaged. Reusing this pass's existing HP/notch case, so this
        // was a missing bandpass case, not a new mechanism.
        //
        // 1.0.25 follow-up (randomizeNeverSilentChainedTest's chained,
        // full-note-range coverage, plus randomizeAllChainedRealLibraryTest):
        // even inside the 100 Hz-3 kHz band above, a BP centred at (say)
        // 1.6 kHz measured ~-49 dBFS against a played note near 260 Hz --
        // an attempted fix that instead capped RESONANCE (to widen the
        // passband) turned out to reproduce an unrelated pre-existing
        // background-thread-leak crash under repeated randomizeAll() and was
        // reverted; see chainedRandomizeBackgroundThreadLeakTest. The right
        // fix is the PASSBAND'S POSITION, not its width: randomizeAll has no
        // idea which note will be played, so instead of guessing a cutoff
        // that happens to be close enough, floor filterKeytrack/
        // filter2Keytrack at 0.9 whenever that filter is a bandpass --
        // SPASynthVoice already computes `cutoff * 2^(keytrack*(note-60)/12)`
        // (see the "Cutoff with keytracking" comment there), so at keytrack
        // ~1 the passband tracks ANY played note relative to C3 rather than
        // sitting fixed at whatever cutoff got rolled. The cutoff clamp
        // itself is also tightened (150 Hz-1.2 kHz, was 100 Hz-3 kHz) so the
        // reference note (C3/MIDI 60) it's centred on on is itself always
        // audible, keytracking then carries that centring to every other
        // note. Filter 2 gets the identical treatment, so a series/parallel
        // pair of bandpasses both track together rather than drifting apart.
        const bool bandpass1 = type == params::FilterType::bp12 || type == params::FilterType::bp24;
        if (bandpass1 && (cutoff < 150.0f || cutoff > 1200.0f))
            if (auto* param = apvts.getParameter (params::id::filter1Cutoff))
                param->setValueNotifyingHost (
                    param->convertTo0to1 (150.0f + rng.nextFloat() * 1050.0f));
        if (bandpass1)
        {
            constexpr float minSafeBandpassKeytrack = 0.9f;
            if (auto* param = apvts.getParameter (params::id::filter1Keytrack))
            {
                const auto kt = param->convertFrom0to1 (param->getValue());
                if (kt < minSafeBandpassKeytrack)
                    param->setValueNotifyingHost (param->convertTo0to1 (minSafeBandpassKeytrack));
            }
        }

        const bool bandpass2 = f2Type == params::FilterType::bp12 || f2Type == params::FilterType::bp24;
        if (f2Enabled && bandpass2 && (f2Cutoff < 150.0f || f2Cutoff > 1200.0f))
            if (auto* param = apvts.getParameter (params::id::filter2Cutoff))
                param->setValueNotifyingHost (
                    param->convertTo0to1 (150.0f + rng.nextFloat() * 1050.0f));
        if (f2Enabled && bandpass2)
        {
            constexpr float minSafeBandpassKeytrack = 0.9f;
            if (auto* param = apvts.getParameter (params::id::filter2Keytrack))
            {
                const auto kt = param->convertFrom0to1 (param->getValue());
                if (kt < minSafeBandpassKeytrack)
                    param->setValueNotifyingHost (param->convertTo0to1 (minSafeBandpassKeytrack));
            }
        }

        // A narrow (high-resonance) notch behaves like a bandpass's mirror
        // image: it removes a specific band rather than passing it, but on
        // sparse harmonic content (a near-sine wavetable, a thin sample) a
        // notch that happens to land on the note's fundamental can still
        // strip most of what's audible. Same keytracking fix, gated on
        // resonance so an unresonant/wide notch (which barely dents the
        // spectrum) is left alone.
        constexpr float narrowNotchResonance = 0.5f;
        const bool narrowNotch1 = (type == params::FilterType::notch12 || type == params::FilterType::notch24)
                                 && realValue (params::id::filter1Resonance) > narrowNotchResonance;
        if (narrowNotch1)
            if (auto* param = apvts.getParameter (params::id::filter1Keytrack))
            {
                const auto kt = param->convertFrom0to1 (param->getValue());
                if (kt < 0.9f)
                    param->setValueNotifyingHost (param->convertTo0to1 (0.9f));
            }
        const bool narrowNotch2 = f2Enabled
                                 && (f2Type == params::FilterType::notch12 || f2Type == params::FilterType::notch24)
                                 && realValue (params::id::filter2Resonance) > narrowNotchResonance;
        if (narrowNotch2)
            if (auto* param = apvts.getParameter (params::id::filter2Keytrack))
            {
                const auto kt = param->convertFrom0to1 (param->getValue());
                if (kt < 0.9f)
                    param->setValueNotifyingHost (param->convertTo0to1 (0.9f));
            }

        // Low-pass cut low enough to remove even the note's fundamental is
        // the mirror image of the HP/notch case above.
        const bool lowpass1 = type == params::FilterType::lp12 || type == params::FilterType::lp24;
        if (lowpass1 && cutoff < 150.0f)
            if (auto* param = apvts.getParameter (params::id::filter1Cutoff))
                param->setValueNotifyingHost (
                    param->convertTo0to1 (150.0f + rng.nextFloat() * 850.0f));

        const bool lowpass2 = f2Type == params::FilterType::lp12 || f2Type == params::FilterType::lp24;
        if (f2Enabled && lowpass2 && f2Cutoff < 150.0f)
            if (auto* param = apvts.getParameter (params::id::filter2Cutoff))
                param->setValueNotifyingHost (
                    param->convertTo0to1 (150.0f + rng.nextFloat() * 850.0f));
    }

    // Individual oscillator-level rolls are each bounded, but combinations
    // stack: several slots landing loud together (plus drive/distortion
    // downstream) can produce a headphone-dangerous spike even though no
    // single roll looks unreasonable. Sum the enabled slots' linear gain and,
    // if it clears a budget of one full-scale slot plus headroom, pull every
    // enabled slot's level back by the same dB amount so the rolled balance
    // between slots is preserved, just quieter overall. Gated on the oscillator
    // lock group like the loop/mode guard above, so a locked-oscillators roll
    // doesn't get "fixed" out from under the user's own settings.
    if (oscsUnlocked)
    {
        namespace osc = params::id::osc;
        constexpr float gainBudget = 1.25f; // ~one full-scale slot + headroom

        float gainSum = 0.0f;
        for (int s = 0; s < params::numOscSlots; ++s)
        {
            if (realValue (params::id::oscSlot (s, osc::enable)) < 0.5f)
                continue;
            const auto levelDb = realValue (params::id::oscSlot (s, osc::level));
            gainSum += juce::Decibels::decibelsToGain (levelDb, -60.0f);
        }

        if (gainSum > gainBudget)
        {
            const float trimDb = 20.0f * std::log10 (gainBudget / gainSum);
            for (int s = 0; s < params::numOscSlots; ++s)
            {
                if (realValue (params::id::oscSlot (s, osc::enable)) < 0.5f)
                    continue;
                const auto levelId = params::id::oscSlot (s, osc::level);
                if (auto* param = apvts.getParameter (levelId))
                {
                    const auto trimmed = juce::jlimit (-60.0f, 0.0f,
                                                        realValue (levelId) + trimDb);
                    param->setValueNotifyingHost (param->convertTo0to1 (trimmed));
                }
            }
        }

        // Cause 8 (1.0.25, randomizeNeverSilentChainedTest, chained rolls
        // across the full note range -- the old single-note-60,
        // fresh-processor-per-seed test never hit this): a single enabled
        // oscillator at an individually-reasonable low level (e.g. -14dB, no
        // guard above touches this -- it's not "too loud") measured well
        // under the perceptual floor at every note once an active filter
        // was in the signal path, even though neither the oscillator level
        // nor the filter's own cutoff guard above looked extreme in
        // isolation -- an initial version of this guard tried to name the
        // exact filter shapes at fault (bandpass/notch) and missed a highpass
        // case entirely. Simplified to mirror the gain-budget ceiling just
        // above but as a floor: whenever the enabled oscillators' combined
        // level is this quiet, bring it back up to a floor rather than
        // trying to enumerate every DSP combination that can compound with
        // it. A normal, comfortably-loud roll is untouched (gainSum is
        // almost always well above the floor).
        constexpr float minGainSum = 0.7f; // ~-3.1 dB combined
        if (gainSum < minGainSum && gainSum > 0.0f)
        {
            const float boostDb = 20.0f * std::log10 (minGainSum / gainSum);
            for (int s = 0; s < params::numOscSlots; ++s)
            {
                if (realValue (params::id::oscSlot (s, osc::enable)) < 0.5f)
                    continue;
                const auto levelId = params::id::oscSlot (s, osc::level);
                if (auto* param = apvts.getParameter (levelId))
                {
                    const auto boosted = juce::jlimit (-60.0f, 0.0f,
                                                        realValue (levelId) + boostDb);
                    param->setValueNotifyingHost (param->convertTo0to1 (boosted));
                }
            }
        }
    }

    // FX chain order joins RANDOMIZE ALL (respecting the FX lock), but the
    // limiter keeps its current slot so it stays where the user put it (last by
    // default) rather than being shuffled into the middle of the chain.
    const bool fxUnlocked = (lockedMask & (1u << (int) params::LockGroup::fx)) == 0;
    if (fxUnlocked)
    {
        auto order = getFxOrder();
        const int limiterId = (int) dsp::FXChain::Module::limiter;
        const int limiterPos = order.indexOf (limiterId);
        juce::Array<int> others;
        for (int id : order) if (id != limiterId) others.add (id);
        for (int i = others.size() - 1; i > 0; --i)
            std::swap (others.getReference (i), others.getReference (rng.nextInt (i + 1)));

        juce::Array<int> shuffled;
        int oi = 0;
        for (int pos = 0; pos < order.size(); ++pos)
            shuffled.add (pos == limiterPos ? limiterId : others[oi++]);
        setFxOrder (shuffled);

        // The limiter doubles as the post-randomize safety ceiling: force it
        // on at transparent defaults (not a randomized creative setting) so a
        // painful combination still gets caught. Users can switch it back off.
        namespace fx = params::id::fx;
        const auto resetToDefault = [this] (const juce::String& id)
        {
            if (auto* param = apvts.getParameter (id))
                param->setValueNotifyingHost (param->getDefaultValue());
        };
        if (auto* enableParam = apvts.getParameter (fx::limEnable))
            enableParam->setValueNotifyingHost (1.0f);
        resetToDefault (fx::limDrive);
        resetToDefault (fx::limCeiling);
        resetToDefault (fx::limRelease);
        resetToDefault (fx::limAutoRelease);
        resetToDefault (fx::limCharacter);
        resetToDefault (fx::limStereoLink);
        resetToDefault (fx::limTruePeak);
        resetToDefault (fx::limLookahead);
        resetToDefault (fx::limAutoGain);

        // Hard cap on top of RandomSpec's maxNorm=0.5: sampleRandomValue()
        // deliberately opens a param's range toward 1.0 as wildness rises
        // past 0.5 (see Randomizer.cpp), so maxNorm alone is only a soft
        // bias, not a ceiling. Past the halfway point the Convolve impulse
        // gets progressively thinner, so RANDOMIZE ALL must never land there
        // regardless of wildness -- the user still has the full range by hand.
        if (realValue (fx::convStart) > 0.5f)
            setNorm (fx::convStart, 0.5f);
    }

    // --- Audibility floor -----------------------------------------------------
    // Empirically found via a throwaway multi-thousand-seed sweep (see
    // randomizeNeverSilentTest's history/comment in SPASynthTests.cpp, seeds
    // e.g. 49/405/427 at max wildness) that RANDOMIZE ALL could land on a
    // patch that makes NO sound at all when a note is held, from three
    // independent causes:
    //
    //  1. Amp envelope attack rolled long enough (spec allows up to 10s) that
    //     a short test/user check window sees nothing yet -- not permanently
    //     silent, just too slow to ever feel like "hitting play worked".
    //  2. The mod matrix (rolled at the same time, unless locked) can route a
    //     mostly-static source (velocity, aftertouch, a macro) at a strongly
    //     negative depth onto an oscillator's level (or the amp sustain, or a
    //     filter cutoff), pushing the *effective* normalized value to 0
    //     regardless of the oscillator's own rolled level -- a single route
    //     can hard-mute a slot outright, and independent routes doing this to
    //     every enabled slot silences the whole patch, indefinitely (not just
    //     during the attack).
    //  3. THE DOMINANT CAUSE, found by tracing seeds 49/405/427 all the way
    //     to SPASynthVoice::renderNextBlock (ampEnv genuinely active with a
    //     correct non-zero gain, yet the processor's output stayed at exact
    //     0 for seconds): the arpeggiator. It randomizes independently of the
    //     oscillator/filter/matrix rolls above (its own lock group), and a
    //     held note only ever sounds via the arp's note stream once it is
    //     enabled. `arp.chance` (the per-step "does this step fire" trig
    //     condition) can roll all the way to 0, silencing every step
    //     forever, and `arp.division` can roll as slow as 8 bars/step, so
    //     even at chance=1 a short test/user hold can easily fall entirely
    //     within one silent gap between two steps. Neither is a bug in
    //     isolation (probability-gated steps and slow arps are legitimate
    //     creative choices) but the combination reproduces exactly the "hit
    //     RANDOMIZE ALL, play a note, hear nothing" complaint.
    //
    // All three are fixed here as minimal clamps on the specific values found
    // to cause it, gated by the same lock groups as everything else above so
    // a locked section's rolled values are never second-guessed.
    if (oscsUnlocked)
    {
        // Cause 1: an attack this long (spec max is 10s) means a listener
        // checking "did that make a sound" within any reasonable window hears
        // nothing, even though the patch isn't truly silent. Clamp instead of
        // re-rolling so the rest of the envelope's character survives.
        constexpr float maxAudibleAttackSeconds = 2.5f;
        if (auto* param = apvts.getParameter (params::id::ampAttack))
        {
            const auto attack = param->convertFrom0to1 (param->getValue());
            if (attack > maxAudibleAttackSeconds)
                param->setValueNotifyingHost (param->convertTo0to1 (maxAudibleAttackSeconds));
        }
    }

    const bool matrixUnlocked = (lockedMask & (1u << (int) params::LockGroup::matrix)) == 0;
    if (matrixUnlocked)
    {
        // Cause 2: cap how hard any single route can push a level-ish
        // destination toward its floor. Dropped 0.5 -> 0.4 (1.0.25, found by
        // randomizeAllChainedRealLibraryTest against the real library) --
        // 0.5 still leaves a route's duck/swell clearly audible (matrix
        // modulation is supposed to move things) while a static-ish source
        // at its extreme can no longer, by itself, walk the destination's
        // effective normalized value all the way to 0.
        constexpr float maxRiskyRouteDepth = 0.4f;

        // Cause 2c: grainPos/grainDensity/grainSize joined the risky list --
        // the mod matrix pushing grainPos further toward end-of-file, or
        // grainDensity/grainSize toward a sparse combination, defeats the
        // per-slot floors just added above just as effectively as pushing a
        // level toward 0. Added for every enabled slot regardless of its
        // currently-rolled mode (cheap, and harmless for a slot the mod
        // matrix or a later manual mode change lands in sample/granular).
        juce::Array<int> riskyDests;
        for (int s = 0; s < params::numOscSlots; ++s)
        {
            if (realValue (params::id::oscSlot (s, params::id::osc::enable)) >= 0.5f)
            {
                for (const char* key : { params::id::osc::level, params::id::osc::grainPos,
                                          params::id::osc::grainDensity, params::id::osc::grainSize })
                {
                    const auto idx = params::modDestIndex (params::id::oscSlot (s, key));
                    if (idx >= 0)
                        riskyDests.add (idx);
                }
            }
        }
        if (const auto idx = params::modDestIndex (params::id::ampSustain); idx >= 0)
            riskyDests.add (idx);
        if (realValue (params::id::filter1Enable) >= 0.5f)
            if (const auto idx = params::modDestIndex (params::id::filter1Cutoff); idx >= 0)
                riskyDests.add (idx);
        if (realValue (params::id::filter2Enable) >= 0.5f)
            if (const auto idx = params::modDestIndex (params::id::filter2Cutoff); idx >= 0)
                riskyDests.add (idx);

        for (int r = 0; r < params::numModRoutes; ++r)
        {
            const auto destId = params::id::routeParam (r, params::id::route::dest);
            const auto destChoice = (int) realValue (destId); // 0 = None
            if (destChoice <= 0)
                continue;
            if (! riskyDests.contains (destChoice - 1))
                continue;

            const auto depthId = params::id::routeParam (r, params::id::route::depth);
            const auto depth = realValue (depthId);
            if (std::abs (depth) > maxRiskyRouteDepth)
            {
                const auto clamped = (depth < 0.0f ? -1.0f : 1.0f) * maxRiskyRouteDepth;
                if (auto* param = apvts.getParameter (depthId))
                    param->setValueNotifyingHost (param->convertTo0to1 (clamped));
            }
        }
    }

    const bool chaosUnlocked = (lockedMask & (1u << (int) params::LockGroup::chaos)) == 0;
    if (chaosUnlocked && realValue (params::id::chaos::enable) >= 0.5f
        && realValue (params::id::chaos::ampOn) >= 0.5f)
    {
        // Cause 7 (1.0.25, randomizeAllChainedRealLibraryTest): Organic
        // Chaos's amp drive multiplies the voice's gain by a slow random
        // walk; at the top of ampAmount's range that walk can sit near-zero
        // for long enough that a held note's whole test window reads as
        // silence even though every other stage genuinely has signal.
        // Capped rather than disabled so the drive is still clearly
        // audible -- matches the same "cap this one causal knob, don't ban
        // the section" approach as the mod-matrix depth cap above.
        constexpr float maxSafeChaosAmpAmount = 0.6f;
        if (auto* param = apvts.getParameter (params::id::chaos::ampAmount))
        {
            const auto amount = param->convertFrom0to1 (param->getValue());
            if (amount > maxSafeChaosAmpAmount)
                param->setValueNotifyingHost (param->convertTo0to1 (maxSafeChaosAmpAmount));
        }
    }

    const bool arpUnlocked = (lockedMask & (1u << (int) params::LockGroup::arp)) == 0;
    if (arpUnlocked && realValue (params::id::arp::enable) >= 0.5f)
    {
        // Cause 3a: a step that never fires can silence the arp forever.
        // Raised 0.6 -> 0.85 (1.0.25, randomizeAllChainedRealLibraryTest):
        // 0.6 (40% skip odds/step) was tuned against a single-note,
        // note-60-only sweep and looked astronomically safe on paper, but
        // testing across hundreds of chained rolls and the full note range
        // turned up real cases -- a chord's guaranteed first step (see
        // Arpeggiator's firstStepPending) fires audibly, then a short
        // ampRelease lets it decay away before the next non-skipped step,
        // which a run of several 40%-odds skips in a row can push past.
        // 0.85 (15% skip odds/step) keeps "some steps rest" clearly
        // characterful while cutting the odds of a several-steps-long dry
        // spell by roughly two more orders of magnitude.
        constexpr float minAudibleArpChance = 0.85f;
        if (auto* param = apvts.getParameter (params::id::arp::chance))
        {
            const auto chance = param->convertFrom0to1 (param->getValue());
            if (chance < minAudibleArpChance)
                param->setValueNotifyingHost (param->convertTo0to1 (minAudibleArpChance));
        }

        // Cause 3b: division choice index into lfoDivisionNames() ("8/1",
        // "4/1", "2/1", "1/1", "1/2", ... down to "1/32"). The real offenders
        // were the 1-8 BAR steps (indices 0-3: 8/1, 4/1, 2/1, 1/1 = 32, 16, 8,
        // 4 beats/step) -- multi-second-or-longer gaps a short hold can fall
        // entirely within even at chance=1. Flooring at index 6 ("1/4" = 1
        // beat/step, lfoDivisionBeats) bans only those bar-length steps and
        // leaves every quarter-note-or-faster arp (the overwhelming majority
        // of musically useful settings, including 1/4 and 1/8) untouched.
        // Flooring the index (not the musical rate) guarantees several steps
        // land inside any reasonable hold, so with the chance floor above the
        // odds of every one of them resting at once are negligible.
        constexpr float minAudibleArpDivisionIndex = 6.0f; // "1/4" (1 beat/step)
        if (auto* param = apvts.getParameter (params::id::arp::division))
        {
            const auto division = param->convertFrom0to1 (param->getValue());
            if (division < minAudibleArpDivisionIndex)
                param->setValueNotifyingHost (param->convertTo0to1 (minAudibleArpDivisionIndex));
        }

        // Cause 4, found while diagnosing a leftover silent seed (44 at
        // wildness=1) after 3a/3b above: JUCE's Synthesiser::noteOn(), when
        // retriggered for a note that's still ringing on the same channel,
        // stops that voice and starts a fresh one (see the "hitting a note
        // that's still ringing" branch in juce_Synthesiser.cpp) -- so EVERY
        // same-note arp retrigger snaps that voice's amp envelope back to
        // attack-start. Any arp mode with a repeated note (every mode when
        // only one note is held; several phrase presets, e.g. "Root Pulse"'s
        // {0,0,12,0}) hits this once the step interval is faster than the
        // attack: the envelope perpetually restarts and never rises past a
        // sliver of attack, so the patch never becomes more than barely
        // audible -- not from a step never firing (3a) or a slow division
        // (3b), but from FAST retriggering outrunning a slow attack. Seed 44
        // rolled Phrase/"Root Pulse" + division index 8 ("1/4T", ~0.33s/step
        // at 120bpm) + attack 1.17s and measured peak ~0.0004; reproduced
        // identically with the oscillator's wavetable choice forced back to
        // Basic Shapes, ruling out the oscillator engine. Clamp attack to a
        // fraction of one step's duration at the synth's current tempo so a
        // retrigger always lands comfortably after the envelope has risen.
        // Gated on the envelope lock group (what this actually modifies),
        // independent of whether the arp section itself is locked -- a
        // locked arp's own real division/tempo combination is still read to
        // decide how much headroom a freshly-rolled attack needs.
        const bool envelopesUnlocked =
            (lockedMask & (1u << (int) params::LockGroup::envelopes)) == 0;
        if (envelopesUnlocked)
        {
            const auto divisionIndex = (int) realValue (params::id::arp::division);
            const auto beatsPerStep = params::lfoDivisionBeats (divisionIndex);
            const auto secondsPerBeat = 60.0 / juce::jmax (20.0, getCurrentBpm());
            const auto stepSeconds = beatsPerStep * secondsPerBeat;

            constexpr float maxAttackFractionOfStep = 0.4f;
            const auto maxAttackForArp = juce::jmax (0.001f,
                (float) (stepSeconds * maxAttackFractionOfStep));

            if (auto* param = apvts.getParameter (params::id::ampAttack))
            {
                const auto attack = param->convertFrom0to1 (param->getValue());
                if (attack > maxAttackForArp)
                    param->setValueNotifyingHost (param->convertTo0to1 (maxAttackForArp));
            }
        }
    }

    // Cause 10 (1.0.25) -- CONSIDERED, NOT APPLIED. Mechanical
    // section-by-section bisection of the hermetic residuals that survived
    // every other 1.0.25 fix (randomizeQuietCauseBisectionTest) found
    // ampEnv.sustain at a moderate, non-extreme roll as a repeat co-culprit
    // alongside a sparse arp or a subtractive filter. Before adding a floor
    // for it, re-measured the ONSET (first ~500ms) of each of those 13
    // residuals: most had a clearly audible pluck/attack -- a characterful
    // patch, not a real "RANDOMIZE ALL gave nothing" complaint. Changed
    // randomizeNeverSilentChainedTest's classifier to judge the WHOLE note
    // (silent only if BOTH onset and held tail are quiet) instead of just
    // the held tail, per that finding -- and an anti-vacuous check (a
    // sustain-floor prototype, gated exactly like this comment describes,
    // temporarily forced off) showed all 216 chained renders already pass
    // the -40dBFS/-30dBFS gate on the new classifier WITHOUT any sustain
    // floor. So no floor was needed or added; ampEnv.sustain is left fully
    // randomizable. If a genuinely onset-quiet residual turns up later, the
    // conditional design above (floor only when arp is on or a subtractive
    // filter is engaged, never a blanket floor) is the one to reach for.

    sendChangeMessage();
}

void SPASynthProcessor::installTable (int slot, std::shared_ptr<const dsp::Wavetable> table,
                                     juce::String path, juce::String error)
{
    auto& st = slotTables[(size_t) slot];
    st.error = std::move (error);

    if (table != nullptr)
    {
        retiredTables.push_back (std::move (st.current));
        st.current = std::move (table);
        st.live.store (st.current.get());
        st.path = std::move (path);
    }

    sendChangeMessage();
}

void SPASynthProcessor::loadWavetableFromFile (int slot, const juce::File& file, bool markEdited)
{
    // A content change (1.0.26 edited indicator) -- see markPresetDirty().
    // markEdited: see loadSampleFromFile()'s identical parameter.
    if (markEdited)
        markPresetDirty();

    // Same loading-state + latest-wins bookkeeping as loadSampleFromFile
    // (requestSerial rationale: see that function).
    auto& st = slotTables[(size_t) slot];
    const int serial = ++st.requestSerial;
    st.pendingLoads.fetch_add (1);
    sendChangeMessage();

    // Weak-ref treatment mirrors loadSampleFromFile — see the comment there.
    // activeBackgroundThreads bookkeeping: see ~SPASynthProcessor().
    juce::WeakReference<SPASynthProcessor> weak (this);
    ++activeBackgroundThreads;
    juce::Thread::launch ([weak, slot, file, serial]
    {
        // 1.0.25: skip the load entirely if superseded before it started —
        // see setBuiltInWavetable's identical check for the full rationale.
        if (weak == nullptr || serial != weak->slotTables[(size_t) slot].requestSerial.load())
        {
            if (weak != nullptr)
            {
                weak->slotTables[(size_t) slot].pendingLoads.fetch_sub (1);
                --weak->activeBackgroundThreads;
            }
            return;
        }

        auto result = dsp::loadWavetableFromFile (file);

        juce::MessageManager::callAsync ([weak, slot, serial, loaded = std::move (result),
                                          path = file.getFullPathName()]() mutable
        {
            if (weak == nullptr)
                return;   // processor was destroyed while this load was in flight
            auto& t = weak->slotTables[(size_t) slot];
            t.pendingLoads.fetch_sub (1);
            if (serial != t.requestSerial)
                return;   // superseded by a newer request — drop this stale result
            weak->installTable (slot, std::move (loaded.table), path, loaded.error);
        });

        if (weak != nullptr)
            --weak->activeBackgroundThreads;
    });
}

void SPASynthProcessor::setFactoryWavetable (int slot)
{
    installTable (slot, factoryTable, {}, {});
}

void SPASynthProcessor::setBuiltInWavetable (int slot, int tableChoice)
{
    const auto choice = juce::jlimit (0, dsp::numWavetableTableChoices - 1, tableChoice);

    // Basic Shapes is always ready (built in the ctor); any other choice
    // already built once for this processor is cached and equally instant.
    // Still bump requestSerial: an EARLIER selection on this slot may have a
    // background build in flight, and without this its callback's
    // serial-match check (below) would not notice it has been superseded --
    // it would land after this instant selection and silently overwrite it
    // with stale content once it completes.
    if (choice == 0 || builtInTables[(size_t) choice] != nullptr)
    {
        ++slotTables[(size_t) slot].requestSerial;
        installTable (slot, builtInTables[(size_t) choice], {}, {});
        return;
    }

    // First selection of this choice: build it on a background thread, same
    // loading-state (pendingLoads/isWavetableLoading) + latest-wins
    // (requestSerial) bookkeeping as loadWavetableFromFile -- see that
    // function's comment. The built table is cached in builtInTables
    // regardless of whether this particular request is superseded, so a
    // later reselect (this slot or another) never rebuilds it.
    auto& st = slotTables[(size_t) slot];
    const int serial = ++st.requestSerial;
    st.pendingLoads.fetch_add (1);
    sendChangeMessage();

    juce::WeakReference<SPASynthProcessor> weak (this);
    ++activeBackgroundThreads;
    juce::Thread::launch ([weak, slot, choice, serial]
    {
        // 1.0.25: skip the build ENTIRELY if a newer request for this slot
        // already landed before this thread got scheduled -- found while
        // chasing randomizeNeverSilentChainedTest residuals under chained
        // RANDOMIZE ALL: rapid re-rolls of a slot's Table choice each
        // launched their own thread, all queued behind the FFT-serializing
        // mutex below, so a slot could sit `isWavetableLoading() == true`
        // (playing its previous table/Basic Shapes, not silence -- see
        // installTable, which only ever writes `live` on a successful
        // build) for as long as it took every superseded request ahead of
        // it to grind through a real build it would just discard. This
        // check is safe from a background thread: requestSerial is atomic,
        // and `weak` nulling out on the message thread is the same
        // established pattern already used for the activeBackgroundThreads
        // decrement below.
        if (weak == nullptr || serial != weak->slotTables[(size_t) slot].requestSerial.load())
        {
            if (weak != nullptr)
            {
                weak->slotTables[(size_t) slot].pendingLoads.fetch_sub (1);
                --weak->activeBackgroundThreads;
            }
            return;
        }

        // Serialize actual table generation: WavetableFactory::build() goes
        // through juce::dsp::FFT (Wavetable::fromSpectra), and on some
        // platforms/backends an FFT engine has shared/cached setup state
        // that is not safe under truly concurrent use from independent FFT
        // instances on different threads. Multiple slots/processors can each
        // kick off a background build around the same moment (e.g. several
        // oscillator slots randomized to new table choices at once), so
        // without this, concurrent builds could silently corrupt each
        // other's result -- reproduced as a deterministic-per-race, garbage
        // (near-silent) built-in wavetable under back-to-back RANDOMIZE ALL
        // stress (randomizeNeverSilentTest, seed 37 @ wildness 1.0). Building
        // is rare and cheap (a handful of ms), so serializing it here costs
        // nothing that matters.
        static std::mutex buildMutex;
        std::unique_ptr<const dsp::Wavetable> built;
        {
            const std::lock_guard<std::mutex> lock (buildMutex);
            built = std::make_unique<const dsp::Wavetable> (dsp::WavetableFactory::build (choice));
        }
        std::shared_ptr<const dsp::Wavetable> table (std::move (built));

        juce::MessageManager::callAsync ([weak, slot, choice, serial, table]() mutable
        {
            if (weak == nullptr)
                return;   // processor was destroyed while this build was in flight
            weak->builtInTables[(size_t) choice] = table;
            auto& t = weak->slotTables[(size_t) slot];
            t.pendingLoads.fetch_sub (1);
            if (serial != t.requestSerial)
                return;   // superseded by a newer request on this slot -- drop it
            weak->installTable (slot, std::move (table), {}, {});
        });

        if (weak != nullptr)
            --weak->activeBackgroundThreads;
    });
}

void SPASynthProcessor::applyBuiltInWavetableFromParam (int slot)
{
    if (slot < 0 || slot >= params::numOscSlots)
        return;
    const auto pid = params::id::oscSlot (slot, params::id::osc::table);
    if (auto* v = apvts.getRawParameterValue (pid))
        setBuiltInWavetable (slot, (int) v->load());
}

juce::String SPASynthProcessor::getWavetableName (int slot) const
{
    const auto& current = slotTables[(size_t) slot].current;
    return current != nullptr ? current->getName() : juce::String();
}

juce::String SPASynthProcessor::getWavetableError (int slot) const
{
    return slotTables[(size_t) slot].error;
}

void SPASynthProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    hostSampleRate = sampleRate;
    hostBlockSize = samplesPerBlock;
    midiClock.prepare (sampleRate);   // tempo detection stays in the host domain

    const int factor = 1 << juce::jlimit (0, 3, (int) raw.oversampling->load());
    currentOsFactor = factor;
    pendingOsFactor.store (factor, std::memory_order_relaxed);
    prepareEngine (sampleRate * factor, samplesPerBlock * factor);

    if (factor > 1)
    {
        oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
            2, (size_t) juce::roundToInt (std::log2 ((double) factor)),
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, false, false);
        oversampler->initProcessing ((size_t) samplesPerBlock);
        oversampler->reset();
        osLatencyHost = (int) std::ceil (oversampler->getLatencyInSamples());
    }
    else
    {
        oversampler.reset();
        osLatencyHost = 0;
    }
}

// All rate-dependent engine setup, at the (possibly oversampled) engine rate.
void SPASynthProcessor::prepareEngine (double engineRate, int engineBlock)
{
    currentSampleRate = engineRate;
    synth.setCurrentPlaybackSampleRate (engineRate);
    arp.prepare (engineRate);
    scaledMidi.ensureSize (8192);   // no audio-thread allocation; see Arpeggiator::scratch
    bypassMidi.ensureSize (8192);   // same rationale; see processBlockBypassed
    fxChain.prepare (engineRate, engineBlock);

    paraEnv.setSampleRate (engineRate);
    paraEnv.reset();
    paraGateWasOn = false;
    paraEnvBuf.setSize (1, juce::jmax (1, engineBlock), false, false, true);
    masterGain.reset (engineRate, 0.02);
    masterGain.setCurrentAndTargetValue (
        juce::Decibels::decibelsToGain (raw.masterGain->load(), -60.0f));
}

// Swap the oversampling factor (message thread; processing suspended by caller).
void SPASynthProcessor::rebuildOversampling (int factor)
{
    factor = 1 << juce::jlimit (0, 3, (int) std::round (std::log2 ((double) factor)));
    currentOsFactor = factor;
    prepareEngine (hostSampleRate * factor, hostBlockSize * factor);

    if (factor > 1)
    {
        oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
            2, (size_t) juce::roundToInt (std::log2 ((double) factor)),
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, false, false);
        oversampler->initProcessing ((size_t) hostBlockSize);
        oversampler->reset();
        osLatencyHost = (int) std::ceil (oversampler->getLatencyInSamples());
    }
    else
    {
        oversampler.reset();
        osLatencyHost = 0;
    }
}

// Paraphonic pre-pass + voices + FX + master, at whatever rate `buffer` is
// sized for. Called on the host buffer directly, or on the oversampled buffer.
void SPASynthProcessor::renderEngine (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    // Paraphonic: advance one shared amp envelope from the collective key count
    // and render it per-sample for the voices to read this block. Gated on the
    // key count from the previous block, so voices attack one block after the
    // first key lands (imperceptible, and keeps this a simple pre-pass).
    if (shared.voiceMode == params::VoiceMode::paraphonic)
    {
        const bool anyKey = glideState.keysDown > 0;
        if (anyKey != paraGateWasOn)
        {
            if (anyKey) paraEnv.noteOn(); else paraEnv.noteOff();
            paraGateWasOn = anyKey;
        }
        paraEnv.setParameters ({ raw.ampAttack->load(), raw.ampDecay->load(),
                                 raw.ampSustain->load(), raw.ampRelease->load() });
        const int n = buffer.getNumSamples();
        auto* pe = paraEnvBuf.getWritePointer (0);
        for (int i = 0; i < n; ++i) pe[i] = paraEnv.getNextSample();
        shared.paraEnvBlock = pe;
        shared.paraGateActive = anyKey;
    }
    else
    {
        shared.paraEnvBlock = nullptr;
        shared.paraGateActive = false;
        if (paraGateWasOn) { paraEnv.reset(); paraGateWasOn = false; }
    }

    synth.renderNextBlock (buffer, midi, 0, buffer.getNumSamples());

    updateFXParams();
    // Intentional: the FX chain runs on whatever rate `buffer` is at, which at
    // higher oversampling factors is the OVERSAMPLED rate (up to 8x, so up to
    // ~384kHz on a 128-sample host buffer) — not just the synth engines. This
    // matches the "whole-synth oversampling" branding (everything downstream
    // of the oscillators, including reverb/EQ/convolution/limiter, benefits
    // from the higher internal rate), but it means CPU cost for the ENTIRE FX
    // chain scales with the oversampling factor, not just the voice count. A
    // user running 8x oversampling with a heavy chain (convolution + reverb +
    // EQ all active) should expect a real, compounding CPU cost from this —
    // kept as a deliberate design tradeoff, not an oversight.
    fxChain.process (buffer, fxParams);

    masterGain.setTargetValue (juce::Decibels::decibelsToGain (raw.masterGain->load(), -60.0f));
    masterGain.applyGain (buffer, buffer.getNumSamples());
}

void SPASynthProcessor::setCustomLfoShape (int lfoIndex, const dsp::CustomLFOShape& shape, bool markEdited)
{
    // Single-writer double buffer (message thread only -- see the class
    // comment on CustomLfoStorage). Write the FULL new shape into whichever
    // buffer is not currently live, then publish it with one atomic pointer
    // store; the audio thread only ever reads a complete, already-published
    // shape via updateSharedState's once-per-block load, never a
    // half-written one. Both buffers are fixed members (no allocation), so
    // this is safe to call from the audio thread's perspective at any time,
    // including while voices are actively rendering.
    auto& storage = customLfo[(size_t) lfoIndex];
    const auto* liveNow = storage.live.load();
    auto& target = storage.buffers[liveNow == &storage.buffers[0] ? 1 : 0];
    target = shape;
    storage.live.store (&target);

    if (markEdited)
        markPresetDirty();   // a content change (1.0.26 edited indicator)
}

void SPASynthProcessor::setInternalBpm (double bpm)
{
    bpm = juce::jlimit (20.0, 300.0, bpm);
    internalBpm.store (bpm, std::memory_order_relaxed);
    apvts.state.setProperty ("standaloneBpm", bpm, nullptr);
}

void SPASynthProcessor::setTempoSyncMode (int mode)
{
    tempoSyncMode.store (mode, std::memory_order_relaxed);
    apvts.state.setProperty ("tempoSyncMode", mode, nullptr);
}

void SPASynthProcessor::setFxOrder (const juce::Array<int>& moduleIds)
{
    if (moduleIds.size() != dsp::FXChain::numModules)
        return;
    dsp::FXChain::Module ord[dsp::FXChain::numModules];
    for (int i = 0; i < dsp::FXChain::numModules; ++i)
        ord[i] = (dsp::FXChain::Module) moduleIds[i];
    const auto packed = dsp::FXChain::packOrder (ord);
    fxOrderPacked.store (packed, std::memory_order_relaxed);
    apvts.state.setProperty ("fxOrder", (juce::int64) packed, nullptr);
    markPresetDirty();   // a content change (1.0.26 edited indicator); also
                          // covers RANDOMIZE ALL's own FX-order shuffle,
                          // which goes through this same setter
}

juce::Array<int> SPASynthProcessor::getFxOrder() const
{
    dsp::FXChain::Module ord[dsp::FXChain::numModules];
    dsp::FXChain::unpackOrder (fxOrderPacked.load (std::memory_order_relaxed), ord);
    juce::Array<int> ids;
    for (auto m : ord)
        ids.add ((int) m);
    return ids;
}

void SPASynthProcessor::loadConvolutionIR (const juce::File& file)
{
    convIrPath = file.existsAsFile() ? file.getFullPathName() : juce::String();
    fxChain.loadConvolutionIR (file);
    const auto libraryRoot = library::findLibraryRoot();
    apvts.state.setProperty ("convIR",
        convIrPath.isEmpty() ? juce::String() : library::toPortable (file, libraryRoot),
        nullptr);
    markPresetDirty();   // a content change (1.0.26 edited indicator)
    sendChangeMessage();   // refresh the IR name in the UI
}

void SPASynthProcessor::updateFXParams()
{
    const auto& rf = raw.fx;
    auto& p = fxParams;

    dsp::FXChain::unpackOrder (fxOrderPacked.load (std::memory_order_relaxed), p.order);

    p.distEnable     = rf.distEnable->load() >= 0.5f;
    p.distType       = (int) rf.distType->load();
    p.distDrive      = rf.distDrive->load();
    p.distToneHz     = rf.distTone->load();
    p.distMix        = rf.distMix->load();
    p.chorusEnable   = rf.chorusEnable->load() >= 0.5f;
    p.chorusRate     = rf.chorusRate->load();
    p.chorusDepth    = rf.chorusDepth->load();
    p.chorusFeedback = rf.chorusFeedback->load();
    // The only percent -> 0..1 conversion for chorus width: the registry
    // carries it as 0..100 % (that is how the knob reads), the DSP wants
    // 0..1. Same shape as the loop-crossfade/LFO smooth+jitter conversions.
    p.chorusWidth    = rf.chorusWidth->load() * 0.01f;
    p.chorusMode     = (int) rf.chorusMode->load();
    p.chorusMix      = rf.chorusMix->load();
    p.delayEnable    = rf.delayEnable->load() >= 0.5f;
    p.delaySync      = rf.delaySync->load() >= 0.5f;
    p.delayTimeMs    = rf.delayTime->load();
    p.delayDivision  = (int) rf.delayDivision->load();
    p.delayFeedback  = rf.delayFeedback->load();
    p.delayPingPong  = rf.delayPingPong->load() >= 0.5f;
    // Same percent -> 0..1 conversion as chorusWidth (registry carries %, DSP wants 0..1).
    p.delayWidth     = rf.delayWidth->load() * 0.01f;
    p.delayMix       = rf.delayMix->load();
    p.reverbEnable   = rf.reverbEnable->load() >= 0.5f;
    p.reverbMode     = (int) rf.reverbMode->load();
    p.reverbPreDelay = rf.reverbPreDelay->load();
    p.reverbSize     = rf.reverbSize->load();
    p.reverbDecay    = rf.reverbDecay->load();
    p.reverbDamping  = rf.reverbDamping->load();
    p.reverbModDepth = rf.reverbModDepth->load();
    p.reverbLowCut   = rf.reverbLowCut->load();
    p.reverbHighCut  = rf.reverbHighCut->load();
    p.reverbWidth    = rf.reverbWidth->load();
    p.reverbMix      = rf.reverbMix->load();
    p.eqEnable       = rf.eqEnable->load() >= 0.5f;
    p.eqCharacter    = (int) rf.eqCharacter->load();
    for (int b = 0; b < 8; ++b)
    {
        const auto& bp = rf.eqBands[(size_t) b];
        auto& band = p.eqBands[(size_t) b];
        band.enabled = bp.enable->load() >= 0.5f;
        band.type    = (int) bp.type->load();
        band.slope   = (int) bp.slope->load();
        band.freq    = bp.freq->load();
        band.gainDb  = bp.gain->load();
        band.q       = bp.q->load();
    }

    p.modEnable   = rf.modEnable->load() >= 0.5f;
    p.modType     = (int) rf.modType->load();
    p.modRate     = rf.modRate->load();
    p.modSync     = rf.modSync->load() >= 0.5f;
    p.modDivision = (int) rf.modDivision->load();
    p.modDepth    = rf.modDepth->load();
    p.modFeedback = rf.modFeedback->load();
    {
        static constexpr int stageCounts[] = { 2, 4, 6, 8, 12 };
        p.modStages = stageCounts[juce::jlimit (0, 4, (int) rf.modStages->load())];
    }
    p.modCentreHz = rf.modCentre->load();
    p.modManualMs = rf.modManual->load();
    p.modWidth    = rf.modWidth->load();
    p.modMix      = rf.modMix->load();

    p.tremEnable   = rf.tremEnable->load() >= 0.5f;
    p.tremRate     = rf.tremRate->load();
    p.tremSync     = rf.tremSync->load() >= 0.5f;
    p.tremDivision = (int) rf.tremDivision->load();
    p.tremDepth    = rf.tremDepth->load();
    p.tremShape    = (int) rf.tremShape->load();
    p.tremStereo   = rf.tremStereo->load();
    p.tremMix      = rf.tremMix->load();
    p.vibEnable    = rf.vibEnable->load() >= 0.5f;
    p.vibRate      = rf.vibRate->load();
    p.vibSync      = rf.vibSync->load() >= 0.5f;
    p.vibDivision  = (int) rf.vibDivision->load();
    p.vibDepth     = rf.vibDepth->load();
    p.vibMix       = rf.vibMix->load();

    p.limEnable      = rf.limEnable->load() >= 0.5f;
    p.limDrive       = rf.limDrive->load();
    p.limCeiling     = rf.limCeiling->load();
    p.limRelease     = rf.limRelease->load();
    p.limAutoRelease = rf.limAutoRelease->load() >= 0.5f;
    p.limCharacter   = (int) rf.limCharacter->load();
    p.limStereoLink  = rf.limStereoLink->load();
    p.limTruePeak    = rf.limTruePeak->load() >= 0.5f;
    p.limLookahead   = rf.limLookahead->load() >= 0.5f;
    p.limAutoGain    = rf.limAutoGain->load() >= 0.5f;

    p.convEnable   = rf.convEnable->load() >= 0.5f;
    p.convMix      = rf.convMix->load();
    p.convWidth    = rf.convWidth->load();
    p.convPreDelay = rf.convPreDelay->load();
    p.convDecay    = rf.convDecay->load();
    p.convDamping  = rf.convDamping->load();
    p.convStart    = rf.convStart->load();

    desiredLatency.store (fxChain.limiterLatencySamples (p), std::memory_order_relaxed);
    p.bpm            = shared.bpm;
}

bool SPASynthProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono();
}

void SPASynthProcessor::scanMidiControllers (const juce::MidiBuffer& midi)
{
    for (const auto metadata : midi)
    {
        const auto m = metadata.getMessage();

        if (m.isController() && m.getControllerNumber() == 1)
            lastModWheel = (float) m.getControllerValue() / 127.0f;
        else if (m.isChannelPressure())
            lastAftertouch = (float) m.getChannelPressureValue() / 127.0f;
    }

    shared.modWheel = lastModWheel;
    shared.aftertouch = lastAftertouch;
}

void SPASynthProcessor::updateSharedState (int blockLength)
{
    for (int s = 0; s < params::numOscSlots; ++s)
    {
        const auto& rs = raw.slots[(size_t) s];
        auto& slot = shared.slots[(size_t) s];

        slot.enabled     = rs.enable->load() >= 0.5f;
        slot.mode        = (params::OscMode) (int) rs.mode->load();
        slot.table       = slotTables[(size_t) s].live.load();
        slot.sample      = slotSamples[(size_t) s].live.load();
        slot.phase       = rs.phase->load();
        slot.phaseMode   = (params::PhaseMode) (int) rs.phaseMode->load();
        slot.unisonCount = (int) rs.unisonCount->load();
        slot.sampleStart = rs.sampleStart->load();
        slot.loop        = rs.loop->load() >= 0.5f;
        slot.loopStart   = rs.loopStart->load();
        slot.loopEnd     = rs.loopEnd->load();
        // Param is 0..100%, the DSP/voice side wants 0..1 -- normalize here,
        // the one place that conversion happens.
        slot.loopXfade   = rs.loopXfade->load() * 0.01f;
        slot.keytrack    = rs.keytrack->load() >= 0.5f;
        slot.rootNote    = (int) rs.rootNote->load();
        slot.grainPitch  = rs.grainPitch->load();
        slot.analogShape = (int) rs.analogShape->load();
        slot.subLevel    = rs.sub->load();
        slot.fmRatio     = rs.fmRatio->load();
        slot.noiseColor  = (int) rs.noiseColor->load();
        slot.filterRoute = rs.filterRoute->load() >= 0.5f;

        // SYNC has no effect while LOOP is off -- keep the raw param value
        // (rs.syncToBpm) but gate what actually reaches the engine so nothing
        // keeps stretching silently once LOOP is switched off.
        slot.syncToBpm = slot.loop && rs.syncToBpm->load() >= 0.5f;
        {
            const auto beatsOverride = rs.syncBeatsOverride->load();
            if (beatsOverride > 0.0f && slot.sample != nullptr && slot.sample->lengthSeconds() > 1.0e-6)
                slot.nativeBpm = 60.0 * (double) beatsOverride / slot.sample->lengthSeconds();
            else
                slot.nativeBpm = slot.sample != nullptr ? slot.sample->detectedBpm : 120.0;

            slot.gridBeatSeconds = slot.nativeBpm > 1.0e-6 ? 60.0 / slot.nativeBpm : 0.5;
            slot.gridOffsetSeconds = slot.sample != nullptr ? slot.sample->firstOnsetSeconds : 0.0;

            // Per-slot time signature for THIS slot's transport-lock bar
            // origin (see SPASynthVoice's use of SlotStatic::beatsPerBar).
            // "Host" (0) falls back to the block's resolved host/global
            // beatsPerBar (blockBeatsPerBar, set above from the host's own
            // reported signature or global.timeSig); 1-7 map to
            // timeSigBeatsPerBar(choice-1) -- lets one oscillator run its
            // own meter against the project (or another oscillator) for
            // polyrhythms.
            const auto slotTimeSigChoice = juce::roundToInt (rs.timeSig->load());
            slot.beatsPerBar = slotTimeSigChoice > 0
                ? params::id::timeSigBeatsPerBar (slotTimeSigChoice - 1)
                : blockBeatsPerBar;
        }
    }

    shared.hostPlaying = blockPlaying;
    shared.hostTransportValid = blockGotHostPpq;
    shared.hostPpqBeats = blockPpq;
    shared.beatsPerBar = blockBeatsPerBar;

    shared.glideMode = (params::GlideMode) (int) raw.glideMode->load();
    shared.glideTimeMs = raw.glideTime->load();
    shared.voiceMode = (params::VoiceMode) (int) raw.voiceMode->load();
    shared.notePriority = (params::NotePriority) (int) raw.notePriority->load();
    shared.unisonVoices = (int) raw.unisonVoices->load();
    shared.unisonDetuneCents = raw.unisonDetune->load();
    shared.unisonWidth = raw.unisonWidth->load();

    shared.filter1Enabled = raw.filter1Enable->load() >= 0.5f;
    shared.filterType = (params::FilterType) (int) raw.filterType->load();
    shared.filterKeytrack = raw.filterKeytrack->load();
    shared.filter2Enabled = raw.filter2Enable->load() >= 0.5f;
    shared.filter2Type = (params::FilterType) (int) raw.filter2Type->load();
    shared.filter2Keytrack = raw.filter2Keytrack->load();
    shared.filterParallel = raw.filterRouting->load() >= 0.5f;

    // Normalized base values for every mod destination.
    const auto& dests = params::modDestinations();
    for (size_t d = 0; d < dests.size(); ++d)
        shared.baseNorm[d] = dests[d].def->range.convertTo0to1 (raw.dests[d]->load());

    // Compact list of routes that actually do something.
    shared.numActiveRoutes = 0;
    for (int r = 0; r < params::numModRoutes; ++r)
    {
        const auto& rr = raw.routes[(size_t) r];
        const auto source = (int) rr.source->load();
        const auto destChoice = (int) rr.dest->load();
        const auto depth = rr.depth->load();

        if (source == 0 || destChoice == 0 || depth == 0.0f)
            continue;

        auto& route = shared.routes[(size_t) shared.numActiveRoutes++];
        route.source = source;
        route.destIndex = destChoice - 1;  // choice 0 is "None"
        route.depth = depth;
    }

    // Transport (resolved once in processBlock: host, internal, or MIDI clock).
    shared.bpm = blockBpm;

    // LFO params + global free-running phases (value at block start; advanced
    // past the block for next time).
    for (int i = 0; i < params::numLFOs; ++i)
    {
        const auto& rl = raw.lfos[(size_t) i];
        auto& lp = shared.lfo[(size_t) i];
        lp.shape       = (params::LFOShape) (int) rl.shape->load();
        lp.rateHz      = rl.rate->load();
        lp.sync        = rl.sync->load() >= 0.5f;
        lp.division    = (int) rl.division->load();
        lp.phaseOffset = rl.phase->load();
        lp.retrig      = rl.retrig->load() >= 0.5f;
        lp.unipolar    = rl.unipolar->load() >= 0.5f;
        // Params store 0..100%; the DSP wants 0..1 -- the one place this
        // conversion happens (mirrors the loopXfade pattern below).
        lp.smooth      = rl.smooth->load() * 0.01f;
        lp.jitter      = rl.jitter->load() * 0.01f;
        lp.custom      = customLfo[(size_t) i].live.load();

        const auto inc = (double) dsp::LFO::effectiveRateHz (lp, shared.bpm) / currentSampleRate;
        lfoPhaseAccum[(size_t) i] = std::fmod (lfoPhaseAccum[(size_t) i], 1.0);
        shared.lfoGlobalPhase[(size_t) i] = lfoPhaseAccum[(size_t) i];
        lfoPhaseAccum[(size_t) i] += inc * blockLength;
    }

    for (int m = 0; m < params::numMacros; ++m)
        shared.macros[(size_t) m] = raw.macros[(size_t) m]->load();

    auto& ch = shared.chaos;
    ch.enabled          = raw.chaos.enable->load() >= 0.5f;
    ch.pitchOn          = raw.chaos.pitchOn->load() >= 0.5f;
    ch.pitchAmountCents = raw.chaos.pitchAmount->load();
    ch.phaseOn          = raw.chaos.phaseOn->load() >= 0.5f;
    ch.phaseAmount      = raw.chaos.phaseAmount->load();
    ch.positionOn       = raw.chaos.positionOn->load() >= 0.5f;
    ch.positionAmount   = raw.chaos.positionAmount->load();
    ch.ampOn            = raw.chaos.ampOn->load() >= 0.5f;
    ch.ampAmount        = raw.chaos.ampAmount->load();
    ch.satOn            = raw.chaos.satOn->load() >= 0.5f;
    ch.saturation       = raw.chaos.saturation->load();
    ch.distOn           = raw.chaos.distOn->load() >= 0.5f;
    ch.distortion       = raw.chaos.distortion->load();
    ch.syncToBpm        = raw.chaos.syncToBpm->load() >= 0.5f;
    ch.division         = (int) raw.chaos.division->load();
}

void SPASynthProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    // MIDI Learn diagnostics: count every incoming message by type exactly as
    // it arrives from the host, before anything (keyboard-state merge,
    // oversampling scale, arp rewrite) can touch the buffer. This is what
    // lets the UI badge tell Mike not just whether MIDI is reaching the
    // plugin in Logic, but WHAT KIND -- e.g. notes arriving while CC stays at
    // 0 means the controller's knobs never reach processBlock at all.
    for (const auto md : midi)
    {
        const auto m = md.getMessage();
        if (m.isController())
        {
            telemetry.midiCcSeen.fetch_add (1, std::memory_order_relaxed);
            telemetry.lastCcNumber.store (m.getControllerNumber(), std::memory_order_relaxed);
            telemetry.lastCcChannel.store (m.getChannel(), std::memory_order_relaxed);
        }
        else if (m.isNoteOn())
            telemetry.midiNoteOnSeen.fetch_add (1, std::memory_order_relaxed);
        else if (m.isPitchWheel())
        {
            telemetry.midiPitchWheelSeen.fetch_add (1, std::memory_order_relaxed);
            telemetry.lastPitchBendChannel.store (m.getChannel(), std::memory_order_relaxed);
        }
        else if (m.isChannelPressure())
        {
            telemetry.midiChannelPressureSeen.fetch_add (1, std::memory_order_relaxed);
            telemetry.lastAftertouchChannel.store (m.getChannel(), std::memory_order_relaxed);
        }
        else if (m.isAftertouch())
        {
            telemetry.midiAftertouchSeen.fetch_add (1, std::memory_order_relaxed);
            telemetry.lastAftertouchChannel.store (m.getChannel(), std::memory_order_relaxed);
        }
        else if (m.isProgramChange())
            telemetry.midiProgramChangeSeen.fetch_add (1, std::memory_order_relaxed);
        else if (m.isSysEx())
            telemetry.midiSysExSeen.fetch_add (1, std::memory_order_relaxed);
        else
            telemetry.midiOtherSeen.fetch_add (1, std::memory_order_relaxed);
    }

    // Merge the on-screen / computer keyboard's notes into the host MIDI stream
    // before anything consumes it. MidiKeyboardState briefly locks here; that is
    // the standard JUCE idiom for an on-screen keyboard, the lock is held only
    // to splice a couple of queued note events and is uncontended in practice.
    keyboardState.processNextMidiBuffer (midi, 0, buffer.getNumSamples(), true);

    // Panic: from the UI button or an incoming All Sound/Notes Off (CC 120/123).
    // Kill every voice with no tail-off and clear the arp's latched/held chord so
    // a stuck (e.g. latched) note cannot keep sounding.
    bool doPanic = panicRequested.exchange (false, std::memory_order_relaxed);
    for (const auto md : midi)
    {
        const auto m = md.getMessage();
        if (m.isController()
            && (m.getControllerNumber() == 120 || m.getControllerNumber() == 123))
            doPanic = true;
    }
    if (doPanic)
    {
        synth.allNotesOff (0, false);   // channel <= 0 = all voices, no tail-off
        arp.reset();
        keyboardState.allNotesOff (0);
    }

    // Resolve tempo + transport once per block: host playhead if it provides a
    // tempo (plugin), otherwise the standalone internal BPM or external MIDI
    // clock. Everything tempo-synced (arp, delay, LFOs) reads blockBpm below.
    midiClock.process (midi, buffer.getNumSamples());
    blockBpm = 120.0; blockPlaying = true; blockPpq = 0.0;
    bool gotHostTempo = false, gotHostPpq = false;
    blockGotHostTimeSig = false;
    int hostTimeSigNum = 4, hostTimeSigDen = 4;
    if (auto* playHead = getPlayHead())
        if (const auto position = playHead->getPosition())
            if (const auto bpm = position->getBpm())
            {
                blockBpm = *bpm;
                blockPlaying = position->getIsPlaying();
                if (const auto ppq = position->getPpqPosition())
                    { blockPpq = *ppq; gotHostPpq = true; }
                if (const auto ts = position->getTimeSignature())
                {
                    hostTimeSigNum = ts->numerator;
                    hostTimeSigDen = juce::jmax (1, ts->denominator);
                    blockGotHostTimeSig = true;
                }
                gotHostTempo = true;
            }
    if (! gotHostTempo)   // standalone / host without tempo
    {
        if (tempoSyncMode.load (std::memory_order_relaxed) == 1 && midiClock.hasClock())
        {
            blockBpm = midiClock.bpm();
            blockPlaying = midiClock.isPlaying();
        }
        else
        {
            blockBpm = internalBpm.load (std::memory_order_relaxed);
            blockPlaying = true;   // internal clock free-runs
        }
    }
    currentBpm.store (blockBpm, std::memory_order_relaxed);
    // Mirrored into Telemetry (the audio->UI channel DisplayComponent
    // subclasses already read) so FXDisplay's synced Delay/Mod/Trem/Vib
    // drawings can use the real resolved tempo instead of a fixed fallback.
    telemetry.bpm.store ((float) blockBpm, std::memory_order_relaxed);
    blockGotHostPpq = gotHostPpq;

    // Time signature: the host's own value wins when it reports one; otherwise
    // the global.timeSig setting (standalone tempo bar / settings menu for
    // hosts that don't report a signature). Feeds sample LOOP+SYNC's bar-line
    // phase-lock origin (beatsPerBar) and the standalone tempo bar readout.
    if (blockGotHostTimeSig)
    {
        blockBeatsPerBar = (float) hostTimeSigNum * (4.0f / (float) hostTimeSigDen);
    }
    else
    {
        blockBeatsPerBar = raw.timeSig != nullptr
                          ? params::id::timeSigBeatsPerBar ((int) raw.timeSig->load())
                          : 4.0f;
    }
    currentBeatsPerBar.store (blockBeatsPerBar, std::memory_order_relaxed);
    hostReportsTimeSig.store (blockGotHostTimeSig, std::memory_order_relaxed);

    scanMidiControllers (midi);
    midiLearn->processMidi (midi);

    // Below the tempo/CC layer the whole engine runs at the engine rate, which
    // equals the host rate unless oversampling is on. Ask the message thread to
    // rebuild the engine if the factor changed (done under the callback lock).
    pendingOsFactor.store (1 << juce::jlimit (0, 3, (int) raw.oversampling->load()),
                           std::memory_order_relaxed);

    const int factor = currentOsFactor;
    const int hostN = buffer.getNumSamples();
    const int engN = hostN * factor;

    // Scale MIDI into the (possibly oversampled) engine sample domain.
    scaledMidi.clear();
    if (factor > 1)
        for (const auto md : midi)
            scaledMidi.addEvent (md.getMessage(), md.samplePosition * factor);
    juce::MidiBuffer& engMidi = factor > 1 ? scaledMidi : midi;

    // Arpeggiator transforms the note stream ahead of the synth (engine domain).
    {
        dsp::Arpeggiator::Params ap;
        ap.enable       = raw.arp.enable->load() >= 0.5f;
        ap.mode         = (params::ArpMode) (int) raw.arp.mode->load();
        ap.division     = (int) raw.arp.division->load();
        ap.octaves      = (int) raw.arp.octaves->load();
        ap.gate         = raw.arp.gate->load();
        ap.swing        = raw.arp.swing->load();
        ap.latch        = raw.arp.latch->load() >= 0.5f;
        ap.phrase       = (int) raw.arp.phrase->load();
        ap.velocityMode = (int) raw.arp.velMode->load();
        ap.chance       = raw.arp.chance->load();
        ap.stutter      = raw.arp.stutter->load();
        ap.jump         = raw.arp.jump->load();
        ap.humanize     = raw.arp.humanize->load();
        ap.bpm             = blockBpm;
        ap.sampleRate      = currentSampleRate;   // engine rate
        // Follow the host's ppq timeline only when we actually have one AND it is
        // playing; otherwise (standalone/internal clock, or a stopped host) the
        // arp free-runs on its own beat clock. Passing hostPlaying=true with a
        // frozen ppq (as the internal clock did) made it re-fire beat 0 every
        // block, so every note stuck.
        ap.hostPlaying     = gotHostPpq && blockPlaying;
        ap.ppqAtBlockStart = blockPpq;

        arp.process (engMidi, engN, ap);
    }

    updateSharedState (engN);

    if (factor > 1 && oversampler != nullptr)
    {
        // Render the engine at the oversampled rate, then decimate to host rate.
        juce::dsp::AudioBlock<float> hostBlock (buffer);
        auto osBlock = oversampler->processSamplesUp (hostBlock);   // silent -> upsampled
        const int numCh = buffer.getNumChannels();
        float* chans[2] = { osBlock.getChannelPointer (0),
                            numCh > 1 ? osBlock.getChannelPointer (1)
                                      : osBlock.getChannelPointer (0) };
        juce::AudioBuffer<float> osBuf (chans, numCh, (int) osBlock.getNumSamples());
        osBuf.clear();
        renderEngine (osBuf, engMidi);
        oversampler->processSamplesDown (hostBlock);   // anti-alias + decimate
    }
    else
    {
        renderEngine (buffer, engMidi);
    }

    // Non-finite flush: a NaN/Inf sample landing in a feedback structure (delay
    // ring, reverb) never decays on its own -- it recirculates forever, so
    // silence this block outright and ask the timer to reset the FX chain's
    // state (under the callback lock) rather than let it keep recirculating.
    // Tight scan over the final host-domain buffer only; cheap per block.
    {
        bool hasNonFinite = false;
        for (int ch = 0; ch < buffer.getNumChannels() && ! hasNonFinite; ++ch)
        {
            const auto* data = buffer.getReadPointer (ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                if (! std::isfinite (data[i]))
                {
                    hasNonFinite = true;
                    break;
                }
            }
        }
        if (hasNonFinite)
        {
            buffer.clear();
            fxStateFlushPending.store (true, std::memory_order_relaxed);
        }
    }

    // Block-level telemetry.
    int active = 0;
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (synth.getVoice (i)->isVoiceActive())
            ++active;
    telemetry.activeVoices.store (active, std::memory_order_relaxed);
    telemetry.peakL.store (buffer.getMagnitude (0, 0, buffer.getNumSamples()),
                           std::memory_order_relaxed);
    telemetry.peakR.store (buffer.getNumChannels() > 1
                               ? buffer.getMagnitude (1, 0, buffer.getNumSamples())
                               : telemetry.peakL.load (std::memory_order_relaxed),
                           std::memory_order_relaxed);

    // Feed the EQ spectrum analyzer: push the master output (mono sum) into the
    // scope ring, write index published last so the UI reads a coherent window.
    {
        const int n = buffer.getNumSamples();
        const auto* l = buffer.getReadPointer (0);
        const auto* r = buffer.getNumChannels() > 1 ? buffer.getReadPointer (1) : l;
        int w = telemetry.scopeWrite.load (std::memory_order_relaxed);
        for (int i = 0; i < n; ++i)
        {
            telemetry.scope[(size_t) w].store (0.5f * (l[i] + r[i]),
                                               std::memory_order_relaxed);
            w = (w + 1) & (dsp::Telemetry::scopeSize - 1);
        }
        telemetry.scopeWrite.store (w, std::memory_order_release);
    }

    // Scrolling limiter history: one frame per block. When the limiter is off we
    // still scroll the master level (with zero reduction) so the display lives.
    {
        const bool limOn = fxParams.limEnable;
        const float masterPk = juce::jmax (telemetry.peakL.load (std::memory_order_relaxed),
                                           telemetry.peakR.load (std::memory_order_relaxed));
        const float outLvl = limOn ? fxChain.limiterOutputPeak() : masterPk;
        const float grDb   = limOn ? fxChain.limiterGainReductionDb() : 0.0f;
        const int lw = telemetry.limWrite.load (std::memory_order_relaxed);
        telemetry.limOut[(size_t) lw].store (outLvl, std::memory_order_relaxed);
        telemetry.limGrDb[(size_t) lw].store (grDb, std::memory_order_relaxed);
        telemetry.limWrite.store ((lw + 1) % dsp::Telemetry::limiterHistory,
                                  std::memory_order_release);
    }

    // Headphone-safety ceiling for pathological states only; normal audio
    // (limiter/master stage) never approaches +12 dBFS, so this is inaudible
    // insurance against a runaway/huge-but-finite value reaching the output.
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        juce::FloatVectorOperations::clip (data, data, -4.0f, 4.0f, buffer.getNumSamples());
    }
}

// Soft bypass: strip note-on messages (velocity 0 "note on" counts as a
// note-off, per MidiMessage::isNoteOn's default, so it correctly passes
// through) and otherwise run the identical pipeline processBlock does --
// voices already sounding keep releasing/decaying, the FX chain keeps
// processing their output normally, so reverb/delay/convolve tails ring out
// instead of being hard-cut. Note-offs, CC (including panic's 120/123),
// pitch bend, sustain, aftertouch, MIDI clock, etc. all pass through
// untouched. No separate render path to keep in sync with processBlock.
void SPASynthProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midi)
{
    bypassMidi.clear();
    for (const auto md : midi)
    {
        const auto m = md.getMessage();
        if (! m.isNoteOn())
            bypassMidi.addEvent (m, md.samplePosition);
    }
    processBlock (buffer, bypassMidi);
}

juce::ValueTree SPASynthProcessor::buildStateTree (bool includeMidiMap)
{
    const auto libraryRoot = library::findLibraryRoot();

    // Stamp the CURRENT wildness onto the live tree before copying, so it is
    // always present in the saved state (session or preset) rather than only
    // when the WILD knob happens to have been touched since launch -- the
    // property previously stayed absent from a session's/preset's tree until
    // setRandomWildness() was first called, which is why real preset files
    // never carried it.
    apvts.state.setProperty (wildnessProperty, (double) getRandomWildness(), nullptr);

    auto state = apvts.copyState();

    if (includeMidiMap)
        state.appendChild (midiLearn->toValueTree(), nullptr);

    auto wavetables = state.getOrCreateChildWithName (wavetableStateType, nullptr);
    auto samples = state.getOrCreateChildWithName (sampleStateType, nullptr);
    for (int s = 0; s < params::numOscSlots; ++s)
    {
        const auto& wtPath = slotTables[(size_t) s].path;
        const auto& smpPath = slotSamples[(size_t) s].path;
        wavetables.setProperty (slotPathProperty (s),
                                wtPath.isEmpty() ? juce::String()
                                    : library::toPortable (juce::File (wtPath), libraryRoot),
                                nullptr);
        samples.setProperty (slotPathProperty (s),
                             smpPath.isEmpty() ? juce::String()
                                 : library::toPortable (juce::File (smpPath), libraryRoot),
                             nullptr);
    }

    auto lfoCustom = state.getOrCreateChildWithName (lfoCustomStateType, nullptr);
    for (int i = 0; i < params::numLFOs; ++i)
        lfoCustom.appendChild (customLfoShapeToValueTree (getCustomLfoShape (i), i), nullptr);

    return state;
}

void SPASynthProcessor::restoreStateTree (const juce::ValueTree& incoming, bool isPresetLoad)
{
    if (! incoming.hasType (apvts.state.getType()))
        return;

    // The dirty/edited flag must not flip anywhere in this function or in
    // the deferred content loads it kicks off below -- set BEFORE anything
    // else runs, not just before apvts.replaceState() (the per-LFO
    // setCustomLfoShape() restore a few lines down also marks dirty on its
    // own, as a normal content change, and that loop runs before the
    // ScopedLock block).
    presetLoadGuard.store (true, std::memory_order_relaxed);

    auto state = incoming.createCopy();
    const auto libraryRoot = library::findLibraryRoot();

    // Wavetable/sample paths ride along in the state tree but are not
    // parameters.
    auto wavetables = state.getChildWithName (wavetableStateType);
    if (wavetables.isValid())
        state.removeChild (wavetables, nullptr);
    auto samples = state.getChildWithName (sampleStateType);
    if (samples.isValid())
        state.removeChild (samples, nullptr);

    // Custom LFO breakpoint shapes (1.0.25): a per-LFO child tree, not
    // parameters, same reason as wavetable/sample paths above. Per the
    // design, this DOES apply on preset load (unlike uiScale etc. below) --
    // a preset's drawn shape is part of the sound, not a window/UI pref. A
    // session/preset missing the data (predates this feature, or a given
    // LFO simply has no child) gets the default triangle --
    // customLfoShapeFromValueTree()/its default-constructed fallback cover
    // both an absent LFOCUSTOM tree entirely and an absent single LFO child.
    auto lfoCustom = state.getChildWithName (lfoCustomStateType);
    if (lfoCustom.isValid())
        state.removeChild (lfoCustom, nullptr);
    for (int i = 0; i < params::numLFOs; ++i)
    {
        const auto child = lfoCustom.isValid() ? lfoCustom.getChildWithName (lfoCustomChildType (i))
                                                : juce::ValueTree();
        setCustomLfoShape (i, customLfoShapeFromValueTree (child), false);
    }

    // MIDI map: restore when present (host sessions); presets omit it and
    // leave the current hardware mapping untouched.
    auto midiMap = state.getChildWithName (MidiLearnManager::mapTreeType);
    if (midiMap.isValid())
    {
        state.removeChild (midiMap, nullptr);
        midiLearn->restoreFromValueTree (midiMap);
    }

    // A parameter missing from `incoming` (an older preset saved before that
    // parameter existed, or any other incomplete state) must come up at its
    // registry default. Traced against JUCE's own ParameterAdapter: on
    // replaceState(), a param with no matching child in the new tree gets a
    // fresh PARAM node appended via state.appendChild(), which synchronously
    // fires APVTS's own valueTreeChildAdded -> setNewState() listener on
    // that still-empty node BEFORE the node has a "value" property --
    // setNewState() then reads getProperty("value", getDenormalisedDefault-
    // Value()), falling back to the REGISTRY DEFAULT. So JUCE already
    // resets a missing param to its default on its own (confirmed with an
    // anti-vacuous test: disabling the fill below still passed
    // presetMissingParamsDefaultOnLoadTest). This fill is kept anyway as an
    // explicit, order-independent guarantee -- it makes the "missing param
    // -> registry default" contract visible at the call site instead of
    // resting on an internal JUCE listener-ordering detail, and applies
    // uniformly to presets, reset-to-default and host session restores.
    fillMissingParamsWithDefaults (state);

    // Machine/window UI properties (uiScale, uiKeyboardOctave,
    // uiKeyboardVisible) must NOT change when a preset loads -- a preset is
    // a sound, not a window layout -- but DO apply on a real host session
    // restore. Capture whatever the live tree currently holds so they can be
    // restamped after replaceState() below; `has*`/`saved*` distinguish "was
    // never set" from "was set to some value" so a preset load never
    // fabricates a property a fresh instance never had.
    const bool hadUiScale = isPresetLoad && apvts.state.hasProperty (uiScaleProperty);
    const auto savedUiScale = hadUiScale ? apvts.state.getProperty (uiScaleProperty) : juce::var();
    const bool hadUiKeyboardOctave = isPresetLoad && apvts.state.hasProperty (uiKeyboardOctaveProperty);
    const auto savedUiKeyboardOctave = hadUiKeyboardOctave ? apvts.state.getProperty (uiKeyboardOctaveProperty) : juce::var();
    const bool hadUiKeyboardVisible = isPresetLoad && apvts.state.hasProperty (uiKeyboardVisibleProperty);
    const auto savedUiKeyboardVisible = hadUiKeyboardVisible ? apvts.state.getProperty (uiKeyboardVisibleProperty) : juce::var();

    juce::String convIrPathLocal;
    {
        // AudioProcessor::setStateInformation (the AU wrapper's entry point) is
        // called with NO lock, while processBlock runs under getCallbackLock().
        // apvts.replaceState() updates parameters ONE AT A TIME, so without this
        // lock processBlock can render blocks against a half-old/half-new
        // parameter set mid-restore -- unstable coefficient combos that inject
        // audible energy bursts into feedback DSP (filters/delay/reverb) when a
        // Logic session restores. Mirrors the getCallbackLock() idiom already
        // used around rebuildOversampling() in timerCallback(). Filesystem I/O
        // (findLibraryRoot) stays outside so the audio thread is never blocked
        // on disk.
        const juce::ScopedLock sl (getCallbackLock());

        apvts.replaceState (state);

        // Restamp the machine/window UI properties preset loads must leave
        // untouched, restoring exactly the pre-load state (present-with-value,
        // or absent) rather than whatever the incoming preset carried.
        if (isPresetLoad)
        {
            if (hadUiScale)
                apvts.state.setProperty (uiScaleProperty, savedUiScale, nullptr);
            else
                apvts.state.removeProperty (uiScaleProperty, nullptr);

            if (hadUiKeyboardOctave)
                apvts.state.setProperty (uiKeyboardOctaveProperty, savedUiKeyboardOctave, nullptr);
            else
                apvts.state.removeProperty (uiKeyboardOctaveProperty, nullptr);

            if (hadUiKeyboardVisible)
                apvts.state.setProperty (uiKeyboardVisibleProperty, savedUiKeyboardVisible, nullptr);
            else
                apvts.state.removeProperty (uiKeyboardVisibleProperty, nullptr);
        }

        // Tester-reported burst on preset clicks: replaceState() above just
        // swapped every coefficient under live, non-zero voice/FX state (FDN
        // reverb was the worst offender). A preset load is a full state
        // replacement, so reuse panic()'s hard reset here, synchronously
        // under the lock, so the first block after this sees new params
        // applied to already-silent state.
        synth.allNotesOff (0, false);
        arp.reset();
        fxChain.reset();

        // Standalone tempo settings ride in the state tree (not parameters).
        internalBpm.store ((double) apvts.state.getProperty ("standaloneBpm", 120.0),
                           std::memory_order_relaxed);
        tempoSyncMode.store ((int) apvts.state.getProperty ("tempoSyncMode", 0),
                             std::memory_order_relaxed);
        fxOrderPacked.store ((juce::uint64) (juce::int64) apvts.state.getProperty (
                                 "fxOrder", (juce::int64) dsp::FXChain::defaultOrderPacked()),
                             std::memory_order_relaxed);

        const auto convIR = apvts.state.getProperty ("convIR").toString();
        convIrPath = convIR.isEmpty() ? juce::String()
                                      : library::fromPortable (convIR, libraryRoot).getFullPathName();
        convIrPathLocal = convIrPath;

        // Lands clean and lifts the guard: a preset load, Reset to Default
        // and a host session restore all start the edited/dirty indicator
        // fresh (a just-restored session is not "edited" relative to
        // itself). Safe to do here, synchronously, even though the convIR/
        // per-slot content loads below still run via deferred callAsync --
        // each of THOSE calls its own loadConvolutionIR/loadWavetableFromFile/
        // loadSampleFromFile/setCustomLfoShape with markEdited=false (see
        // their calls below and the per-LFO restore loop above), so they
        // never mark dirty in the first place; this no longer depends on
        // guarding a window until they happen to run, which is not
        // deterministically timed against a plain guard/counter (measured:
        // callAsync delivery in this process can lag by seconds under load).
        presetLoadGuard.store (false, std::memory_order_relaxed);
        presetDirty.store (false, std::memory_order_relaxed);
    }
    dirtyNotifier.triggerAsyncUpdate();

    // Weak-ref treatment mirrors loadSampleFromFile — see the comment there.
    juce::MessageManager::callAsync ([weak = juce::WeakReference<SPASynthProcessor> (this),
                                      f = juce::File (convIrPathLocal)]
    {
        if (weak != nullptr)
            weak->fxChain.loadConvolutionIR (f);
    });

    for (int s = 0; s < params::numOscSlots; ++s)
    {
        const auto wtPath = wavetables.isValid()
                          ? wavetables.getProperty (slotPathProperty (s)).toString()
                          : juce::String();
        const auto smpPath = samples.isValid()
                           ? samples.getProperty (slotPathProperty (s)).toString()
                           : juce::String();

        // Weak-ref treatment mirrors loadSampleFromFile — see the comment there.
        // (setStateInformation can hand the host a destroyed processor before
        // this deferred callAsync runs, e.g. during a fast preset/plugin
        // teardown sequence.)
        juce::MessageManager::callAsync ([weak = juce::WeakReference<SPASynthProcessor> (this),
                                          s, wtPath, smpPath, libraryRoot]
        {
            if (weak == nullptr)
                return;

            // apvts.replaceState() already ran synchronously above, so the
            // osc::table choice param is already restored by the time this
            // deferred callback runs -- rebuild from it rather than always
            // forcing Basic Shapes. A loaded file still wins over the choice.
            // markEdited=false on both loads: this is the preset/session's
            // OWN saved content loading back in, not a new edit.
            if (wtPath.isEmpty())
                weak->applyBuiltInWavetableFromParam (s);
            else
                weak->loadWavetableFromFile (s, library::fromPortable (wtPath, libraryRoot), false);

            if (smpPath.isNotEmpty())
                weak->loadSampleFromFile (s, library::fromPortable (smpPath, libraryRoot), false);
        });
    }
}

bool SPASynthProcessor::refreshLibrary()
{
    const auto root = library::findLibraryRoot();
    if (! root.isDirectory())
    {
        lastLibraryPackCount = 0;
        // Resets the watcher too: an absent root reads back as the same
        // "<missing>" fingerprint every tick, so this doesn't cause a
        // refresh loop, and a manual Rescan while the root is gone still
        // clears any stale pending state from before it disappeared.
        lastScannedLibraryFingerprint = computeLibraryFingerprint();
        pendingLibraryFingerprint = lastScannedLibraryFingerprint;
        libraryFingerprintStableTicks = 0;
        return false;
    }

    const auto packs = library::scanLibrary (root);
    lastLibraryPackCount = (int) packs.size();
    presetManager->generateFactoryPresets (packs, root);
    presetManager->rescan();

    // Record what we just scanned so the watcher doesn't immediately think
    // its own refresh was an unrelated change (and so a manual Rescan click
    // resets the debounce state exactly like a watcher-triggered one).
    lastScannedLibraryFingerprint = computeLibraryFingerprint();
    pendingLibraryFingerprint = lastScannedLibraryFingerprint;
    libraryFingerprintStableTicks = 0;
    return true;
}

void SPASynthProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = buildStateTree().createXml())
        copyXmlToBinary (*xml, destData);
}

void SPASynthProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        restoreStateTree (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* SPASynthProcessor::createEditor()
{
    return new SPASynthEditor (*this);
}

} // namespace spa

// This creates new instances of the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new spa::SPASynthProcessor();
}
