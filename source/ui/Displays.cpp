#include "Displays.h"
#include "../SPASynthProcessor.h"
#include "../dsp/FXChain.h"
#include "../dsp/ParametricEQ.h"
#include "../dsp/SamplePlayer.h"

namespace spa::ui
{

namespace
{
    // Test-only instrumentation, not declared in Displays.h (out of scope
    // for the loopXfade/chaos-division subscription fixes) -- forward-
    // declared instead directly in SPASynthTests.cpp. paintDisplay() always
    // computes from LIVE parameter values, so a force-painted image looks
    // correct regardless of whether the automatic dirty-flag + 24Hz-timer
    // repaint (DisplayComponent::timerCallback) actually fires -- these
    // counters are the only way to observe THAT path (an unsubscribed
    // parameter change leaves it silent) without editing the header.
    std::atomic<int> waveDisplayPaintCounter { 0 };
    std::atomic<int> chaosDisplayPaintCounter { 0 };
    std::atomic<int> lfoDisplayPaintCounter { 0 };
}

// Prototypes live here (not in Displays.h, out of scope for this fix) --
// SPASynthTests.cpp forward-declares these same signatures itself to
// call them; this declaration just keeps this TU's own -Wmissing-prototypes
// happy.
int waveDisplayPaintCountForTest();
int chaosDisplayPaintCountForTest();
int lfoDisplayPaintCountForTest();

int waveDisplayPaintCountForTest() { return waveDisplayPaintCounter.load (std::memory_order_relaxed); }
int chaosDisplayPaintCountForTest() { return chaosDisplayPaintCounter.load (std::memory_order_relaxed); }
int lfoDisplayPaintCountForTest() { return lfoDisplayPaintCounter.load (std::memory_order_relaxed); }

// ========================== DisplayComponent ===============================

DisplayComponent::DisplayComponent (juce::AudioProcessorValueTreeState& state,
                                    juce::StringArray paramIDs,
                                    const dsp::Telemetry* tel)
    : apvts (state), telemetry (tel), watched (std::move (paramIDs))
{
    setInterceptsMouseClicks (false, false);
    for (const auto& id : watched)
        apvts.addParameterListener (id, this);
    startTimerHz (24);
}

DisplayComponent::~DisplayComponent()
{
    for (const auto& id : watched)
        apvts.removeParameterListener (id, this);
}

bool DisplayComponent::isLive() const
{
    return telemetry != nullptr
        && telemetry->activeVoices.load (std::memory_order_relaxed) > 0;
}

float DisplayComponent::value (const juce::String& paramID) const
{
    auto* param = apvts.getParameter (paramID);
    return param != nullptr ? param->convertFrom0to1 (param->getValue()) : 0.0f;
}

void DisplayComponent::timerCallback()
{
    if (dirty.exchange (false) || isLive())
        repaint();
}

void DisplayComponent::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    draw::displayWell (g, bounds);
    paintDisplay (g, bounds.reduced (3.0f));
}

// A background load is in flight for this slot: sweeping accent bar +
// caption. Callers markDirty() after painting this so the 24 Hz display
// timer keeps the sweep moving until the load lands.
static void paintLoadingOverlay (juce::Graphics& g, juce::Rectangle<float> area)
{
    const auto& t = currentTheme();

    const auto phase = (float) (juce::Time::getMillisecondCounter() % 1200u) / 1200.0f;
    const auto barWidth = area.getWidth() * 0.18f;
    const auto x = area.getX() - barWidth + phase * (area.getWidth() + barWidth);

    g.setGradientFill (juce::ColourGradient (
        t.accent.withAlpha (0.0f),  x, 0.0f,
        t.accent.withAlpha (0.35f), x + barWidth, 0.0f, false));
    g.fillRect (juce::Rectangle<float> (x, area.getY(), barWidth, area.getHeight())
                    .getIntersection (area));

    g.setColour (t.textSecondary);
    g.setFont (metrics::labelFont());
    g.drawText ("loading...", area.toNearestInt(), juce::Justification::centred);
}

// ============================ WaveDisplay ==================================

WaveDisplay::WaveDisplay (SPASynthProcessor& p, int slotIndex)
    : DisplayComponent (p.getAPVTS(),
                        { params::id::oscSlot (slotIndex, params::id::osc::mode),
                          params::id::oscSlot (slotIndex, params::id::osc::position),
                          params::id::oscSlot (slotIndex, params::id::osc::grainPos),
                          params::id::oscSlot (slotIndex, params::id::osc::sampleStart),
                          params::id::oscSlot (slotIndex, params::id::osc::loop),
                          params::id::oscSlot (slotIndex, params::id::osc::loopStart),
                          params::id::oscSlot (slotIndex, params::id::osc::loopEnd),
                          params::id::oscSlot (slotIndex, params::id::osc::loopXfade),
                          params::id::oscSlot (slotIndex, params::id::osc::syncToBpm),
                          params::id::oscSlot (slotIndex, params::id::osc::syncBeatsOverride),
                          params::id::oscSlot (slotIndex, params::id::osc::timeSig),
                          params::id::oscSlot (slotIndex, params::id::osc::analogShape),
                          params::id::oscSlot (slotIndex, params::id::osc::pulseWidth),
                          params::id::oscSlot (slotIndex, params::id::osc::fmRatio),
                          params::id::oscSlot (slotIndex, params::id::osc::fmIndex),
                          params::id::oscSlot (slotIndex, params::id::osc::noiseColor),
                          params::id::oscSlot (slotIndex, params::id::osc::pluckDamp) },
                        &p.getTelemetry()),
      processor (p), slot (slotIndex)
{
    processor.addChangeListener (this);
    // Interactive for zoom/pan gestures (sample/granular waveform), but
    // never steals QWERTY focus -- presetBrowserFocusGrabTest sweeps the
    // whole editor and fails on any component left grabbing click focus.
    setInterceptsMouseClicks (true, false);
    setMouseClickGrabsKeyboardFocus (false);
    setTooltip ("scroll: zoom, drag: pan, double-click: reset");
}

WaveDisplay::~WaveDisplay()
{
    processor.removeChangeListener (this);
}

void WaveDisplay::changeListenerCallback (juce::ChangeBroadcaster*)
{
    markDirty();
}

juce::Rectangle<float> WaveDisplay::waveArea() const
{
    // Must match DisplayComponent::paint()'s reduced bounds exactly -- that's
    // the rectangle paintDisplay() actually draws into.
    return getLocalBounds().toFloat().reduced (3.0f);
}

float WaveDisplay::xToNorm (float x, juce::Rectangle<float> area) const
{
    if (area.getWidth() <= 0.0f)
        return viewStart;
    const auto frac = (x - area.getX()) / area.getWidth();
    return juce::jlimit (0.0f, 1.0f, viewStart + frac * viewLength);
}

float WaveDisplay::normToX (float norm, juce::Rectangle<float> area) const
{
    if (viewLength <= 0.0f)
        return area.getX();
    return area.getX() + ((norm - viewStart) / viewLength) * area.getWidth();
}

// Visual gains for the chaos drift cues (see WaveDisplay::ChaosViz). The
// audio-path drift at the registry DEFAULTS (depth 0.4 x mix 1.0, phase
// amount 0.15 cycles, pitch amount 8 ct) is 0.06 cycles / 0.032 semitones
// at a walker's full swing -- a 0.06-cycle slide is ~6% of the display
// width and 0.032 semitones is invisible, which is why the shape hardly
// moved before. Phase: 2x, so a default full-swing slide is 12% of the
// width (the slide wraps, so at maximum the shape simply scrolls through
// a whole cycle, never off the display). Pitch: 2^tanh(4 x semitones),
// ~+-9% stretch at a default full swing, saturating at exactly 2x / 0.5x
// (one octave, i.e. double / half the visible cycles) at the maximum
// 1-semitone drift so it can never become cartoonish.
constexpr float chaosVizPhaseGain = 2.0f;
constexpr float chaosVizPitchGain = 4.0f;

// A drift cue may only be drawn where the drift it stands for actually
// reaches that engine's AUDIO and where the drawn thing really represents
// that quantity. In SPASynthVoice::computeChunk the chaos PHASE drift is
// passed only into WavetableOscillator (updateBlock's phaseOffset); no
// other engine ever sees it. The chaos PITCH drift is added to pitchOffset
// for every engine. And the sample/granular display draws the audio FILE,
// which does not move when anything drifts. Hence, per mode:
//   wavetable          slide + stretch (live wave shape; both drifts real)
//   analog / FM / pluck stretch only (ideal one-cycle preview: a horizontal
//                      stretch = more/fewer cycles = a pitch change; no
//                      phase drift reaches these engines)
//   noise              nothing (its shape is per-step random, so any cue
//                      would be meaningless)
//   sample / granular  nothing (the drawn waveform is the file; position
//                      drift is already shown honestly by the granular
//                      grain-cloud markers and the sample playhead)
WaveDisplay::ChaosViz WaveDisplay::chaosViz (juce::Rectangle<float> area) const
{
    ChaosViz v;
    if (! isLive())
        return v;

    const auto mode = (params::OscMode) (int) value (
        params::id::oscSlot (slot, params::id::osc::mode));
    const bool drawsPhase = mode == params::OscMode::wavetable;
    const bool drawsPitch = mode == params::OscMode::wavetable
                         || mode == params::OscMode::analog
                         || mode == params::OscMode::fm
                         || mode == params::OscMode::pluck;
    if (! drawsPhase && ! drawsPitch)
        return v;

    const auto pitchSemis = drawsPitch
        ? telemetry->slotChaosPitch[(size_t) slot].load (std::memory_order_relaxed) : 0.0f;
    const auto phaseCycles = drawsPhase
        ? telemetry->slotChaosPhase[(size_t) slot].load (std::memory_order_relaxed) : 0.0f;
    if (pitchSemis == 0.0f && phaseCycles == 0.0f)
        return v;

    // Wrap the gained phase to [-0.5, 0.5) cycles so the slide is the
    // shortest way round, then scale to pixels.
    auto cycles = phaseCycles * chaosVizPhaseGain;
    cycles -= std::floor (cycles + 0.5f);
    v.slidePx = cycles * area.getWidth();

    v.stretch = std::exp2 (std::tanh (pitchSemis * chaosVizPitchGain)) - 1.0f;
    return v;
}

std::vector<WaveDisplay::XfadeRamp> WaveDisplay::getXfadeRamps() const
{
    std::vector<XfadeRamp> out;
    namespace id = params::id;

    const auto mode = (params::OscMode) (int) value (id::oscSlot (slot, id::osc::mode));
    if (mode != params::OscMode::sample)
        return out;

    if (value (id::oscSlot (slot, id::osc::loop)) < 0.5f)
        return out;

    const auto sample = processor.getSample (slot);
    if (sample == nullptr)
        return out;

    const auto lenSamples = (double) sample->lengthSamples();
    if (lenSamples < 2.0)
        return out;
    const auto lastSampleSrc = juce::jmax (0.0, lenSamples - 1.0);

    dsp::SamplePlayer::Params xfP;
    xfP.sample = sample.get();
    xfP.loop = true;
    xfP.loopStartNorm = value (id::oscSlot (slot, id::osc::loopStart));
    xfP.loopEndNorm   = value (id::oscSlot (slot, id::osc::loopEnd));
    xfP.loopXfade     = value (id::oscSlot (slot, id::osc::loopXfade)) * 0.01f;

    // SYNC (beat-grid snap): same effective bounds the engine actually
    // loops on, whether or not SYNC is on -- the crossfade now applies in
    // both sync states (the stretcher applies it per-grain too), so the
    // ramps must sit on the snapped seam under SYNC to match what's heard.
    if (value (id::oscSlot (slot, id::osc::syncToBpm)) >= 0.5f)
    {
        const auto beatsOverride = value (id::oscSlot (slot, id::osc::syncBeatsOverride));
        const auto lengthSeconds = sample->lengthSeconds();
        const auto nativeBpm = beatsOverride > 0.0f && lengthSeconds > 1.0e-6
                              ? 60.0 * beatsOverride / lengthSeconds
                              : sample->detectedBpm;
        if (nativeBpm > 1.0 && lengthSeconds > 1.0e-6)
        {
            xfP.snapToGrid = true;
            xfP.gridBeatSeconds = 60.0 / nativeBpm;
            xfP.gridOffsetSeconds = sample->firstOnsetSeconds;
        }
    }

    double xfLoopStart = 0.0, xfLoopEnd = 0.0;
    dsp::SamplePlayer::effectiveLoopBoundsSamples (xfP, lenSamples, xfLoopStart, xfLoopEnd);

    bool usePreRoll = true;
    const auto X = dsp::SamplePlayer::effectiveXfadeSamples (xfP, lastSampleSrc, xfLoopStart, xfLoopEnd, usePreRoll);
    if (X <= 0.0)
        return out;

    const auto toNorm = [lenSamples] (double s) { return (float) (s / lenSamples); };

    if (usePreRoll)
    {
        out.push_back ({ toNorm (xfLoopEnd - X), toNorm (xfLoopEnd), true, false });       // fade-out, inside the band
        out.push_back ({ toNorm (xfLoopStart - X), toNorm (xfLoopStart), false, true });   // fade-in, outside (borrowed run-up)
    }
    else
    {
        out.push_back ({ toNorm (xfLoopStart), toNorm (xfLoopStart + X), true, true });    // fade-in, inside the band
        out.push_back ({ toNorm (xfLoopEnd), toNorm (xfLoopEnd + X), false, false });      // fade-out, outside (borrowed tail)
    }

    return out;
}

void WaveDisplay::zoomAt (float normCursor, float factor)
{
    const auto newLen = juce::jlimit (minViewLength, 1.0f, viewLength / factor);
    const auto frac = viewLength > 0.0f ? (normCursor - viewStart) / viewLength : 0.5f;
    viewStart = juce::jlimit (0.0f, 1.0f - newLen, normCursor - frac * newLen);
    viewLength = newLen;
    markDirty();
}

void WaveDisplay::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const auto area = waveArea();
    if (area.getWidth() <= 0.0f)
        return;

    if (std::abs (wheel.deltaX) > std::abs (wheel.deltaY))
    {
        // Horizontal wheel / shift-wheel (the OS maps shift+vertical to a
        // horizontal delta already): pan, scaled by the current zoom so a
        // full-swipe always covers the same fraction of the visible window.
        viewStart = juce::jlimit (0.0f, 1.0f - viewLength,
                                  viewStart - wheel.deltaX * viewLength);
        markDirty();
    }
    else if (wheel.deltaY != 0.0f)
    {
        const auto normCursor = xToNorm ((float) e.position.x, area);
        const auto factor = std::pow (2.0f, wheel.deltaY * 4.0f);   // up = zoom in
        zoomAt (normCursor, factor);
    }
}

void WaveDisplay::mouseMagnify (const juce::MouseEvent& e, float scaleFactor)
{
    const auto area = waveArea();
    if (area.getWidth() <= 0.0f)
        return;
    zoomAt (xToNorm ((float) e.position.x, area), scaleFactor);
}

void WaveDisplay::mouseDown (const juce::MouseEvent& e)
{
    dragStartViewStart = viewStart;
    dragAnchorX = e.position.x;
}

void WaveDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (viewLength >= 1.0f)
        return;   // not zoomed -- keep today's behaviour (no-op)

    const auto area = waveArea();
    if (area.getWidth() <= 0.0f)
        return;

    const auto dxNorm = (e.position.x - dragAnchorX) / area.getWidth() * viewLength;
    viewStart = juce::jlimit (0.0f, 1.0f - viewLength, dragStartViewStart - dxNorm);
    markDirty();
}

void WaveDisplay::mouseDoubleClick (const juce::MouseEvent&)
{
    viewStart = 0.0f;
    viewLength = 1.0f;
    markDirty();
}

void WaveDisplay::paintDisplay (juce::Graphics& g, juce::Rectangle<float> area)
{
    waveDisplayPaintCounter.fetch_add (1, std::memory_order_relaxed);
    const auto& t = currentTheme();
    const auto mode = (params::OscMode) (int) value (
        params::id::oscSlot (slot, params::id::osc::mode));

    // Organic Chaos pitch / phase drift moves the drawn SHAPE (see
    // WaveDisplay::ChaosViz): a drawn horizontal fraction `ph` (0..1 across
    // the area) reads its source material at `chaosSrc (ph)` -- stretched
    // about the centre by the pitch cue, slid by the phase cue, wrapped.
    // chaosViz() decides PER MODE which of the two cues is honest here (see
    // its comment); this lambda just applies whatever survived, so the
    // wavetable branch gets both, the ideal-cycle preview only the pitch
    // stretch, and noise / sample / granular nothing at all. With no drift
    // it returns `ph` untouched (not even a 0.5 +/- round trip), so the
    // undrifted drawing stays bit-identical to the pre-chaos-viz one.
    const auto viz = chaosViz (area);
    const auto slideNorm = area.getWidth() > 0.0f ? viz.slidePx / area.getWidth() : 0.0f;
    const auto chaosSrc = [&] (float ph)
    {
        if (viz.slidePx == 0.0f && viz.stretch == 0.0f)
            return ph;
        auto src = 0.5f + (ph - 0.5f) * (1.0f + viz.stretch) - slideNorm;
        src -= std::floor (src);
        return src;
    };

    if (mode == params::OscMode::wavetable)
    {
        if (processor.isWavetableLoading (slot))
        {
            paintLoadingOverlay (g, area);
            markDirty();   // keep the 24 Hz timer animating the sweep
            return;
        }

        const auto table = processor.getWavetable (slot);
        if (table == nullptr || table->getNumFrames() == 0)
            return;

        // Live (modulated) position while playing, knob position otherwise.
        const auto position = isLive()
            ? telemetry->slotPosition[(size_t) slot].load (std::memory_order_relaxed)
            : value (params::id::oscSlot (slot, params::id::osc::position));

        const auto framePos = juce::jlimit (0.0f, 1.0f, position)
                            * (float) (table->getNumFrames() - 1);
        const auto frameA = juce::jmin ((int) framePos, table->getNumFrames() - 1);
        const auto frameB = juce::jmin (frameA + 1, table->getNumFrames() - 1);
        const auto frac = framePos - (float) frameA;
        const auto* a = table->getFrame (0, frameA);
        const auto* b = table->getFrame (0, frameB);

        juce::Path wave;
        constexpr int steps = 128;
        for (int i = 0; i <= steps; ++i)
        {
            const auto idx = (int) (chaosSrc ((float) i / steps) * (dsp::Wavetable::tableSize - 1));
            const auto sample = a[idx] + frac * (b[idx] - a[idx]);
            const auto x = area.getX() + area.getWidth() * (float) i / steps;
            const auto y = area.getCentreY() - sample * area.getHeight() * 0.42f;
            if (i == 0)
                wave.startNewSubPath (x, y);
            else
                wave.lineTo (x, y);
        }

        draw::glowStroke (g, wave, t.accent);
        return;
    }

    // Synthesis engines: draw an ideal-cycle preview.
    if (mode == params::OscMode::analog || mode == params::OscMode::fm
        || mode == params::OscMode::noise || mode == params::OscMode::pluck)
    {
        namespace osc = params::id::osc;
        juce::Random previewRng (17 + slot);
        float brown = 0.0f;

        juce::Path wave;
        constexpr int steps = 160;
        for (int i = 0; i <= steps; ++i)
        {
            const auto drawPh = (float) i / steps;
            const auto ph = chaosSrc (drawPh);
            float v = 0.0f;

            if (mode == params::OscMode::analog)
            {
                const auto shape = (int) value (params::id::oscSlot (slot, osc::analogShape));
                const auto pw = value (params::id::oscSlot (slot, osc::pulseWidth));
                switch (shape)
                {
                    case 0:  v = 2.0f * ph - 1.0f; break;
                    case 1:  v = ph < 0.5f ? 1.0f : -1.0f; break;
                    case 2:  v = ph < pw ? 1.0f : -1.0f; break;
                    case 3:  v = 1.0f - 4.0f * std::abs (ph - 0.5f); break;
                    default: v = std::sin (juce::MathConstants<float>::twoPi * ph); break;
                }
            }
            else if (mode == params::OscMode::fm)
            {
                const auto ratio = value (params::id::oscSlot (slot, osc::fmRatio));
                const auto index = value (params::id::oscSlot (slot, osc::fmIndex));
                v = std::sin (juce::MathConstants<float>::twoPi * ph
                              + index * std::sin (juce::MathConstants<float>::twoPi * ph * ratio));
            }
            else if (mode == params::OscMode::noise)
            {
                const auto colour = (int) value (params::id::oscSlot (slot, osc::noiseColor));
                const auto white = previewRng.nextFloat() * 2.0f - 1.0f;
                if (colour == 2)
                {
                    brown = juce::jlimit (-1.0f, 1.0f, brown * 0.9f + white * 0.35f);
                    v = brown;
                }
                else if (colour == 1)
                {
                    brown = brown * 0.5f + white * 0.5f;
                    v = brown;
                }
                else
                    v = white;
            }
            else   // pluck: decaying burst
            {
                const auto damp = value (params::id::oscSlot (slot, osc::pluckDamp));
                const auto white = previewRng.nextFloat() * 2.0f - 1.0f;
                v = white * std::exp (-(3.5f - 2.5f * damp) * ph);
            }

            const auto x = area.getX() + area.getWidth() * drawPh;
            const auto y = area.getCentreY() - v * area.getHeight() * 0.42f;
            if (i == 0)
                wave.startNewSubPath (x, y);
            else
                wave.lineTo (x, y);
        }

        draw::glowStroke (g, wave, t.accent);
        return;
    }

    // Sample / granular: waveform overview.
    const auto sample = processor.getSample (slot);
    const bool loading = processor.isSampleLoading (slot);
    if (sample == nullptr || sample->lengthSamples() < 2)
    {
        lastSample = nullptr;
        if (loading)
        {
            paintLoadingOverlay (g, area);
            markDirty();   // keep the 24 Hz timer animating the sweep
            return;
        }

        g.setColour (t.textSecondary.withAlpha (0.6f));
        g.setFont (metrics::labelFont());
        g.drawText ("no SFX loaded", area.toNearestInt(), juce::Justification::centred);
        return;
    }

    // A new load resets the zoom/pan (mode switches -- e.g. sample <->
    // granular on the same file -- keep the existing view, since it stays
    // meaningful; see the class comment on getViewStart()/getViewLength()).
    if (sample.get() != lastSample)
    {
        lastSample = sample.get();
        viewStart = 0.0f;
        viewLength = 1.0f;
    }

    const auto* audio = sample->audio.getReadPointer (0);
    const juce::int64 numSamples = sample->lengthSamples();
    const auto columns = juce::jmax (32, (int) area.getWidth() / 2);

    // Peak-per-column over the VISIBLE window only -- zooming in shrinks the
    // per-column sample span, which is what makes zoomed painting read at
    // higher resolution with no extra cache: the stride-capped read below
    // (<= 64 samples/column, bounded regardless of file length) already
    // satisfies the RT-safety-adjacent "no unbounded work per paint" rule
    // whether the file is 4 samples or 5 minutes long.
    const auto viewStartSample = (juce::int64) ((double) viewStart * (double) numSamples);
    const auto viewSampleCount = juce::jlimit (
        (juce::int64) 1, numSamples - viewStartSample,
        (juce::int64) ((double) viewLength * (double) numSamples));

    juce::Path fill;
    fill.startNewSubPath (area.getX(), area.getCentreY());
    const auto columnPeak = [&] (int c)
    {
        // No chaos drift cue here, deliberately: this draws the audio FILE,
        // whose waveform does not move when chaos drifts. Phase drift never
        // reaches the sample or granular engines at all, and position drift
        // (granular only) is already shown where it is true -- the grain
        // cloud markers and the sample playhead. chaosViz() returns zero in
        // these modes, so `chaosSrc` is identity; this column index is the
        // raw one regardless.
        const auto start = viewStartSample + (juce::int64) c * viewSampleCount / columns;
        const auto end = juce::jmin (numSamples,
                                     viewStartSample + (juce::int64) (c + 1) * viewSampleCount / columns);
        float peak = 0.0f;
        const auto span = juce::jmax ((juce::int64) 1, end - start);
        const auto stride = juce::jmax ((juce::int64) 1, span / 64);
        for (juce::int64 i = start; i < end; i += stride)
            peak = juce::jmax (peak, std::abs (audio[i]));
        return peak;
    };

    // Everything below draws only within `area`: off-view geometry (a loop
    // marker that scrolled out of the zoomed window, etc.) must be clipped,
    // not drawn wrong -- see class comment.
    g.saveState();
    g.reduceClipRegion (area.toNearestInt());

    for (int c = 0; c < columns; ++c)
        fill.lineTo (area.getX() + area.getWidth() * (float) c / (float) (columns - 1),
                     area.getCentreY() - columnPeak (c) * area.getHeight() * 0.46f);
    for (int c = columns - 1; c >= 0; --c)
        fill.lineTo (area.getX() + area.getWidth() * (float) c / (float) (columns - 1),
                     area.getCentreY() + columnPeak (c) * area.getHeight() * 0.46f);
    fill.closeSubPath();

    g.setColour (t.accent.withAlpha (0.55f));
    g.fillPath (fill);

    // Same normalized<->x mapping the gesture handlers use (normToX), so
    // every overlay below stays pixel-exact with the zoom/pan the mouse
    // just applied.
    const auto markerX = [&] (float norm)
    {
        return normToX (norm, area);
    };

    // Sample mode: shade the loop region (while LOOP is on) and mark its
    // edges, so a long SFX's loop points read directly on the waveform
    // (tester request from Paul). sampleStart/loopStart/loopEnd are all
    // normalized 0..1 fractions of the file -- SamplePlayer::getNextSample
    // multiplies loopStartNorm/loopEndNorm by lengthSamples() the same way
    // noteOn() scales startNorm (source/dsp/SamplePlayer.h) -- the same
    // domain markerX already uses for the waveform columns, so no extra
    // mapping is needed. Granular has its own grain-viz below and is left
    // untouched. Drawn under the tick/playhead lines below so those stay
    // crisp on top.
    const auto loopOn = mode == params::OscMode::sample
                      && value (params::id::oscSlot (slot, params::id::osc::loop)) >= 0.5f;
    // SYNC only exists (and only applies) while LOOP is on (see OscStrip) --
    // gate the beat grid the same way.
    const auto syncOn = loopOn
                      && value (params::id::oscSlot (slot, params::id::osc::syncToBpm)) >= 0.5f;

    double nativeBpm = 0.0, gridOffsetSeconds = 0.0, lengthSeconds = 0.0;
    std::shared_ptr<const dsp::SampleData> syncSample;
    if (mode == params::OscMode::sample && (loopOn || syncOn))
    {
        syncSample = processor.getSample (slot);
        if (syncSample != nullptr)
        {
            const auto beatsOverride = value (params::id::oscSlot (slot, params::id::osc::syncBeatsOverride));
            lengthSeconds = syncSample->lengthSeconds();
            nativeBpm = beatsOverride > 0.0f && lengthSeconds > 1.0e-6
                      ? 60.0 * beatsOverride / lengthSeconds
                      : syncSample->detectedBpm;
            gridOffsetSeconds = syncSample->firstOnsetSeconds;
        }
    }

    if (loopOn)
    {
        // With SYNC on, show the EFFECTIVE (snapped) loop -- the whole-beat,
        // grid-anchored span the engine actually plays -- not the raw knob
        // values, so the display always matches what's heard.
        auto loopStartNorm = value (params::id::oscSlot (slot, params::id::osc::loopStart));
        auto loopEndNorm = value (params::id::oscSlot (slot, params::id::osc::loopEnd));
        if (syncOn && syncSample != nullptr && nativeBpm > 1.0 && lengthSeconds > 1.0e-6)
        {
            dsp::SamplePlayer::Params snapP;
            snapP.sample = syncSample.get();
            snapP.loopStartNorm = loopStartNorm;
            snapP.loopEndNorm = loopEndNorm;
            snapP.snapToGrid = true;
            snapP.gridBeatSeconds = 60.0 / nativeBpm;
            snapP.gridOffsetSeconds = gridOffsetSeconds;
            double snappedStart = 0.0, snappedEnd = 0.0;
            dsp::SamplePlayer::effectiveLoopBoundsSamples (snapP, (double) syncSample->lengthSamples(),
                                                            snappedStart, snappedEnd);
            const auto len = juce::jmax (1.0, (double) syncSample->lengthSamples());
            loopStartNorm = (float) (snappedStart / len);
            loopEndNorm = (float) (snappedEnd / len);
        }

        const auto lx0 = markerX (loopStartNorm);
        const auto lx1 = markerX (loopEndNorm);
        const auto bandX = juce::jmin (lx0, lx1);
        const auto bandW = std::abs (lx1 - lx0);

        g.setColour (t.accentMod.withAlpha (0.14f));
        g.fillRect (juce::Rectangle<float> (bandX, area.getY(), bandW, area.getHeight()));

        g.setColour (t.accentMod.withAlpha (0.85f));
        g.drawLine (lx0, area.getY(), lx0, area.getBottom(), 1.0f);
        g.drawLine (lx1, area.getY(), lx1, area.getBottom(), 1.0f);

        // Loop crossfade (XFADE) visualization: two matched ramps at the
        // seam showing WHERE the blend material comes from -- because the
        // crossfade borrows audio from OUTSIDE the loop, exactly one ramp
        // of each pair sits outside the band above (that's the borrowed
        // material) and the other sits inside it. Geometry comes from
        // getXfadeRamps() (single source of this geometry, also used by
        // tests), which calls the engine's own effectiveXfadeSamples on the
        // SAME effective (snapped when SYNC is on) bounds the engine
        // actually loops on, so this can never disagree with what's heard
        // -- including under SYNC, where the stretcher now applies this
        // same crossfade per-grain. Kept subordinate to the crisp accentMod
        // edge lines just drawn above: a soft fill (0.22 alpha, fading to
        // 0) plus a thin 0.7-alpha stroke of the actual equal-power curve
        // the engine uses.
        for (const auto& ramp : getXfadeRamps())
        {
            constexpr int steps = 20;
            const auto x0 = markerX (ramp.fromNorm);
            const auto x1 = markerX (ramp.toNorm);
            // At extreme zoom-out on a long file, x0/x1 can round to the
            // same (or a sub-pixel-apart) pixel; ColourGradient with
            // coincident points is degenerate, so skip drawing this ramp.
            if (std::abs (x1 - x0) < 0.5f)
                continue;
            const auto topY = area.getY();
            const auto bottomY = area.getBottom();

            juce::Path curve;
            for (int i = 0; i <= steps; ++i)
            {
                const auto w = (float) i / (float) steps;
                const auto ang = w * juce::MathConstants<float>::halfPi;
                const auto g01 = ramp.fadeIn ? std::sin (ang) : std::cos (ang);
                const auto x = x0 + (x1 - x0) * w;
                const auto y = bottomY + (topY - bottomY) * g01;
                if (i == 0) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
            }

            auto wedge = curve;
            wedge.lineTo (x1, bottomY);
            wedge.lineTo (x0, bottomY);
            wedge.closeSubPath();

            // Full alpha at the curve's peak end, fading to 0 at the other.
            const auto peakX = ramp.fadeIn ? x1 : x0;
            const auto zeroX = ramp.fadeIn ? x0 : x1;
            juce::ColourGradient grad (t.accentMod.withAlpha (0.22f), peakX, bottomY,
                                       t.accentMod.withAlpha (0.0f), zeroX, bottomY, false);
            g.setGradientFill (grad);
            g.fillPath (wedge);

            g.setColour (t.accentMod.withAlpha (0.7f));
            g.strokePath (curve, juce::PathStrokeType (1.0f));
        }
    }

    // LOOP + SYNC on: beat-grid ticks across the file, spaced at the
    // effective native BPM (override beats if set, else the loader's
    // detected tempo), anchored at the sample's first detected onset (not
    // t=0) so the grid lines up with the hits -- lets the user see how the
    // beat grid actually lines up with the waveform. Bar lines (every
    // beatsPerBar-th tick) draw heavier/brighter than plain beat ticks.
    if (syncOn && syncSample != nullptr && nativeBpm > 1.0 && lengthSeconds > 1.0e-6)
    {
        const auto beatSeconds = 60.0 / nativeBpm;
        const auto beatNorm = (float) (beatSeconds / lengthSeconds);
        if (beatNorm > 0.0005f)
        {
            // Follow THIS oscillator's own time signature (id::osc::timeSig)
            // when it's not "Host" -- lets the bar-line grid show the
            // polyrhythm a per-slot signature creates, matching the engine's
            // per-slot bar origin (see SPASynthVoice's use of
            // SlotStatic::beatsPerBar).
            const auto slotTimeSigChoice = juce::roundToInt (
                value (params::id::oscSlot (slot, params::id::osc::timeSig)));
            const auto effectiveBeatsPerBar = slotTimeSigChoice > 0
                ? params::id::timeSigBeatsPerBar (slotTimeSigChoice - 1)
                : processor.getCurrentBeatsPerBar();
            const auto beatsPerBar = juce::jmax (1, juce::roundToInt (effectiveBeatsPerBar));
            const auto offsetNorm = (float) (std::fmod (gridOffsetSeconds, beatSeconds) / lengthSeconds);
            // Walk both directions from the anchored offset so ticks cover
            // the whole file even when the first onset isn't near t=0.
            int beatIndex = (int) std::floor ((0.0f - offsetNorm) / beatNorm) - 1;
            for (float n = offsetNorm + (float) beatIndex * beatNorm; n < 1.0f; n += beatNorm, ++beatIndex)
            {
                if (n < 0.0f) continue;
                const auto isBar = beatIndex % beatsPerBar == 0;
                g.setColour (isBar ? t.textPrimary.withAlpha (0.5f) : t.textSecondary.withAlpha (0.3f));
                const auto tickH = isBar ? 10.0f : 6.0f;
                const auto tx = markerX (n);
                g.drawLine (tx, area.getY(), tx, area.getY() + tickH, isBar ? 1.5f : 1.0f);
            }
        }
    }

    // Granular while sounding: animate the live grain cloud (each grain is a
    // faint playhead scanning the buffer, brightness following its window) so
    // playback reads the way it does in other granular synths, plus a soft
    // centre marker at the grain position.
    if (mode == params::OscMode::granular && isLive() && telemetry != nullptr)
    {
        const auto& gv = telemetry->grainViz[(size_t) slot];
        const auto n = juce::jlimit (0, dsp::Telemetry::maxVizGrains,
                                     gv.count.load (std::memory_order_relaxed));
        for (int i = 0; i < n; ++i)
        {
            const auto gx = markerX (gv.pos[(size_t) i].load (std::memory_order_relaxed));
            const auto gamp = gv.amp[(size_t) i].load (std::memory_order_relaxed);
            g.setColour (t.accent.withAlpha (0.15f + 0.55f * gamp));
            g.drawLine (gx, area.getY(), gx, area.getBottom(), 1.0f);
        }

        const auto cx = markerX (telemetry->slotPosition[(size_t) slot]
                                     .load (std::memory_order_relaxed));
        g.setColour (t.textPrimary.withAlpha (0.55f));
        g.drawLine (cx, area.getY(), cx, area.getBottom(), 1.0f);
    }
    else if (mode == params::OscMode::sample)
    {
        // START point: always drawn, including while a note sounds -- it is
        // a setting being dialled in, and the moment you play a note to hear
        // the result is exactly when you still want to see where it is set.
        // Before 1.0.19 this shared one line with the playhead below and so
        // vanished during playback. Full height like the loop markers, in
        // textSecondary (dimmer than the accentMod loop start/end markers
        // and the bright textPrimary live playhead) so it stays visually
        // distinguishable from both rather than competing.
        const auto startX = markerX (value (params::id::oscSlot (slot, params::id::osc::sampleStart)));
        g.setColour (t.textSecondary.withAlpha (0.9f));
        g.drawLine (startX, area.getY(), startX, area.getBottom(), 1.0f);

        // Live playhead on top, brighter, so the two stay tellable apart
        // even when they coincide.
        if (isLive())
        {
            const auto px = markerX (telemetry->slotPosition[(size_t) slot].load (std::memory_order_relaxed));
            g.setColour (t.textPrimary);
            g.drawLine (px, area.getY(), px, area.getBottom(), 1.2f);
        }
    }
    else
    {
        // Granular (idle or live): a single marker, live playhead when
        // sounding, the grainPos knob otherwise. (Every other mode returns
        // earlier in paintDisplay() and never reaches this section; live
        // granular is also already fully handled above.) Unchanged from
        // before 1.0.19.
        const auto markerParam = mode == params::OscMode::granular
                               ? params::id::osc::grainPos : params::id::osc::sampleStart;
        const auto live = isLive();
        const auto marker = live
            ? telemetry->slotPosition[(size_t) slot].load (std::memory_order_relaxed)
            : value (params::id::oscSlot (slot, markerParam));

        g.setColour (t.textPrimary);
        g.drawLine (markerX (marker), area.getY(), markerX (marker), area.getBottom(), 1.2f);
    }

    g.restoreState();   // end of the area-clipped drawing above

    // Zoomed: a thin overview bar along the bottom edge showing the visible
    // window, plus a small "xN" readout -- Pro-audio convention (Pro-Q-style
    // zoom chip). Only shown while zoomed so the un-zoomed look is unchanged.
    if (viewLength < 1.0f)
    {
        constexpr float barHeight = 3.0f;
        const auto barY = area.getBottom() - barHeight;
        g.setColour (t.outline);
        g.fillRect (juce::Rectangle<float> (area.getX(), barY, area.getWidth(), barHeight));
        g.setColour (t.accentMod.withAlpha (0.8f));
        g.fillRect (juce::Rectangle<float> (normToX (viewStart, area), barY,
                                            juce::jmax (2.0f, area.getWidth() * viewLength), barHeight));

        const auto zoomFactor = juce::roundToInt (1.0f / viewLength);
        auto readoutArea = area;
        g.setColour (t.textSecondary);
        g.setFont (metrics::labelFont().withHeight (9.5f));
        g.drawText ("x" + juce::String (zoomFactor),
                   readoutArea.removeFromTop (11.0f).removeFromRight (28.0f),
                   juce::Justification::centredRight);
    }

    // A replacement is still loading: dim the stale waveform so it reads as
    // outgoing, and run the sweep on top.
    if (loading)
    {
        g.setColour (t.background.withAlpha (0.6f));
        g.fillRect (area);
        paintLoadingOverlay (g, area);
        markDirty();   // keep the 24 Hz timer animating the sweep
    }
}

// ============================ EnvDisplay ===================================

EnvDisplay::EnvDisplay (SPASynthProcessor& p, juce::String idPrefix, int envIndex)
    : DisplayComponent (p.getAPVTS(),
                        { idPrefix + ".attack", idPrefix + ".decay",
                          idPrefix + ".sustain", idPrefix + ".release" },
                        &p.getTelemetry()),
      prefix (std::move (idPrefix)), env (envIndex)
{
}

void EnvDisplay::paintDisplay (juce::Graphics& g, juce::Rectangle<float> area)
{
    const auto& t = currentTheme();

    // Live level bar on the right edge.
    if (isLive())
    {
        const auto level = juce::jlimit (0.0f, 1.0f,
            telemetry->envValue[(size_t) env].load (std::memory_order_relaxed));
        auto bar = area.removeFromRight (5.0f);
        g.setColour (t.knobTrack.withAlpha (0.6f));
        g.fillRect (bar);
        g.setColour (t.accentMod);
        g.fillRect (bar.removeFromBottom (bar.getHeight() * level));
        area.removeFromRight (3.0f);
    }

    // Perceptual (sqrt) time scaling keeps short envelopes readable.
    const auto a = std::sqrt (value (prefix + ".attack") / 10.0f);
    const auto d = std::sqrt (value (prefix + ".decay") / 10.0f);
    const auto s = value (prefix + ".sustain");
    const auto r = std::sqrt (value (prefix + ".release") / 20.0f);
    const auto total = juce::jmax (0.05f, a + d + 0.25f + r);

    const auto xAt = [&] (float seg) { return area.getX() + area.getWidth() * seg / total; };
    const auto yAt = [&] (float level) { return area.getBottom() - level * area.getHeight() * 0.92f; };

    juce::Path curve;
    curve.startNewSubPath (area.getX(), yAt (0.0f));
    curve.quadraticTo (xAt (a * 0.4f), yAt (0.85f), xAt (a), yAt (1.0f));
    curve.quadraticTo (xAt (a + d * 0.3f), yAt (s + (1.0f - s) * 0.25f), xAt (a + d), yAt (s));
    curve.lineTo (xAt (a + d + 0.25f), yAt (s));
    curve.quadraticTo (xAt (a + d + 0.25f + r * 0.3f), yAt (s * 0.25f), area.getRight(), yAt (0.0f));

    auto fill = curve;
    fill.lineTo (area.getRight(), area.getBottom());
    fill.lineTo (area.getX(), area.getBottom());
    fill.closeSubPath();

    g.setColour (t.accentMod.withAlpha (0.18f));
    g.fillPath (fill);
    draw::glowStroke (g, curve, t.accentMod, 1.6f);
}

// ============================ LFODisplay ===================================

LFODisplay::LFODisplay (SPASynthProcessor& p, int lfoIndex)
    : DisplayComponent (p.getAPVTS(),
                        { params::id::lfoParam (lfoIndex, params::id::lfo::shape),
                          params::id::lfoParam (lfoIndex, params::id::lfo::phase),
                          params::id::lfoParam (lfoIndex, params::id::lfo::unipolar),
                          params::id::lfoParam (lfoIndex, params::id::lfo::smooth),
                          params::id::lfoParam (lfoIndex, params::id::lfo::jitter) },
                        &p.getTelemetry()),
      lfo (lfoIndex)
{
}

void LFODisplay::paintDisplay (juce::Graphics& g, juce::Rectangle<float> area)
{
    lfoDisplayPaintCounter.fetch_add (1, std::memory_order_relaxed);
    const auto& t = currentTheme();
    const auto shape = (params::LFOShape) (int) value (
        params::id::lfoParam (lfo, params::id::lfo::shape));
    const auto phaseOffset = value (params::id::lfoParam (lfo, params::id::lfo::phase));
    const auto unipolar = value (params::id::lfoParam (lfo, params::id::lfo::unipolar)) >= 0.5f;
    const auto smoothAmount = value (params::id::lfoParam (lfo, params::id::lfo::smooth)) * 0.01f;
    const auto jitterAmount = value (params::id::lfoParam (lfo, params::id::lfo::jitter)) * 0.01f;

    juce::Random shRandom (42 + lfo);  // stable S&H preview
    float shValue = shRandom.nextFloat() * 2.0f - 1.0f;
    int shCycle = 0;

    // Fixed seed so the jitter trace is stable between repaints rather than
    // shimmering every time this repaints.
    juce::Random jitterRandom (142 + lfo);
    float jitterValue = jitterRandom.nextFloat() * 2.0f - 1.0f;
    int jitterCycle = 0;

    // Chunk-rate one-pole slew preview: the drawn cycle stands in for a
    // nominal 1-second span (the curve is phase-domain, not tempo-domain,
    // same as the rest of this preview), so each of the `steps` segments
    // previews one modulation chunk at that nominal rate.
    constexpr int steps = 160;
    constexpr float previewDt = 1.0f / (float) steps;
    float smoothed = 0.0f;
    bool smoothPrimed = false;

    juce::Path curve;
    for (int i = 0; i <= steps; ++i)
    {
        const auto raw = (float) i / steps + phaseOffset;
        const auto ph = raw - std::floor (raw);

        float v = 0.0f;
        switch (shape)
        {
            case params::LFOShape::sine:     v = std::sin (juce::MathConstants<float>::twoPi * ph); break;
            case params::LFOShape::triangle: v = 1.0f - 4.0f * std::abs (ph - 0.5f); break;
            case params::LFOShape::sawUp:    v = 2.0f * ph - 1.0f; break;
            case params::LFOShape::sawDown:  v = 1.0f - 2.0f * ph; break;
            case params::LFOShape::square:   v = ph < 0.5f ? 1.0f : -1.0f; break;
            case params::LFOShape::sampleHold:
            {
                const auto cycle = (int) (raw * 6.0f);
                if (cycle != shCycle)
                {
                    shCycle = cycle;
                    shValue = shRandom.nextFloat() * 2.0f - 1.0f;
                }
                v = shValue;
                break;
            }
        }

        // Same order of operations as LFO::processChunk: shape, then jitter,
        // then smoothing (on the bipolar value), then unipolar folding.
        if (jitterAmount > 0.0f)
        {
            const auto cycle = (int) (raw * 6.0f);   // same fake-cycle clock as the S&H preview
            if (cycle != jitterCycle)
            {
                jitterCycle = cycle;
                jitterValue = jitterRandom.nextFloat() * 2.0f - 1.0f;
            }
            v = (1.0f - jitterAmount) * v + jitterAmount * jitterValue;
        }

        if (smoothAmount > 0.0f)
        {
            const auto tau = 0.001f * std::pow (300.0f, smoothAmount);
            const auto alpha = 1.0f - std::exp (-previewDt / tau);
            if (! smoothPrimed)
            {
                smoothed = v;
                smoothPrimed = true;
            }
            else
            {
                smoothed += alpha * (v - smoothed);
            }
            v = smoothed;
        }

        if (unipolar)
            v = v * 0.5f + 0.5f;

        const auto x = area.getX() + area.getWidth() * (float) i / steps;
        const auto y = area.getCentreY() - v * area.getHeight() * 0.42f;
        if (i == 0)
            curve.startNewSubPath (x, y);
        else
            curve.lineTo (x, y);
    }

    draw::glowStroke (g, curve, t.accentMod, 1.6f);

    // Live playhead dot at (phase, value).
    if (isLive())
    {
        const auto phase = telemetry->lfoPhase[(size_t) lfo].load (std::memory_order_relaxed);
        const auto v = telemetry->lfoValue[(size_t) lfo].load (std::memory_order_relaxed);
        const auto x = area.getX() + area.getWidth() * juce::jlimit (0.0f, 1.0f, phase);
        const auto y = area.getCentreY() - v * area.getHeight() * 0.42f;
        g.setColour (t.textPrimary);
        g.fillEllipse (x - 3.0f, y - 3.0f, 6.0f, 6.0f);
    }
}

// =========================== FilterDisplay =================================

FilterDisplay::FilterDisplay (SPASynthProcessor& p, int filterIndex)
    : DisplayComponent (p.getAPVTS(),
                        filterIndex == 1
                            ? juce::StringArray { params::id::filter1Type,
                                                  params::id::filter1Cutoff,
                                                  params::id::filter1Resonance }
                            : juce::StringArray { params::id::filter2Type,
                                                  params::id::filter2Cutoff,
                                                  params::id::filter2Resonance,
                                                  params::id::filter2Enable },
                        &p.getTelemetry()),
      index (filterIndex)
{
}

void FilterDisplay::paintDisplay (juce::Graphics& g, juce::Rectangle<float> area)
{
    const auto& t = currentTheme();
    const bool second = index == 2;
    const auto enabled = ! second || value (params::id::filter2Enable) >= 0.5f;
    const auto type = (params::FilterType) (int) value (
        second ? params::id::filter2Type : params::id::filter1Type);

    // Live modulated cutoff/res while playing.
    const auto cutoff = isLive() && enabled
        ? (second ? telemetry->filter2CutoffHz : telemetry->filterCutoffHz)
              .load (std::memory_order_relaxed)
        : value (second ? params::id::filter2Cutoff : params::id::filter1Cutoff);
    const auto res = isLive() && enabled
        ? (second ? telemetry->filter2Resonance : telemetry->filterResonance)
              .load (std::memory_order_relaxed)
        : value (second ? params::id::filter2Resonance : params::id::filter1Resonance);
    const auto q = 0.5f + res * 4.5f;

    const bool is24 = type == params::FilterType::lp24 || type == params::FilterType::hp24
                   || type == params::FilterType::bp24 || type == params::FilterType::notch24;

    juce::Path curve;
    constexpr int steps = 140;
    for (int i = 0; i <= steps; ++i)
    {
        const auto freq = 20.0f * std::pow (1000.0f, (float) i / steps);
        const auto w = freq / juce::jmax (20.0f, cutoff);
        const auto w2 = w * w;
        const auto denom = std::sqrt ((1.0f - w2) * (1.0f - w2) + w2 / (q * q));

        float mag;
        switch (type)
        {
            case params::FilterType::lp12: case params::FilterType::lp24:
                mag = 1.0f / denom; break;
            case params::FilterType::hp12: case params::FilterType::hp24:
                mag = w2 / denom; break;
            case params::FilterType::bp12: case params::FilterType::bp24:
                mag = (w / q) / denom; break;
            case params::FilterType::notch12: case params::FilterType::notch24:
            default:
                mag = std::abs (1.0f - w2) / denom; break;
        }

        if (is24)
            mag *= mag;

        const auto dB = juce::jlimit (-36.0f, 18.0f,
                                      juce::Decibels::gainToDecibels (mag, -60.0f));
        const auto x = area.getX() + area.getWidth() * (float) i / steps;
        const auto y = juce::jmap (dB, -36.0f, 18.0f, area.getBottom(), area.getY());
        if (i == 0)
            curve.startNewSubPath (x, y);
        else
            curve.lineTo (x, y);
    }

    draw::glowStroke (g, curve,
                      enabled ? t.accent : t.textSecondary.withAlpha (0.45f), 1.6f);
}

// ============================ ChaosDisplay =================================

ChaosDisplay::ChaosDisplay (SPASynthProcessor& p)
    : DisplayComponent (p.getAPVTS(),
                        { params::id::chaos::depth, params::id::chaos::rate,
                          params::id::chaos::mix, params::id::chaos::enable,
                          params::id::chaos::syncToBpm, params::id::chaos::division },
                        &p.getTelemetry())
{
}

void ChaosDisplay::paintDisplay (juce::Graphics& g, juce::Rectangle<float> area)
{
    chaosDisplayPaintCounter.fetch_add (1, std::memory_order_relaxed);
    const auto& t = currentTheme();
    const auto enabled = value (params::id::chaos::enable) >= 0.5f;

    // Snapshot the most recent chaosTraceSize samples from the telemetry ring,
    // oldest first, newest last -> drawn newest-at-the-right. No allocation:
    // traceSnapshot is a preallocated member.
    constexpr int N = dsp::Telemetry::chaosTraceSize;
    bool everWritten = false;
    if (telemetry != nullptr)
    {
        const auto writeIdx = telemetry->chaosTraceWrite.load (std::memory_order_relaxed);
        everWritten = writeIdx > 0;
        for (int i = 0; i < N; ++i)
        {
            const auto idx = (writeIdx - N + i) & (N - 1);
            traceSnapshot[(size_t) i] = telemetry->chaosTrace[(size_t) idx]
                                            .load (std::memory_order_relaxed);
        }
    }
    else
    {
        traceSnapshot.fill (0.0f);
    }

    juce::Path curve;
    for (int i = 0; i < N; ++i)
    {
        const auto x = area.getX() + area.getWidth() * (float) i / (float) (N - 1);
        const auto y = area.getCentreY() - traceSnapshot[(size_t) i] * area.getHeight() * 0.45f;
        if (i == 0)
            curve.startNewSubPath (x, y);
        else
            curve.lineTo (x, y);
    }

    const auto live = isLive();
    const auto colour = enabled && live
                       ? t.accentMod
                       : t.textSecondary.withAlpha (0.4f);
    draw::glowStroke (g, curve, colour, 1.6f);

    // Live chaos output dot on the right edge, on the newest sample.
    if (enabled && live && everWritten)
    {
        const auto v = traceSnapshot[(size_t) (N - 1)];
        const auto y = area.getCentreY() - v * area.getHeight() * 0.45f;
        g.setColour (t.textPrimary);
        g.fillEllipse (area.getRight() - 7.0f, y - 3.0f, 6.0f, 6.0f);
    }
}

// ============================= FXDisplay ===================================

juce::StringArray FXDisplay::watchedFor (Kind kind)
{
    namespace fx = params::id::fx;
    switch (kind)
    {
        case Kind::distortion: return { fx::distEnable, fx::distType, fx::distDrive, fx::distMix };
        case Kind::chorus:     return { fx::chorusEnable, fx::chorusRate, fx::chorusDepth,
                                        fx::chorusMix };
        case Kind::delay:      return { fx::delayEnable, fx::delayFeedback, fx::delayPingPong,
                                        fx::delayMix };
        case Kind::reverb:     return { fx::reverbEnable, fx::reverbSize, fx::reverbDamping,
                                        fx::reverbMix };
        case Kind::eq:
        {
            juce::StringArray ids { fx::eqEnable, fx::eqCharacter };
            for (int b = 0; b < 8; ++b)
            {
                ids.add (params::id::eqBand (b, fx::eqband::enable));
                ids.add (params::id::eqBand (b, fx::eqband::type));
                ids.add (params::id::eqBand (b, fx::eqband::freq));
                ids.add (params::id::eqBand (b, fx::eqband::gain));
                ids.add (params::id::eqBand (b, fx::eqband::q));
            }
            return ids;
        }
    }
    return {};
}

FXDisplay::FXDisplay (juce::AudioProcessorValueTreeState& state, Kind k)
    : DisplayComponent (state, watchedFor (k)), kind (k)
{
}

void FXDisplay::paintDisplay (juce::Graphics& g, juce::Rectangle<float> area)
{
    namespace fx = params::id::fx;
    const auto& t = currentTheme();

    const auto enabledID = kind == Kind::distortion ? fx::distEnable
                         : kind == Kind::chorus     ? fx::chorusEnable
                         : kind == Kind::delay      ? fx::delayEnable
                         : kind == Kind::reverb     ? fx::reverbEnable : fx::eqEnable;
    const auto colour = value (enabledID) >= 0.5f ? t.accent
                                                  : t.textSecondary.withAlpha (0.45f);

    switch (kind)
    {
        case Kind::distortion:
        {
            // Transfer curve, input -1..1 -> output, with the dry diagonal.
            const auto drive = 1.0f + 15.0f * value (fx::distDrive);
            const auto type = (int) value (fx::distType);
            const auto mix = value (fx::distMix);

            g.setColour (t.outline);
            g.drawLine (area.getX(), area.getBottom(), area.getRight(), area.getY(), 1.0f);

            juce::Path curve;

            if (type == 3)
            {
                // Bit-crush staircase: fewer, wider steps as DRIVE (bit
                // depth reduction) increases, so the display reads as a
                // quantiser rather than a smooth shaper.
                const auto crushDrive = value (fx::distDrive);
                const auto crushLevels = std::pow (2.0f, dsp::FXChain::crushBitsForDrive (crushDrive));
                const auto numSteps = (float) juce::jlimit (3, 64, (int) crushLevels);

                for (int s = 0; s <= (int) numSteps; ++s)
                {
                    const auto in = -1.0f + 2.0f * (float) s / numSteps;
                    const auto quantised = std::round (in * crushLevels) / crushLevels;
                    const auto out = in + (quantised - in) * mix;

                    const auto px = area.getX() + area.getWidth() * (float) s / numSteps;
                    const auto py = area.getCentreY() - out * area.getHeight() * 0.46f;
                    if (s == 0)
                        curve.startNewSubPath (px, py);
                    else
                        curve.lineTo (px, py);

                    if ((float) s < numSteps)
                    {
                        const auto nextPx = area.getX() + area.getWidth() * (float) (s + 1) / numSteps;
                        curve.lineTo (nextPx, py);
                    }
                }
            }
            else
            {
                constexpr int steps = 96;
                for (int i = 0; i <= steps; ++i)
                {
                    const auto in = -1.0f + 2.0f * (float) i / steps;
                    const auto x = in * drive;
                    float wet;
                    switch (type)
                    {
                        case 1:  wet = juce::jlimit (-1.0f, 1.0f, x); break;
                        case 2:  wet = std::sin (x * 1.2f); break;
                        default: wet = std::tanh (x); break;
                    }
                    wet /= std::sqrt (drive);
                    const auto out = in + (wet - in) * mix;

                    const auto px = area.getX() + area.getWidth() * (float) i / steps;
                    const auto py = area.getCentreY() - out * area.getHeight() * 0.46f;
                    if (i == 0)
                        curve.startNewSubPath (px, py);
                    else
                        curve.lineTo (px, py);
                }
            }
            draw::glowStroke (g, curve, colour, 1.6f);
            break;
        }

        case Kind::chorus:
        {
            // Two detuned voices weaving around the dry centre line.
            const auto depth = value (fx::chorusDepth);
            const auto mix = value (fx::chorusMix);

            for (int voice = 0; voice < 2; ++voice)
            {
                juce::Path curve;
                constexpr int steps = 120;
                for (int i = 0; i <= steps; ++i)
                {
                    const auto ph = (float) i / steps * juce::MathConstants<float>::twoPi * 2.0f
                                  + (voice == 0 ? 0.0f : juce::MathConstants<float>::pi * 0.6f);
                    const auto v = std::sin (ph) * depth * (0.25f + 0.75f * mix);
                    const auto x = area.getX() + area.getWidth() * (float) i / steps;
                    const auto y = area.getCentreY() - v * area.getHeight() * 0.42f;
                    if (i == 0)
                        curve.startNewSubPath (x, y);
                    else
                        curve.lineTo (x, y);
                }
                draw::glowStroke (g, curve, voice == 0 ? colour : colour.withAlpha (0.55f), 1.4f);
            }
            break;
        }

        case Kind::delay:
        {
            // Echo taps decaying by feedback; ping-pong alternates sides.
            const auto feedback = value (fx::delayFeedback);
            const auto pingpong = value (fx::delayPingPong) >= 0.5f;
            const auto mix = value (fx::delayMix);

            const auto baseX = area.getX() + 6.0f;
            const auto spacing = (area.getWidth() - 12.0f) / 6.0f;

            // Dry impulse.
            g.setColour (t.textPrimary.withAlpha (0.8f));
            g.fillRect (juce::Rectangle<float> (baseX - 1.5f,
                                                area.getCentreY() - area.getHeight() * 0.45f,
                                                3.0f, area.getHeight() * 0.9f));

            float gain = mix;
            for (int tap = 1; tap <= 6 && gain > 0.02f; ++tap)
            {
                const auto h = area.getHeight() * 0.45f * gain;
                const auto x = baseX + spacing * (float) tap;
                const auto up = ! pingpong || (tap % 2 == 1);
                g.setColour (colour);
                g.fillRect (juce::Rectangle<float> (x - 1.5f,
                                                    up ? area.getCentreY() - h : area.getCentreY(),
                                                    3.0f, h));
                gain *= feedback;
            }

            g.setColour (t.outline.withAlpha (0.7f));
            g.drawHorizontalLine ((int) area.getCentreY(), area.getX(), area.getRight());
            break;
        }

        case Kind::reverb:
        {
            // Decay envelope; size stretches it, damping bows it down.
            const auto size = value (fx::reverbSize);
            const auto damping = value (fx::reverbDamping);
            const auto mix = value (fx::reverbMix);

            juce::Path curve;
            constexpr int steps = 100;
            for (int i = 0; i <= steps; ++i)
            {
                const auto x01 = (float) i / steps;
                const auto decay = 1.2f + (1.0f - size) * 6.0f + damping * 2.0f;
                const auto v = (0.2f + 0.8f * mix) * std::exp (-decay * x01);
                const auto x = area.getX() + area.getWidth() * x01;
                const auto y = area.getBottom() - v * area.getHeight() * 0.92f;
                if (i == 0)
                    curve.startNewSubPath (x, y);
                else
                    curve.lineTo (x, y);
            }

            auto fill = curve;
            fill.lineTo (area.getRight(), area.getBottom());
            fill.lineTo (area.getX(), area.getBottom());
            fill.closeSubPath();
            g.setColour (colour.withAlpha (0.18f));
            g.fillPath (fill);
            draw::glowStroke (g, curve, colour, 1.6f);
            break;
        }

        case Kind::eq:
        {
            // Composite response of the 8 active parametric bands, +/-24 dB,
            // computed by the same function the DSP uses.
            std::array<dsp::ParametricEQ::Band, 8> bands;
            for (int b = 0; b < 8; ++b)
            {
                auto& bd = bands[(size_t) b];
                bd.enabled = value (params::id::eqBand (b, fx::eqband::enable)) >= 0.5f;
                bd.type    = (int) value (params::id::eqBand (b, fx::eqband::type));
                bd.freq    = value (params::id::eqBand (b, fx::eqband::freq));
                bd.gainDb  = value (params::id::eqBand (b, fx::eqband::gain));
                bd.q       = value (params::id::eqBand (b, fx::eqband::q));
            }

            g.setColour (t.outline.withAlpha (0.7f));
            g.drawHorizontalLine ((int) area.getCentreY(), area.getX(), area.getRight());

            juce::Path curve;
            constexpr int steps = 160;
            for (int i = 0; i <= steps; ++i)
            {
                const auto freq = 20.0f * std::pow (1000.0f, (float) i / steps);
                const auto dB = juce::jlimit (-24.0f, 24.0f,
                    dsp::ParametricEQ::magnitudeDb (bands, freq, 48000.0));
                const auto x = area.getX() + area.getWidth() * (float) i / steps;
                const auto y = juce::jmap (dB, -24.0f, 24.0f, area.getBottom(), area.getY());
                if (i == 0)
                    curve.startNewSubPath (x, y);
                else
                    curve.lineTo (x, y);
            }
            draw::glowStroke (g, curve, colour, 1.6f);
            break;
        }
    }
}

// ============================ OutputMeter ==================================

OutputMeter::OutputMeter (const dsp::Telemetry& tel)
    : telemetry (tel)
{
    setInterceptsMouseClicks (false, false);
    startTimerHz (30);
}

void OutputMeter::timerCallback()
{
    const auto attackRelease = [] (float current, float target)
    {
        return target > current ? target : current * 0.82f;
    };
    levelL = attackRelease (levelL, telemetry.peakL.load (std::memory_order_relaxed));
    levelR = attackRelease (levelR, telemetry.peakR.load (std::memory_order_relaxed));
    repaint();
}

void OutputMeter::paint (juce::Graphics& g)
{
    const auto& t = currentTheme();
    auto bounds = getLocalBounds().toFloat();
    const auto barW = (bounds.getWidth() - 2.0f) / 2.0f;

    const auto drawBar = [&] (juce::Rectangle<float> bar, float level)
    {
        // A meter still needs a visible lane, but not the old display-well
        // black -- use meterLane (iteration 3: seam itself darkened enough
        // that it started reading as display-well black again, so the meter
        // now has its own, lighter, token instead of following seam down).
        g.setColour (t.meterLane);
        g.fillRoundedRectangle (bar, 1.5f);

        const auto dB = juce::Decibels::gainToDecibels (level, -60.0f);
        const auto h = juce::jmap (juce::jlimit (-60.0f, 0.0f, dB), -60.0f, 0.0f,
                                   0.0f, bar.getHeight());
        auto fill = bar.removeFromBottom (h);
        g.setColour (dB > -3.0f ? t.accent : t.accentMod);
        g.fillRoundedRectangle (fill, 1.5f);
    };

    drawBar (bounds.removeFromLeft (barW), levelL);
    bounds.removeFromLeft (2.0f);
    drawBar (bounds, levelR);
}

} // namespace spa::ui
