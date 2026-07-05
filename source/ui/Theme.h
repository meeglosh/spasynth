#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace arsenal::ui
{

// Design tokens. Every colour, font and spacing value the UI uses lives here
// so the whole look can be restyled without touching component code.
// Current values are a placeholder warm/mid-century-modern direction; final
// palette lands in the theming pass (checkpoint 9).
namespace theme
{
    // Palette
    inline const juce::Colour background      { 0xff2b2723 };  // warm charcoal
    inline const juce::Colour surface         { 0xff37322c };
    inline const juce::Colour surfaceRaised   { 0xff443e36 };
    inline const juce::Colour textPrimary     { 0xfff2ece2 };  // warm off-white
    inline const juce::Colour textSecondary   { 0xffb3a99a };
    inline const juce::Colour accent          { 0xffe08e45 };  // burnt orange
    inline const juce::Colour accentSecondary { 0xff7da87b };  // sage
    inline const juce::Colour outline         { 0xff554e44 };

    // Spacing / metrics
    inline constexpr int unit = 8;
    inline constexpr int topBarHeight = 64;
    inline constexpr int slotStripHeight = 64;  // two rows: wavetable + SFX
    inline constexpr int lockRowHeight = 30;    // RANDOMIZE ALL lock groups
    inline constexpr float cornerRadius = 6.0f;

    // Typography
    inline juce::Font titleFont()  { return juce::Font (juce::FontOptions (20.0f, juce::Font::bold)); }
    inline juce::Font labelFont()  { return juce::Font (juce::FontOptions (13.0f)); }
}

} // namespace arsenal::ui
