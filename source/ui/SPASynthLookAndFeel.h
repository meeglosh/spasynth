#pragma once

#include "Theme.h"

namespace spa::ui
{

// Bespoke control rendering: minimal arc knobs, pill toggles, soft rounded
// surfaces. All colours come from the active Theme so light/dark switching
// is a palette refresh.
class SPASynthLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SPASynthLookAndFeel();

    // Re-reads the active theme into the LookAndFeel colour palette.
    void refreshPalette();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;

    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&,
                               const juce::Colour& backgroundColour,
                               bool highlighted, bool down) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool highlighted, bool down) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox&) override;

    juce::Font getComboBoxFont (juce::ComboBox&) override { return metrics::labelFont(); }
    juce::Font getPopupMenuFont() override { return metrics::labelFont(); }
    juce::Font getTextButtonFont (juce::TextButton& b, int) override
    {
        // Filter chips (preset browser) are tighter than regular buttons.
        return b.getComponentID() == "chip" ? metrics::smallFont() : metrics::labelFont();
    }
    juce::Font getLabelFont (juce::Label&) override { return metrics::labelFont(); }

    int getTabButtonBestWidth (juce::TabBarButton&, int tabDepth) override;
    void drawTabButton (juce::TabBarButton&, juce::Graphics&,
                        bool isMouseOver, bool isMouseDown) override;
    void drawTabbedButtonBarBackground (juce::TabbedButtonBar&, juce::Graphics&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SPASynthLookAndFeel)
};

} // namespace spa::ui
