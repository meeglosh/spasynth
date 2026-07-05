#include "ArsenalLookAndFeel.h"
#include "../library/Library.h"

namespace arsenal::ui
{

namespace
{
    std::atomic<bool> darkThemeActive { true };
    bool themeInitialised = false;
}

const Theme& currentTheme()
{
    static const Theme darkTheme = Theme::dark();
    static const Theme lightTheme = Theme::light();

    if (! themeInitialised)
    {
        darkThemeActive.store (library::getDarkThemeEnabled());
        themeInitialised = true;
    }

    return darkThemeActive.load() ? darkTheme : lightTheme;
}

void setDarkTheme (bool dark)
{
    darkThemeActive.store (dark);
    themeInitialised = true;
    library::setDarkThemeEnabled (dark);
}

ArsenalLookAndFeel::ArsenalLookAndFeel()
{
    refreshPalette();
}

void ArsenalLookAndFeel::refreshPalette()
{
    const auto& t = currentTheme();

    setColour (juce::ResizableWindow::backgroundColourId, t.background);
    setColour (juce::Label::textColourId, t.textSecondary);
    setColour (juce::TextButton::buttonColourId, t.surfaceRaised);
    setColour (juce::TextButton::buttonOnColourId, t.accentSecondary);
    setColour (juce::TextButton::textColourOffId, t.textPrimary);
    setColour (juce::TextButton::textColourOnId, t.isDark ? t.background : t.textPrimary);
    setColour (juce::ComboBox::backgroundColourId, t.surfaceRaised);
    setColour (juce::ComboBox::textColourId, t.textPrimary);
    setColour (juce::ComboBox::outlineColourId, t.outline);
    setColour (juce::ComboBox::arrowColourId, t.textSecondary);
    setColour (juce::PopupMenu::backgroundColourId, t.surface);
    setColour (juce::PopupMenu::textColourId, t.textPrimary);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, t.accent.withAlpha (0.35f));
    setColour (juce::PopupMenu::highlightedTextColourId, t.textPrimary);
    setColour (juce::Slider::textBoxTextColourId, t.textPrimary);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::thumbColourId, t.accent);
    setColour (juce::Slider::trackColourId, t.knobTrack);
    setColour (juce::Slider::backgroundColourId, t.knobTrack);
    setColour (juce::ScrollBar::thumbColourId, t.outline);
    setColour (juce::TooltipWindow::backgroundColourId, t.surfaceRaised);
    setColour (juce::TooltipWindow::textColourId, t.textPrimary);
    setColour (juce::BubbleComponent::backgroundColourId, t.surfaceRaised);
    setColour (juce::TabbedButtonBar::tabTextColourId, t.textSecondary);
    setColour (juce::TabbedButtonBar::frontTextColourId, t.textPrimary);
    setColour (juce::AlertWindow::backgroundColourId, t.surface);
    setColour (juce::AlertWindow::textColourId, t.textPrimary);
}

void ArsenalLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                           float sliderPos, float rotaryStartAngle,
                                           float rotaryEndAngle, juce::Slider& slider)
{
    const auto& t = currentTheme();
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (3.0f);
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    const auto arcRadius = radius - 2.5f;
    const auto lineW = juce::jmax (2.0f, radius * 0.14f);

    // Body.
    g.setColour (t.surfaceRaised.brighter (slider.isMouseOverOrDragging() ? 0.06f : 0.0f));
    g.fillEllipse (centre.x - radius * 0.72f, centre.y - radius * 0.72f,
                   radius * 1.44f, radius * 1.44f);

    // Track arc.
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                         rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (t.knobTrack);
    g.strokePath (track, juce::PathStrokeType (lineW, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    // Value arc. Bipolar params (range crossing zero) fill from centre.
    const auto isBipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
    const auto fillStart = isBipolar
                         ? rotaryStartAngle + 0.5f * (rotaryEndAngle - rotaryStartAngle)
                         : rotaryStartAngle;
    juce::Path value;
    value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                         fillStart, angle, true);
    g.setColour (slider.isEnabled() ? t.accent : t.textSecondary.withAlpha (0.4f));
    g.strokePath (value, juce::PathStrokeType (lineW, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    // Pointer.
    const auto pointer = centre.getPointOnCircumference (radius * 0.55f, angle);
    g.setColour (t.textPrimary);
    g.drawLine (centre.x, centre.y, pointer.x, pointer.y, lineW * 0.8f);
}

void ArsenalLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                           float sliderPos, float, float,
                                           juce::Slider::SliderStyle style, juce::Slider& slider)
{
    const auto& t = currentTheme();

    if (style == juce::Slider::LinearHorizontal || style == juce::Slider::LinearBar)
    {
        const auto track = juce::Rectangle<float> ((float) x, (float) y + (float) height * 0.5f - 2.0f,
                                                   (float) width, 4.0f);
        g.setColour (t.knobTrack);
        g.fillRoundedRectangle (track, 2.0f);

        const auto isBipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
        const auto zeroX = isBipolar ? (float) x + (float) width * 0.5f : (float) x;
        g.setColour (t.accent);
        g.fillRoundedRectangle (juce::Rectangle<float> (juce::jmin (zeroX, sliderPos),
                                                        track.getY(),
                                                        std::abs (sliderPos - zeroX), 4.0f),
                                2.0f);

        g.setColour (t.textPrimary);
        g.fillEllipse (sliderPos - 5.0f, (float) y + (float) height * 0.5f - 5.0f, 10.0f, 10.0f);
        return;
    }

    LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, 0, 0, style, slider);
}

void ArsenalLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                               const juce::Colour& backgroundColour,
                                               bool highlighted, bool down)
{
    const auto& t = currentTheme();
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);

    auto colour = backgroundColour;
    if (button.getToggleState())
        colour = findColour (juce::TextButton::buttonOnColourId);
    if (down)
        colour = colour.darker (0.2f);
    else if (highlighted)
        colour = colour.brighter (t.isDark ? 0.08f : 0.04f);

    g.setColour (colour);
    g.fillRoundedRectangle (bounds, metrics::cornerRadius);
    g.setColour (t.outline);
    g.drawRoundedRectangle (bounds, metrics::cornerRadius, 1.0f);
}

void ArsenalLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                           bool highlighted, bool)
{
    const auto& t = currentTheme();
    const auto bounds = button.getLocalBounds().toFloat();
    const auto pillH = juce::jmin (16.0f, bounds.getHeight() - 4.0f);
    const auto pillW = pillH * 1.8f;
    const auto pill = juce::Rectangle<float> (bounds.getX(),
                                              bounds.getCentreY() - pillH * 0.5f,
                                              pillW, pillH);

    g.setColour (button.getToggleState() ? t.accentSecondary
                                         : t.knobTrack.brighter (highlighted ? 0.1f : 0.0f));
    g.fillRoundedRectangle (pill, pillH * 0.5f);

    const auto knobX = button.getToggleState() ? pill.getRight() - pillH + 2.0f
                                               : pill.getX() + 2.0f;
    g.setColour (t.textPrimary);
    g.fillEllipse (knobX, pill.getY() + 2.0f, pillH - 4.0f, pillH - 4.0f);

    g.setColour (t.textSecondary);
    g.setFont (metrics::labelFont());
    g.drawText (button.getButtonText(),
                bounds.withTrimmedLeft (pillW + 6.0f).toNearestInt(),
                juce::Justification::centredLeft);
}

void ArsenalLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                       int, int, int, int, juce::ComboBox&)
{
    const auto& t = currentTheme();
    const auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height)
                            .reduced (0.5f);

    g.setColour (findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (bounds, metrics::cornerRadius);
    g.setColour (t.outline);
    g.drawRoundedRectangle (bounds, metrics::cornerRadius, 1.0f);

    juce::Path chevron;
    const auto cx = (float) width - 12.0f;
    const auto cy = (float) height * 0.5f;
    chevron.startNewSubPath (cx - 4.0f, cy - 2.0f);
    chevron.lineTo (cx, cy + 2.5f);
    chevron.lineTo (cx + 4.0f, cy - 2.0f);
    g.setColour (t.textSecondary);
    g.strokePath (chevron, juce::PathStrokeType (1.6f));
}

int ArsenalLookAndFeel::getTabButtonBestWidth (juce::TabBarButton& button, int)
{
    juce::GlyphArrangement glyphs;
    glyphs.addLineOfText (metrics::labelFont(), button.getButtonText(), 0.0f, 0.0f);
    return juce::jmax (44, (int) std::ceil (glyphs.getBoundingBox (0, -1, true).getWidth()) + 20);
}

void ArsenalLookAndFeel::drawTabButton (juce::TabBarButton& button, juce::Graphics& g,
                                        bool isMouseOver, bool)
{
    const auto& t = currentTheme();
    auto bounds = button.getLocalBounds().toFloat().reduced (2.0f, 3.0f);
    const auto front = button.isFrontTab();

    if (front || isMouseOver)
    {
        g.setColour (front ? t.surfaceRaised : t.surfaceRaised.withAlpha (0.5f));
        g.fillRoundedRectangle (bounds, metrics::cornerRadius);
    }
    if (front)
    {
        g.setColour (t.accent);
        g.fillRoundedRectangle (bounds.removeFromBottom (2.0f).reduced (6.0f, 0.0f), 1.0f);
    }

    g.setColour (front ? t.textPrimary : t.textSecondary);
    g.setFont (metrics::labelFont());
    g.drawText (button.getButtonText(), button.getLocalBounds(), juce::Justification::centred);
}

void ArsenalLookAndFeel::drawTabbedButtonBarBackground (juce::TabbedButtonBar&, juce::Graphics&)
{
}

} // namespace arsenal::ui
