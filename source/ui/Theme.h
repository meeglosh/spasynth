#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace spa::ui
{

// Runtime design tokens. Every colour, font and metric the UI uses comes
// through here so the whole look can be restyled without touching component
// code.
//
// Direction (per reference set: Serum 2 / Massive X / Pigments / MiniFreak):
// flat graphite surfaces, hairline rules, display-first modules, thin-ring
// knobs. The two accents default to orange = audio/energy and cool cyan =
// modulation, and are user-tintable via the header colour picker.
struct Theme
{
    juce::Colour background;      // window
    juce::Colour panel;           // module panels
    juce::Colour display;         // scope/curve display wells
    juce::Colour header;          // top bar
    juce::Colour textPrimary;
    juce::Colour textSecondary;
    juce::Colour accent;          // orange — audio signal, primary actions
    juce::Colour accentMod;       // cyan — modulation (LFO/env/matrix/chaos)
    juce::Colour outline;         // hairlines
    juce::Colour knobFace;
    juce::Colour knobTrack;

    static Theme dark()
    {
        // Deep blue-teal register per the redesign mock.
        Theme t;
        t.background    = juce::Colour (0xff0c1114);
        t.panel         = juce::Colour (0xff151c21);
        t.display       = juce::Colour (0xff0a0f13);
        t.header        = juce::Colour (0xff090d10);
        t.textPrimary   = juce::Colour (0xffe7ecef);
        t.textSecondary = juce::Colour (0xff7f8d97);
        t.accent        = juce::Colour (0xfff08b3a);
        t.accentMod     = juce::Colour (0xff4fc4d6);
        t.outline       = juce::Colour (0xff222d35);
        t.knobFace      = juce::Colour (0xff1e262d);
        t.knobTrack     = juce::Colour (0xff2c3841);
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

namespace metrics
{
    inline constexpr int baseWidth = 1380;
    inline constexpr int baseHeight = 900;
    inline constexpr int brandBandHeight = 34;   // centred wordmark strip
    inline constexpr int headerHeight = 54;
    inline constexpr int lockRowHeight = 26;
    inline constexpr int footerHeight = 24;
    inline constexpr int unit = 8;
    inline constexpr float cornerRadius = 7.0f;  // softer, elevated panels

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

    // MiniFreak-style section header: SMALL CAPS title, thin rule to the
    // right, optional right-aligned readout. Returns the content area below.
    juce::Rectangle<int> sectionHeader (juce::Graphics&, juce::Rectangle<int> bounds,
                                        const juce::String& title,
                                        const juce::String& readout = {},
                                        juce::Colour titleColour = {});

    // Display well behind scopes/curves.
    void displayWell (juce::Graphics&, juce::Rectangle<float>);

    // Curve stroke with a soft under-glow, the reference look for scopes.
    void glowStroke (juce::Graphics&, const juce::Path&, juce::Colour, float thickness = 1.8f);
}

} // namespace spa::ui
