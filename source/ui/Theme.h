#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../library/Library.h"
#include <cmath>

namespace spa::ui
{

// Runtime design tokens. Every colour, font and metric the UI uses comes
// through here so the whole look can be restyled without touching component
// code.
//
// Direction (faceplate restyle): one continuous charcoal-graphite surface —
// no per-module cards, no LED-screen display wells. Modules are separated by
// dark recessed seams and soft horizontal shadow bands painted directly on
// the surface (see ContentComponent::paint), not by their own panel fills.
// `panel` intentionally equals `background` so anything that still fills
// with it (transient overlays: popups, the preset drawer, call-outs) reads
// as the same surface rather than a card. Flat knobs, hairline rules and
// both teal accents (#51D0BF, user-tintable) are unchanged from the prior
// direction.
struct Theme
{
    juce::Colour background;      // window / continuous faceplate surface
    juce::Colour panel;           // transient overlay fills (popups, drawer) — == background
    juce::Colour display;         // recessed fields (combo/chip backgrounds); NOT used by scopes/wells
    juce::Colour header;          // top bar / footer rail
    juce::Colour seam;            // recessed vertical grooves between modules, tab hover fill,
                                   // call-out edges -- NOT the OutputMeter lane, see meterLane
    juce::Colour meterLane;       // OutputMeter's unlit bar lane -- deliberately lighter than
                                   // seam (iteration 3: seam darkened into near-display-well
                                   // territory, which read as a dead/black meter; split off so
                                   // the groove could go darker without dragging the meter down)
    juce::Colour textPrimary;
    juce::Colour textSecondary;
    juce::Colour accent;          // audio signal, primary actions
    juce::Colour accentMod;       // modulation (LFO/env/matrix/chaos)
    juce::Colour outline;         // hairlines
    juce::Colour knobFace;
    juce::Colour knobTrack;
    juce::Colour assignGlow;      // ASSIGN mode: pulsing blue glow on assignable controls/menus
    juce::Colour assignSelected;  // ASSIGN mode: solid colour for the currently selected object
    juce::Colour assignWaiting;   // ASSIGN mode: a matrix row half-filled, waiting for its other half

    static Theme dark()
    {
        // Flat charcoal-graphite faceplate register. (Iteration 2, 2026-08-30:
        // darkened toward Mike's design spec — background sampled directly off
        // the spec mock, the rest offset-preserved from the old background so
        // every control keeps its original relative contrast.)
        Theme t;
        t.background    = juce::Colour (0xff181d20);
        t.panel         = t.background;
        t.display       = juce::Colour (0xff0d1318);
        t.header        = juce::Colour (0xff13171a);
        t.seam          = t.background.darker (1.3f);
        t.meterLane     = t.background.darker (0.45f);   // the old seam tone, kept for the meter
        t.textPrimary   = juce::Colour (0xffe7ecef);
        t.textSecondary = juce::Colour (0xff8b989f);
        t.accent        = juce::Colour (0xff51d0bf);
        t.accentMod     = juce::Colour (0xff51d0bf);
        t.outline       = juce::Colour (0xff2a3337);
        t.knobFace      = juce::Colour (0xff232b31);
        t.knobTrack     = juce::Colour (0xff2f393e);
        t.assignGlow     = juce::Colour (0xff4aa3ff);
        t.assignSelected = juce::Colour (0xffffd54a);
        t.assignWaiting  = juce::Colour (0xffff8a3d);
        return t;
    }

};

// Process-wide active theme (shared by all editor instances). Components
// read colours at paint time, so a change only requires a repaint +
// LookAndFeel palette refresh.
const Theme& currentTheme();

// User accent overrides (persisted machine preference): the audio accent
// (orange by default) and the modulation accent (cyan by default).
void setAccentColors (juce::Colour audio, juce::Colour mod);
void resetAccentColors();

// "This parameter is assigned in the mod matrix" indicator colour.
// This is patch information, not decoration -- unlike accent/accentMod
// above, it is NOT part of the user's tintable accent system: the accent
// picker, LINK, and setAccentColors()/resetAccentColors() must never write
// it, and no library:: settings path stores it directly. But it DOES need
// to always contrast with whatever accent(s) the user has chosen, so
// (2026-09-18) it is now DERIVED from the current accent(s) rather than a
// fixed constant -- recomputed fresh on every call (never cached) by
// reading spa::ui::currentTheme() and library::getAccentsLinked() live, so
// it tracks accent changes immediately with no stale state to invalidate.
//
// Derivation rule:
//  - Accents LINKED (one colour): the complement of that accent's hue
//    (+180 degrees). Simple opposite-on-the-wheel, maximal contrast against
//    a single colour.
//  - Accents UNLINKED (two colours): the hue that maximizes the MINIMUM
//    angular distance to EITHER accent hue. Closed form: let h1, h2 be the
//    two hues and d their minor-arc separation (0 <= d <= 180; the major
//    arc is 360-d). The midpoint of the MAJOR arc sits 180-d/2 degrees from
//    each of h1 and h2 along that arc -- strictly more than the minor arc's
//    midpoint (d/2 from each) -- so it's the maximizing choice, computed
//    below via a vector-sum bisector (circularMidpointDegrees) rather than
//    a directional branch, so it's correct at every separation including
//    d==0 and the d==180 (opposite-hues) degeneracy. The guaranteed
//    distance 180-d/2 decreases as d grows, so its WORST CASE over every
//    possible accent pair is at d's own maximum, d=180 (accents already
//    exactly opposite): 180-90 = exactly 90 degrees to the nearer accent.
//    At the other extreme, d==0 (identical hues), it reduces to the same
//    +180 complement as the linked case, so the two rules agree in the
//    limit.
//  - Undefined hue (near-grey or near-black accent, where hue is numerically
//    meaningless -- see hueIsDefined): falls back to the hue of the
//    ORIGINAL fixed violet this feature shipped with (0xff9d84f0, ~254
//    degrees), so a grey/near-black accent still yields the same familiar
//    landmark colour rather than an arbitrary pick.
//
// Saturation/brightness are NOT carried over from the accent -- a raw
// accent's S/B could be washed-out or near-black, which would defeat the
// whole point of a guaranteed-contrasting indicator. Clamped instead to a
// fixed, always-legible range against the charcoal faceplate
// (background ~0xff181d20): S in [0.55, 0.75], B in [0.85, 0.95]. This
// keeps the colour saturated and bright enough to read clearly next to a
// muted accent (the default teal, ~45% saturation) or a vivid one, without
// ever going pastel-washed-out (low S) or neon-clipped (B==1). The
// midpoint of each range (S=0.65, B=0.90) is used directly -- there is no
// per-accent input to blend toward, so a fixed legible point in the
// documented range is simplest and fully deterministic.
inline constexpr float kAssignedColourMinSat = 0.55f;
inline constexpr float kAssignedColourMaxSat = 0.75f;
inline constexpr float kAssignedColourMinBri = 0.85f;
inline constexpr float kAssignedColourMaxBri = 0.95f;

// Below this saturation/brightness, JUCE's Colour::getHue() returns a
// meaningless value (0/red for pure greys) -- treat the hue as undefined
// and use the fixed fallback instead of deriving nonsense. Threshold is
// generously above float noise but well below anything a user would call
// "tinted".
inline constexpr float kHueUndefinedThreshold = 0.06f;

inline bool hueIsDefined (juce::Colour c)
{
    return c.getSaturation() > kHueUndefinedThreshold
        && c.getBrightness() > kHueUndefinedThreshold;
}

inline float wrapHueDegrees (float h)
{
    h = std::fmod (h, 360.0f);
    return h < 0.0f ? h + 360.0f : h;
}

// Angular distance between two hues (degrees), always in [0,180].
inline float hueDistanceDegrees (float a, float b)
{
    const auto d = std::abs (wrapHueDegrees (a) - wrapHueDegrees (b));
    return d > 180.0f ? 360.0f - d : d;
}

// The original fixed violet's hue -- kept as the documented fallback for
// an undefined accent hue (see modAssignedColour() above).
inline float assignedColourFallbackHueDegrees()
{
    static const float hue = juce::Colour (0xff9d84f0).getHue() * 360.0f;
    return hue;
}

// The hue that bisects h1/h2 along whichever arc is requested: `near` picks
// the shorter arc's midpoint, false picks the midpoint of the longer arc
// (180 degrees from the near one). Implemented as a vector-sum bisector
// rather than a directional branch so it's correct (and revert-detectable)
// at every separation, including the exact-opposite degeneracy where the
// two hues' vectors cancel -- handled explicitly below.
inline float circularMidpointDegrees (float h1Degrees, float h2Degrees, bool near)
{
    const auto r1 = juce::degreesToRadians (h1Degrees);
    const auto r2 = juce::degreesToRadians (h2Degrees);
    const auto sx = std::cos (r1) + std::cos (r2);
    const auto sy = std::sin (r1) + std::sin (r2);
    if (std::abs (sx) < 1.0e-5 && std::abs (sy) < 1.0e-5)
    {
        // h1 and h2 are exactly opposite (d==180): every hue on the wheel
        // is equidistant along one perpendicular axis, so +/-90 from h1 is
        // a deterministic, valid pick -- still exactly 90 degrees from
        // both, matching the documented worst-case bound.
        return wrapHueDegrees (h1Degrees + (near ? 90.0f : -90.0f));
    }
    auto angle = juce::radiansToDegrees ((float) std::atan2 (sy, sx));
    if (! near)
        angle += 180.0f;
    return wrapHueDegrees (angle);
}

inline juce::Colour modAssignedColour()
{
    const auto& t = currentTheme();
    const bool linked = library::getAccentsLinked();

    const bool accentDefined = hueIsDefined (t.accent);
    const bool modDefined = hueIsDefined (t.accentMod);

    float hue;
    if (linked)
    {
        hue = accentDefined ? wrapHueDegrees (t.accent.getHue() * 360.0f + 180.0f)
                             : assignedColourFallbackHueDegrees();
    }
    else if (accentDefined && modDefined)
    {
        hue = circularMidpointDegrees (t.accent.getHue() * 360.0f,
                                        t.accentMod.getHue() * 360.0f, false);
    }
    else if (accentDefined)
        hue = wrapHueDegrees (t.accent.getHue() * 360.0f + 180.0f);
    else if (modDefined)
        hue = wrapHueDegrees (t.accentMod.getHue() * 360.0f + 180.0f);
    else
        hue = assignedColourFallbackHueDegrees();

    return juce::Colour::fromHSV (hue / 360.0f,
                                   (kAssignedColourMinSat + kAssignedColourMaxSat) * 0.5f,
                                   (kAssignedColourMinBri + kAssignedColourMaxBri) * 0.5f,
                                   1.0f);
}

namespace metrics
{
    inline constexpr int baseWidth = 1380;
    inline constexpr int baseHeight = 900;
    inline constexpr int brandBandHeight = 34;   // centred wordmark strip
    inline constexpr int headerHeight = 54;
    inline constexpr int lockRowHeight = 26;
    inline constexpr int footerHeight = 24;
    inline constexpr int keyboardStripHeight = 96;   // on-screen keyboard when shown
    inline constexpr int presetBrowserWidth = 320;   // drawer column width; widens the
                                                      // window by this amount when open
                                                      // (rather than overlaying) -- see
                                                      // ContentComponent::getContentBaseWidth
    inline constexpr int unit = 8;
    inline constexpr float cornerRadius = 7.0f;  // softer, elevated panels

    // Randomizer lock strip caption ("LOCKS ->"). Named pieces so the
    // painted caption region (ContentComponent::paint) and the layout inset
    // that skips past it (ContentComponent::resized) can never drift apart --
    // they used to be two hand-coupled magic numbers (44 painted / 46 skipped).
    //
    // The text is drawn right-justified within lockCaptionTextWidth (rather
    // than left-justified in a wider box) so the visible gap to the arrow is
    // exactly lockCaptionArrowGap and nothing else -- a left-justified box
    // leaves leftover slack between the end of the glyphs and the edge of
    // the box, which reads as a bigger gap than the constant says. Chose
    // right-justification over measuring the glyphs with a
    // GlyphArrangement: it can't drift from what's actually painted (no
    // second measurement to keep in sync) and holds at any UI scale or
    // future font change with zero extra code.
    //
    // lockCaptionLeadingIndent moves the text off the panel's left edge
    // (product-owner request: "LOCKS" read as too close to the edge) while
    // lockCaptionTextWidth shrinks by the same amount, so the indent and the
    // narrower box net to zero -- the arrow and the first lock button keep
    // their on-screen position exactly (see lockCaptionWidth below).
    inline constexpr int lockCaptionLeadingIndent = 14;  // extra air before the "LOCKS" text
    inline constexpr int lockCaptionTextWidth = 30;   // room for the "LOCKS" text itself
    inline constexpr int lockCaptionArrowGap = 6;     // air between the text and the arrow
    inline constexpr int lockCaptionArrowWidth = 14;  // the arrow glyph's own footprint
    inline constexpr int lockCaptionTrailingGap = 8;  // air between the arrow and the first lock button
    // Total width of the caption region resized() must skip before laying
    // out the lock buttons -- derived from the pieces above so paint()
    // and resized() can never disagree about where the caption ends.
    inline constexpr int lockCaptionWidth = lockCaptionLeadingIndent + lockCaptionTextWidth
                                             + lockCaptionArrowGap
                                             + lockCaptionArrowWidth + lockCaptionTrailingGap;

    // Section-title row (draw::sectionHeader) reserved from the top of every
    // module panel's bounds. Shared so any site that needs to know where the
    // header ends and content begins -- OscStrip's headerNameRect click/popup
    // hit-test chief among them (iteration 3 restyle bug: it used to hardcode
    // its own copy of this, and every resized() below independently re-trimmed
    // it too) -- can't drift out of sync with what sectionHeader() actually
    // paints. Grew from 20 (iteration 2) for more air around the title, per
    // Mike's spec mock.
    inline constexpr int sectionHeaderHeight = 32;
    inline constexpr int sectionHeaderTopInset = 6;    // air above the title text
    inline constexpr int sectionHeaderLeftInset = 12;  // air to the left of the title text

    inline juce::Font titleFont()   { return juce::Font (juce::FontOptions (17.0f, juce::Font::bold)); }
    inline juce::Font sectionFont()
    {
        return juce::Font (juce::FontOptions (12.0f, juce::Font::bold))
                   .withExtraKerningFactor (0.06f);
    }
    inline juce::Font labelFont()   { return juce::Font (juce::FontOptions (11.0f)); }
    inline juce::Font smallFont()
    {
        return juce::Font (juce::FontOptions (9.5f)).withExtraKerningFactor (0.05f);
    }
    inline juce::Font smallFontBold()
    {
        return juce::Font (juce::FontOptions (9.5f, juce::Font::bold)).withExtraKerningFactor (0.05f);
    }
    inline juce::Font wordmarkFont()
    {
        // The big tracked wordmark: A R S E N A L
        return juce::Font (juce::FontOptions (21.0f, juce::Font::plain))
                   .withExtraKerningFactor (0.42f);
    }
    inline juce::Font brandSubFont()
    {
        return juce::Font (juce::FontOptions (8.5f)).withExtraKerningFactor (0.30f);
    }
}

// Shared painting helpers so every module reads as one system.
namespace draw
{
    // Flat module panel: fill + hairline.
    void panel (juce::Graphics&, juce::Rectangle<float>);

    // Draws heavily-tracked text (extraKerningFactor fonts, e.g. the
    // wordmark) genuinely centred on its INK rather than its advance box --
    // a tracked font's advance box carries a trailing kern gap that plain
    // drawText's centring measures against, so the visible glyphs land off-
    // centre (worse the more tracking). Renders the glyphs to a path and
    // centres the path's own bounds instead. Shared by the brand band
    // (SPASynthEditor.cpp) and the About panel wordmark.
    void trackedCentredText (juce::Graphics&, const juce::Font&, const juce::String& text,
                             juce::Rectangle<int> area, juce::Colour);

    // MiniFreak-style section header: SMALL CAPS title, thin rule to the
    // right, optional right-aligned readout. Returns the content area below.
    // recess: draw the rule plus the eased inner shadow rising from it (the
    // same recessed-channel look a tab strip casts). false omits both --
    // title (and readout) only -- for headers that sit directly beneath a
    // tab strip already carrying that same rule + recess (FilterPanel inside
    // filterTabs, FXPanel inside fxTabs), so the module doesn't show two
    // stacked rule/recessed tiers.
    juce::Rectangle<int> sectionHeader (juce::Graphics&, juce::Rectangle<int> bounds,
                                        const juce::String& title,
                                        const juce::String& readout = {},
                                        juce::Colour titleColour = {},
                                        bool recess = true);

    // Display well behind scopes/curves. centreLine draws a faint
    // zero/centre reference line (useful for bipolar scopes, pointless for
    // plain list/text containers that happen to reuse this helper).
    void displayWell (juce::Graphics&, juce::Rectangle<float>, bool centreLine = true);

    // Curve stroke with a soft under-glow, the reference look for scopes.
    void glowStroke (juce::Graphics&, const juce::Path&, juce::Colour, float thickness = 1.8f);

    // Eased cast-shadow gradient shared by every recessed edge on the
    // faceplate (ContentComponent::paint's row-overhang shadows,
    // SPASynthLookAndFeel::drawTabAreaBehindFrontButton's tab-strip recess):
    // darkest (alpha = startAlpha) at `from`, fading through three eased
    // stops -- steep near the edge, long soft tail -- to fully transparent
    // at `to`, rather than a flat linear ramp, so the band never reads as a
    // visible strip. Pass `from`/`to` flipped to fade the shadow in the
    // other direction (e.g. upward into a recess instead of downward off an
    // overhang). Both call sites must use the same startAlpha so the two
    // shadow languages can never drift apart again.
    juce::ColourGradient easedShadowGradient (juce::Point<float> from, juce::Point<float> to,
                                              float startAlpha);

    // Single tunable for both shadow-language call sites (ContentComponent::
    // paint's row-overhang shadows, SPASynthLookAndFeel::
    // drawTabAreaBehindFrontButton's tab-strip recess). Was 0.42f; Mike
    // called both a little dark, lightened to 0.30f (2026-08-31).
    constexpr float shadowStartAlpha = 0.30f;

    // Module header title colour, for any module with an on/off toggle: the
    // muted white already used for the "LOCKS" caption above the randomizer
    // lock buttons (ContentComponent::paint -- t.textSecondary) when the
    // module is off, the user's accent colour when it's on. One rule in one
    // place rather than a ternary repeated at every draw::sectionHeader call
    // site (oscillator strips, filters, chaos, arp, FX panels). Modules with
    // no toggle (mod matrix, envelopes, LFOs) keep calling sectionHeader with
    // currentTheme().accent directly -- they're always live, so this helper
    // doesn't apply to them.
    inline juce::Colour moduleHeaderColour (const Theme& t, bool poweredOn)
    {
        return poweredOn ? t.accent : t.textSecondary;
    }
}

} // namespace spa::ui
