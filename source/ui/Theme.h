#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace arsenal::ui
{

// Runtime design tokens. Every colour, font and metric the UI uses comes
// through here so the whole look can be restyled (or switched between the
// dark and light themes) without touching component code.
//
// Direction (per reference set: Serum 2 / Massive X / Pigments / MiniFreak):
// flat graphite surfaces, hairline rules, display-first modules, thin-ring
// knobs. Orange = audio/energy, cool cyan = modulation.
struct Theme
{
    bool isDark = true;

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
        t.isDark        = true;
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

    // Light theme in the Massive X register: warm light greys, dark text,
    // the same orange/cyan semantics.
    static Theme light()
    {
        Theme t;
        t.isDark        = false;
        t.background    = juce::Colour (0xffd6d4d1);
        t.panel         = juce::Colour (0xffe4e2df);
        t.display       = juce::Colour (0xfff2f1ef);
        t.header        = juce::Colour (0xff2a2c2e);   // dark header like Massive X
        t.textPrimary   = juce::Colour (0xff232527);
        t.textSecondary = juce::Colour (0xff6d6f72);
        t.accent        = juce::Colour (0xffe1701d);
        t.accentMod     = juce::Colour (0xff2f8fa8);
        t.outline       = juce::Colour (0xffbcbab6);
        t.knobFace      = juce::Colour (0xfff0efed);
        t.knobTrack     = juce::Colour (0xffc4c2be);
        return t;
    }
};

// Process-wide active theme (a machine preference, shared by all editor
// instances). Components read colours at paint time, so switching only
// requires a repaint + LookAndFeel palette refresh.
const Theme& currentTheme();
void setDarkTheme (bool dark);

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

} // namespace arsenal::ui
