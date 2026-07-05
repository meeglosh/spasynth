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

namespace draw
{

void panel (juce::Graphics& g, juce::Rectangle<float> bounds)
{
    const auto& t = currentTheme();
    g.setColour (t.panel);
    g.fillRoundedRectangle (bounds, metrics::cornerRadius);
    g.setColour (t.outline);
    g.drawRoundedRectangle (bounds.reduced (0.5f), metrics::cornerRadius, 1.0f);
}

juce::Rectangle<int> sectionHeader (juce::Graphics& g, juce::Rectangle<int> bounds,
                                    const juce::String& title, const juce::String& readout,
                                    juce::Colour titleColour)
{
    const auto& t = currentTheme();
    auto header = bounds.removeFromTop (20);
    auto text = header.reduced (8, 0);

    if (titleColour == juce::Colour())
        titleColour = t.textPrimary;

    g.setFont (metrics::sectionFont());

    const auto titleText = title.toUpperCase();
    juce::GlyphArrangement glyphs;
    glyphs.addLineOfText (metrics::sectionFont(), titleText, 0.0f, 0.0f);
    const auto titleWidth = (int) std::ceil (glyphs.getBoundingBox (0, -1, true).getWidth());

    g.setColour (titleColour);
    g.drawText (titleText, text, juce::Justification::centredLeft);

    int readoutWidth = 0;
    if (readout.isNotEmpty())
    {
        g.setColour (t.textSecondary);
        g.setFont (metrics::labelFont());
        g.drawText (readout, text, juce::Justification::centredRight);

        juce::GlyphArrangement readoutGlyphs;
        readoutGlyphs.addLineOfText (metrics::labelFont(), readout, 0.0f, 0.0f);
        readoutWidth = 8 + (int) std::ceil (readoutGlyphs.getBoundingBox (0, -1, true).getWidth());
    }

    // Thin rule between title and readout.
    const auto ruleY = (float) header.getCentreY();
    g.setColour (t.outline);
    g.drawLine ((float) (text.getX() + titleWidth + 8), ruleY,
                (float) (text.getRight() - readoutWidth), ruleY, 1.0f);

    return bounds;
}

void displayWell (juce::Graphics& g, juce::Rectangle<float> bounds)
{
    const auto& t = currentTheme();
    g.setColour (t.display);
    g.fillRoundedRectangle (bounds, 2.0f);
    g.setColour (t.outline);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 2.0f, 1.0f);

    // Faint centre line, like the reference scopes.
    g.setColour (t.outline.withAlpha (0.5f));
    g.drawHorizontalLine ((int) bounds.getCentreY(), bounds.getX() + 2.0f,
                          bounds.getRight() - 2.0f);
}

void glowStroke (juce::Graphics& g, const juce::Path& path, juce::Colour colour, float thickness)
{
    g.setColour (colour.withAlpha (0.22f));
    g.strokePath (path, juce::PathStrokeType (thickness * 3.0f,
                                              juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));
    g.setColour (colour);
    g.strokePath (path, juce::PathStrokeType (thickness,
                                              juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));
}

} // namespace draw

ArsenalLookAndFeel::ArsenalLookAndFeel()
{
    refreshPalette();
}

void ArsenalLookAndFeel::refreshPalette()
{
    const auto& t = currentTheme();

    setColour (juce::ResizableWindow::backgroundColourId, t.background);
    setColour (juce::Label::textColourId, t.textSecondary);
    setColour (juce::TextButton::buttonColourId, t.knobFace);
    setColour (juce::TextButton::buttonOnColourId, t.accentMod.withAlpha (0.85f));
    setColour (juce::TextButton::textColourOffId, t.textPrimary);
    setColour (juce::TextButton::textColourOnId, t.isDark ? t.display : t.textPrimary);
    setColour (juce::ComboBox::backgroundColourId, t.display);
    setColour (juce::ComboBox::textColourId, t.textPrimary);
    setColour (juce::ComboBox::outlineColourId, t.outline);
    setColour (juce::ComboBox::arrowColourId, t.textSecondary);
    setColour (juce::PopupMenu::backgroundColourId, t.panel);
    setColour (juce::PopupMenu::textColourId, t.textPrimary);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, t.accent.withAlpha (0.30f));
    setColour (juce::PopupMenu::highlightedTextColourId, t.textPrimary);
    setColour (juce::Slider::textBoxTextColourId, t.textPrimary);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::thumbColourId, t.accent);
    setColour (juce::Slider::trackColourId, t.knobTrack);
    setColour (juce::Slider::backgroundColourId, t.knobTrack);
    setColour (juce::ScrollBar::thumbColourId, t.knobTrack);
    setColour (juce::TooltipWindow::backgroundColourId, t.panel);
    setColour (juce::TooltipWindow::textColourId, t.textPrimary);
    setColour (juce::TooltipWindow::outlineColourId, t.outline);
    setColour (juce::BubbleComponent::backgroundColourId, t.panel);
    setColour (juce::TabbedButtonBar::tabTextColourId, t.textSecondary);
    setColour (juce::TabbedButtonBar::frontTextColourId, t.textPrimary);
    setColour (juce::AlertWindow::backgroundColourId, t.panel);
    setColour (juce::AlertWindow::textColourId, t.textPrimary);
}

void ArsenalLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                           float sliderPos, float rotaryStartAngle,
                                           float rotaryEndAngle, juce::Slider& slider)
{
    const auto& t = currentTheme();
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (2.0f);
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    const auto lineW = juce::jlimit (1.6f, 2.4f, radius * 0.11f);
    const auto arcRadius = radius - lineW * 0.5f;

    // Face — a flat disc, reference style.
    g.setColour (t.knobFace.brighter (slider.isMouseOverOrDragging() ? 0.05f : 0.0f));
    g.fillEllipse (centre.x - arcRadius + lineW, centre.y - arcRadius + lineW,
                   (arcRadius - lineW) * 2.0f, (arcRadius - lineW) * 2.0f);

    // Thin ring track.
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                         rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (t.knobTrack);
    g.strokePath (track, juce::PathStrokeType (lineW, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    // Value arc. Modulation-coloured knobs opt in via component ID.
    const auto accent = slider.getComponentID() == "mod" ? t.accentMod : t.accent;
    const auto isBipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
    const auto fillStart = isBipolar
                         ? rotaryStartAngle + 0.5f * (rotaryEndAngle - rotaryStartAngle)
                         : rotaryStartAngle;
    juce::Path value;
    value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                         fillStart, angle, true);
    g.setColour (slider.isEnabled() ? accent : t.textSecondary.withAlpha (0.35f));
    g.strokePath (value, juce::PathStrokeType (lineW, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    // Pointer.
    const auto tip = centre.getPointOnCircumference (arcRadius - lineW * 1.6f, angle);
    const auto inner = centre.getPointOnCircumference (arcRadius * 0.45f, angle);
    g.setColour (t.textPrimary);
    g.drawLine (inner.x, inner.y, tip.x, tip.y, lineW);
}

void ArsenalLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                           float sliderPos, float, float,
                                           juce::Slider::SliderStyle style, juce::Slider& slider)
{
    const auto& t = currentTheme();

    if (style == juce::Slider::LinearHorizontal || style == juce::Slider::LinearBar)
    {
        const auto track = juce::Rectangle<float> ((float) x, (float) y + (float) height * 0.5f - 1.5f,
                                                   (float) width, 3.0f);
        g.setColour (t.knobTrack);
        g.fillRoundedRectangle (track, 1.5f);

        const auto accent = slider.getComponentID() == "mod" ? t.accentMod : t.accent;
        const auto isBipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
        const auto zeroX = isBipolar ? (float) x + (float) width * 0.5f : (float) x;
        g.setColour (accent);
        g.fillRoundedRectangle (juce::Rectangle<float> (juce::jmin (zeroX, sliderPos),
                                                        track.getY(),
                                                        std::abs (sliderPos - zeroX), 3.0f),
                                1.5f);

        g.setColour (t.textPrimary);
        g.fillEllipse (sliderPos - 4.0f, (float) y + (float) height * 0.5f - 4.0f, 8.0f, 8.0f);
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
        colour = colour.darker (0.15f);
    else if (highlighted)
        colour = colour.brighter (t.isDark ? 0.07f : 0.04f);

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
    const auto pillH = juce::jmin (14.0f, bounds.getHeight() - 4.0f);
    const auto pillW = pillH * 1.9f;
    const auto pill = juce::Rectangle<float> (bounds.getX(),
                                              bounds.getCentreY() - pillH * 0.5f,
                                              pillW, pillH);

    g.setColour (button.getToggleState() ? t.accentMod
                                         : t.knobTrack.brighter (highlighted ? 0.08f : 0.0f));
    g.fillRoundedRectangle (pill, pillH * 0.5f);

    const auto knobX = button.getToggleState() ? pill.getRight() - pillH + 2.0f
                                               : pill.getX() + 2.0f;
    g.setColour (t.isDark ? t.textPrimary : juce::Colours::white);
    g.fillEllipse (knobX, pill.getY() + 2.0f, pillH - 4.0f, pillH - 4.0f);

    g.setColour (t.textSecondary);
    g.setFont (metrics::smallFont());
    g.drawText (button.getButtonText().toUpperCase(),
                bounds.withTrimmedLeft (pillW + 5.0f).toNearestInt(),
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
    const auto cx = (float) width - 11.0f;
    const auto cy = (float) height * 0.5f;
    chevron.startNewSubPath (cx - 3.5f, cy - 1.8f);
    chevron.lineTo (cx, cy + 2.2f);
    chevron.lineTo (cx + 3.5f, cy - 1.8f);
    g.setColour (t.textSecondary);
    g.strokePath (chevron, juce::PathStrokeType (1.4f));
}

int ArsenalLookAndFeel::getTabButtonBestWidth (juce::TabBarButton& button, int)
{
    juce::GlyphArrangement glyphs;
    glyphs.addLineOfText (metrics::smallFont(), button.getButtonText(), 0.0f, 0.0f);
    return juce::jmax (36, (int) std::ceil (glyphs.getBoundingBox (0, -1, true).getWidth()) + 16);
}

void ArsenalLookAndFeel::drawTabButton (juce::TabBarButton& button, juce::Graphics& g,
                                        bool isMouseOver, bool)
{
    const auto& t = currentTheme();
    const auto bounds = button.getLocalBounds().toFloat().reduced (1.0f, 2.0f);
    const auto front = button.isFrontTab();

    if (front)
    {
        g.setColour (t.display);
        g.fillRoundedRectangle (bounds, 2.0f);
        auto underline = bounds;
        g.setColour (t.accent);
        g.fillRect (underline.removeFromBottom (2.0f).reduced (4.0f, 0.0f));
    }
    else if (isMouseOver)
    {
        g.setColour (t.display.withAlpha (0.5f));
        g.fillRoundedRectangle (bounds, 2.0f);
    }

    g.setColour (front ? t.textPrimary : t.textSecondary);
    g.setFont (metrics::smallFont());
    g.drawText (button.getButtonText().toUpperCase(), button.getLocalBounds(),
                juce::Justification::centred);
}

void ArsenalLookAndFeel::drawTabbedButtonBarBackground (juce::TabbedButtonBar&, juce::Graphics&)
{
}

} // namespace arsenal::ui
