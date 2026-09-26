#pragma once

#include "Theme.h"
#include "../params/ParameterRegistry.h"
#include "../dsp/Telemetry.h"
#include "../dsp/LFO.h"
#include <vector>

namespace spa
{
class SPASynthProcessor;

namespace ui
{

// Base for the module scopes: watches a set of parameters and repaints (at a
// throttled rate) when any of them move. When given a telemetry pointer it
// also animates continuously while voices are sounding, so the scopes show
// the *modulated* state, not just knob positions.
class DisplayComponent : public juce::Component,
                         private juce::Timer,
                         private juce::AudioProcessorValueTreeState::Listener
{
public:
    DisplayComponent (juce::AudioProcessorValueTreeState&, juce::StringArray paramIDs,
                      const dsp::Telemetry* telemetry = nullptr);
    ~DisplayComponent() override;

    void paint (juce::Graphics&) final;
    void markDirty() { dirty.store (true); }

protected:
    virtual void paintDisplay (juce::Graphics&, juce::Rectangle<float>) = 0;

    // Subclasses that keep an idle animation running (a scrolling trace, a
    // travelling playhead) override this so the 24Hz timer keeps repainting
    // while it's true, paired with isShowing() so a hidden tab never costs
    // anything. Default false preserves every existing display's behaviour
    // (repaint only on a watched-parameter change, or while isLive()).
    virtual bool wantsAnimation() const { return false; }

    bool isLive() const;   // voices currently sounding

    juce::AudioProcessorValueTreeState& apvts;
    const dsp::Telemetry* telemetry = nullptr;

    float value (const juce::String& paramID) const;   // real-world value

private:
    void parameterChanged (const juce::String&, float) override { dirty.store (true); }
    void timerCallback() override;

    juce::StringArray watched;
    std::atomic<bool> dirty { true };
};

// Oscillator scope: wavetable frame at the (live, modulated) position, or the
// loaded sample's waveform with a moving playhead.
class WaveDisplay : public DisplayComponent,
                    public juce::SettableTooltipClient,
                    private juce::ChangeListener
{
public:
    WaveDisplay (SPASynthProcessor&, int slot);
    ~WaveDisplay() override;

    // Zoom/pan view state for the sample/granular waveform (normalized 0..1
    // fractions of the file, same domain as sampleStart/loopStart/loopEnd).
    // UI-only -- never serialized, never touches DSP; resets on a new
    // content load. Exposed for tests (waveDisplayZoomTest).
    float getViewStart() const  { return viewStart; }
    float getViewLength() const { return viewLength; }
    static constexpr float minViewLength = 1.0f / 64.0f;

    // Maps a normalized (0..1) file position to/from an x coordinate within
    // `area` (the same reduced bounds paintDisplay draws into -- see
    // waveArea()), honoring the current zoom/pan. Shared by paint and the
    // gesture handlers, and by tests, so overlays can never drift from what
    // was actually drawn.
    float normToX (float norm, juce::Rectangle<float> area) const;
    float xToNorm (float x, juce::Rectangle<float> area) const;
    juce::Rectangle<float> waveArea() const;

    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMagnify (const juce::MouseEvent&, float scaleFactor) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    // Loop crossfade (XFADE) ramp geometry, normalized 0..1 file positions --
    // computed via dsp::SamplePlayer::effectiveXfadeSamples on the SAME
    // effective (snapped when SYNC is on) bounds the engine actually loops
    // on, so this can never disagree with what's heard. This is the SINGLE
    // SOURCE of the drawn geometry: paintDisplay() calls this and draws
    // exactly these regions (not a separate re-derivation), and it is also
    // used directly by tests to assert on real geometry without a pixel
    // read. Empty when nothing would be drawn (mode != sample, LOOP off, or
    // no crossfade room). insideBand marks the ramp that sits within
    // [loopStart, loopEnd] -- the other one, outside the band, is the
    // actually-borrowed material (see the class comment in Displays.cpp for
    // why both directions exist).
    struct XfadeRamp
    {
        float fromNorm = 0.0f;
        float toNorm = 0.0f;
        bool insideBand = false;
        bool fadeIn = false;
    };
    std::vector<XfadeRamp> getXfadeRamps() const;

    // Organic Chaos visualization geometry: how the drawn waveform SHAPE is
    // displaced this paint, from the pitch / phase drift the narrating voice
    // publishes (Telemetry::slotChaosPitch/Phase). Phase drift slides the
    // shape horizontally (wrapping), pitch drift stretches it about the
    // centre (pitch up = more cycles visible). Both carry a deliberate
    // visual gain (see chaosVizPhaseGain / chaosVizPitchGain in
    // Displays.cpp) so the DEFAULT drift amounts read on the display.
    // Which of the two cues is legitimate depends on the OSC MODE -- a cue
    // is only drawn where that drift actually reaches the engine's audio
    // and where the drawn thing represents it (wavetable: both; analog /
    // FM / pluck: stretch only; noise, sample, granular: neither). See the
    // rule spelled out above chaosViz() in Displays.cpp.
    // Position drift is not here -- it already reaches the display through
    // slotPosition (and the granular grain markers). SINGLE SOURCE of the drawn geometry: paintDisplay()
    // applies exactly this (via chaosViz()), and tests read it through
    // getChaosVizOffsetsForTest() without a pixel read. Both are exactly 0
    // (no displacement) whenever the slot is idle, chaos is inactive, or
    // that drift is off, so the drawing is then bit-identical to the
    // undrifted one.
    struct ChaosViz
    {
        float slidePx = 0.0f;   // horizontal slide of the shape, pixels, wrapped to +-width/2
        float stretch = 0.0f;   // horizontal scale factor minus 1 (0 = none; >0 = more cycles)
    };
    ChaosViz getChaosVizOffsetsForTest() const { return chaosViz (waveArea()); }

private:
    void paintDisplay (juce::Graphics&, juce::Rectangle<float>) override;
    ChaosViz chaosViz (juce::Rectangle<float> area) const;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void zoomAt (float normCursor, float factor);

    SPASynthProcessor& processor;
    const int slot;

    float viewStart = 0.0f;    // 0..1, left edge of the visible window
    float viewLength = 1.0f;   // minViewLength..1, width of the visible window
    float dragStartViewStart = 0.0f;
    float dragAnchorX = 0.0f;
    const void* lastSample = nullptr;   // identity check -> reset view on a new load
};

// ADSR curve with translucent fill and a live output-level bar.
class EnvDisplay : public DisplayComponent
{
public:
    EnvDisplay (SPASynthProcessor&, juce::String idPrefix, int envIndex);

    // Maps (stage, progress-within-stage) to a point ON the drawn ADSR
    // curve, in the same (segment, level) space paintDisplay's own curve
    // uses -- a/d/r are the display's sqrt-scaled attack/decay/release
    // lengths and s the sustain level (the exact locals paintDisplay derives
    // from the envelope's own parameters). paintDisplay calls this same
    // function for the live playhead dot, so there is one source of truth
    // for "is the dot on the curve" rather than two copies that could drift.
    // Exposed (rather than local to paintDisplay) so tests can verify it.
    static void curvePoint (dsp::Telemetry::EnvStage stage, float progress,
                            float a, float d, float s, float r,
                            float& segOut, float& levelOut);

private:
    void paintDisplay (juce::Graphics&, juce::Rectangle<float>) override;
    juce::String prefix;
    const int env;
};

// One cycle of the LFO shape with a live playhead dot. When the LFO's shape
// is Custom, this also IS the breakpoint editor (1.0.25): drag points, add/
// remove them, drag a segment's handle to bend it. Editing is a no-op for
// every other shape (see isCustomActive()) -- the component always accepts
// mouse clicks (unlike a plain DisplayComponent, which ignores them), but
// never steals keyboard focus (setMouseClickGrabsKeyboardFocus(false), the
// same rule every other control in this codebase follows).
class LFODisplay : public DisplayComponent
{
public:
    LFODisplay (SPASynthProcessor&, int lfoIndex);

    // Coordinate mapping shared by paint, the editor gestures, and tests --
    // same idiom as WaveDisplay's waveArea()/normToX(). area.reduced(3.0f)
    // of the local bounds, matching DisplayComponent::paint(); x maps phase
    // 0..1, y maps the bipolar value -1..1 (with the same 0.42f vertical
    // scale the shape curve itself is drawn at).
    juce::Rectangle<float> curveArea() const { return getLocalBounds().toFloat().reduced (3.0f); }
    juce::Point<float> pointToXY (float xNorm, float yNorm, juce::Rectangle<float> area) const;
    float xToPhase (float x, juce::Rectangle<float> area) const;
    float yToValue (float y, juce::Rectangle<float> area) const;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    bool isCustomActive() const;   // exposed for tests

    // Test helpers -- observe editor gesture state without a pixel read.
    int getSelectedPointForTest() const { return selectedPoint; }
    int hitTestPointForTest (juce::Point<float> pos) const;
    int hitTestHandleForTest (juce::Point<float> pos) const;

private:
    void paintDisplay (juce::Graphics&, juce::Rectangle<float>) override;
    int hitTestPoint (juce::Point<float> pos, juce::Rectangle<float> area,
                      const dsp::CustomLFOShape&) const;
    int hitTestHandle (juce::Point<float> pos, juce::Rectangle<float> area,
                       const dsp::CustomLFOShape&) const;
    void showDeletePointMenu (int pointIndex);

    SPASynthProcessor& processor;
    const int lfo;

    int selectedPoint = -1;
    int hoverPoint = -1;
    int hoverHandle = -1;

    int draggingPoint = -1;
    int draggingHandle = -1;
    juce::Point<float> dragStartMouse;
    float dragStartPointX = 0.0f, dragStartPointY = 0.0f;
    float dragStartCurve = 0.0f;
};

// Approximate filter magnitude response; follows the modulated cutoff live.
class FilterDisplay : public DisplayComponent
{
public:
    FilterDisplay (SPASynthProcessor&, int filterIndex);

private:
    void paintDisplay (juce::Graphics&, juce::Rectangle<float>) override;
    const int index;
};

// A representative chaos walk with a live output dot.
class ChaosDisplay : public DisplayComponent
{
public:
    explicit ChaosDisplay (SPASynthProcessor&);

private:
    void paintDisplay (juce::Graphics&, juce::Rectangle<float>) override;

    // Preallocated snapshot of the telemetry trace ring, read once per paint.
    std::array<float, dsp::Telemetry::chaosTraceSize> traceSnapshot {};
};

// FX scopes: one class, five characters.
class FXDisplay : public DisplayComponent
{
public:
    // mod = phaser/flanger (fxMod section); tremVib = tremolo+vibrato
    // (fxTremVib section). Both used to be constructed as Kind::chorus by
    // mistake (SPASynthEditor.cpp), so MOD and TREM/VIB drew the chorus
    // picture instead of their own -- fixed in 1.0.25.
    enum class Kind { distortion, chorus, delay, reverb, eq, mod, tremVib };

    // telemetry: optional (nullptr keeps the pre-tempo-plumbing 120 BPM
    // fallback for synced Delay/Mod/Trem/Vib); FXPanel passes the
    // processor's real Telemetry so those kinds can read the live resolved
    // tempo (Telemetry::bpm) instead of guessing.
    FXDisplay (juce::AudioProcessorValueTreeState&, Kind, const dsp::Telemetry* telemetry = nullptr);

private:
    void paintDisplay (juce::Graphics&, juce::Rectangle<float>) override;
    bool wantsAnimation() const override;
    static juce::StringArray watchedFor (Kind);

    const Kind kind;
    // The bpm last used to paint a synced Delay/Mod/Trem/Vib -- lets
    // wantsAnimation() notice a tempo change (host tempo automation, tap
    // tempo) and mark itself dirty even while the effect itself is
    // disabled/static, without repainting every 24Hz tick just to poll it.
    mutable float lastDrawnBpm = -1.0f;
};

// Stereo output peak meter for the header, fed by telemetry.
class OutputMeter : public juce::Component,
                    private juce::Timer
{
public:
    explicit OutputMeter (const dsp::Telemetry&);

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    const dsp::Telemetry& telemetry;
    float levelL = 0.0f, levelR = 0.0f;
};

} // namespace ui
} // namespace spa
