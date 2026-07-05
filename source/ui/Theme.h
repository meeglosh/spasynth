#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace arsenal::ui
{

// Runtime design tokens. Every colour, font and metric the UI uses comes
// through here so the whole look can be restyled (or switched between the
// dark and light themes) without touching component code.
//
// Palette direction: mid-century modern — warm neutrals, burnt orange
// accent, sage secondary.
struct Theme
{
    bool isDark = true;

    juce::Colour background;
    juce::Colour surface;        // panels
    juce::Colour surfaceRaised;  // controls, knob bodies
    juce::Colour header;         // top bar
    juce::Colour textPrimary;
    juce::Colour textSecondary;
    juce::Colour accent;         // burnt orange — value arcs, primary actions
    juce::Colour accentSecondary;// sage — toggles, locks
    juce::Colour outline;
    juce::Colour knobTrack;

    static Theme dark()
    {
        Theme t;
        t.isDark          = true;
        t.background      = juce::Colour (0xff1d1a17);
        t.surface         = juce::Colour (0xff262220);
        t.surfaceRaised   = juce::Colour (0xff322d28);
        t.header          = juce::Colour (0xff211d1a);
        t.textPrimary     = juce::Colour (0xfff2ece2);
        t.textSecondary   = juce::Colour (0xffa89d8d);
        t.accent          = juce::Colour (0xffe08e45);
        t.accentSecondary = juce::Colour (0xff8faf8b);
        t.outline         = juce::Colour (0xff3c362f);
        t.knobTrack       = juce::Colour (0xff45403a);
        return t;
    }

    static Theme light()
    {
        Theme t;
        t.isDark          = false;
        t.background      = juce::Colour (0xfff4efe7);
        t.surface         = juce::Colour (0xfffbf8f2);
        t.surfaceRaised   = juce::Colour (0xffeee7da);
        t.header          = juce::Colour (0xffefe9df);
        t.textPrimary     = juce::Colour (0xff2b2723);
        t.textSecondary   = juce::Colour (0xff6b6155);
        t.accent          = juce::Colour (0xffd97b2f);
        t.accentSecondary = juce::Colour (0xff6e9a6a);
        t.outline         = juce::Colour (0xffd8cfc0);
        t.knobTrack       = juce::Colour (0xffddd4c5);
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
    inline constexpr int baseWidth = 1120;
    inline constexpr int baseHeight = 760;
    inline constexpr int headerHeight = 64;
    inline constexpr int lockRowHeight = 30;
    inline constexpr int unit = 8;
    inline constexpr float cornerRadius = 6.0f;

    inline juce::Font titleFont()   { return juce::Font (juce::FontOptions (22.0f, juce::Font::bold)); }
    inline juce::Font sectionFont() { return juce::Font (juce::FontOptions (13.0f, juce::Font::bold)); }
    inline juce::Font labelFont()   { return juce::Font (juce::FontOptions (11.5f)); }
    inline juce::Font smallFont()   { return juce::Font (juce::FontOptions (10.0f)); }
}

} // namespace arsenal::ui
