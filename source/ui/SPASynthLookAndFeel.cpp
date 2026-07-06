#include "SPASynthLookAndFeel.h"
#include "../library/Library.h"

namespace spa::ui
{

namespace
{
    // The one theme instance (message thread only). Accents load from the
    // user's saved preference on first access.
    Theme& mutableTheme()
    {
        static Theme theme = []
        {
            auto t = Theme::dark();
            t.accent = library::getAccentColor (t.accent);
            t.accentMod = library::getAccentModColor (t.accentMod);
            return t;
        }();
        return theme;
    }
}

const Theme& currentTheme()
{
    return mutableTheme();
}

void setAccentColors (juce::Colour audio, juce::Colour mod)
{
    mutableTheme().accent = audio;
    mutableTheme().accentMod = mod;
    library::setAccentColors (audio, mod);
}

void resetAccentColors()
{
    const auto defaults = Theme::dark();
    setAccentColors (defaults.accent, defaults.accentMod);
}

namespace draw
{

void panel (juce::Graphics& g, juce::Rectangle<float> bounds)
{
    const auto& t = currentTheme();

    // Soft elevation shadow (two feathered passes — cheap and convincing).
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (bounds.translated (0.0f, 2.5f).expanded (1.0f),
                            metrics::cornerRadius + 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.20f));
    g.fillRoundedRectangle (bounds.translated (0.0f, 4.5f).expanded (2.5f),
                            metrics::cornerRadius + 4.0f);

    // Panel face with a whisper of vertical gradient.
    g.setGradientFill (juce::ColourGradient (t.panel.brighter (0.05f),
                                             bounds.getX(), bounds.getY(),
                                             t.panel.darker (0.06f),
                                             bounds.getX(), bounds.getBottom(), false));
    g.fillRoundedRectangle (bounds, metrics::cornerRadius);

    g.setColour (t.outline);
    g.drawRoundedRectangle (bounds.reduced (0.5f), metrics::cornerRadius, 1.0f);

    // Hairline top highlight — the "edge catch" the mock's panels have.
    g.setColour (juce::Colours::white.withAlpha (0.045f));
    g.drawLine (bounds.getX() + metrics::cornerRadius, bounds.getY() + 1.0f,
                bounds.getRight() - metrics::cornerRadius, bounds.getY() + 1.0f, 1.0f);
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
        const auto stringWidth = [] (const juce::String& s)
        {
            juce::GlyphArrangement glyphArrangement;
            glyphArrangement.addLineOfText (metrics::labelFont(), s, 0.0f, 0.0f);
            return (int) std::ceil (glyphArrangement.getBoundingBox (0, -1, true).getWidth());
        };

        // Never run under the title: fit into the space after title + a
        // minimum rule, ellipsizing the tail of long content names.
        const auto available = text.getWidth() - titleWidth - 8 - 14;
        auto fitted = readout;
        if (stringWidth (fitted) > available)
        {
            while (fitted.length() > 4 && stringWidth (fitted + "...") > available)
                fitted = fitted.dropLastCharacters (1).trimEnd();
            fitted += "...";
        }

        if (available > 20)
        {
            g.setColour (t.textSecondary);
            g.setFont (metrics::labelFont());
            g.drawText (fitted, text, juce::Justification::centredRight);
            readoutWidth = 8 + stringWidth (fitted);
        }
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

    // Recessed well: vertical gradient, slightly darker at the top.
    g.setGradientFill (juce::ColourGradient (t.display.darker (0.25f),
                                             bounds.getX(), bounds.getY(),
                                             t.display.brighter (0.08f),
                                             bounds.getX(), bounds.getBottom(), false));
    g.fillRoundedRectangle (bounds, 3.0f);
    g.setColour (t.outline);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 3.0f, 1.0f);

    // Faint centre line, like the reference scopes.
    g.setColour (t.outline.withAlpha (0.45f));
    g.drawHorizontalLine ((int) bounds.getCentreY(), bounds.getX() + 2.0f,
                          bounds.getRight() - 2.0f);
}

void glowStroke (juce::Graphics& g, const juce::Path& path, juce::Colour colour, float thickness)
{
    // Three feathered passes for the mock's neon glow.
    g.setColour (colour.withAlpha (0.10f));
    g.strokePath (path, juce::PathStrokeType (thickness * 6.0f,
                                              juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));
    g.setColour (colour.withAlpha (0.28f));
    g.strokePath (path, juce::PathStrokeType (thickness * 2.6f,
                                              juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));
    g.setColour (colour.brighter (0.05f));
    g.strokePath (path, juce::PathStrokeType (thickness,
                                              juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));
}

} // namespace draw

SPASynthLookAndFeel::SPASynthLookAndFeel()
{
    refreshPalette();
}

void SPASynthLookAndFeel::refreshPalette()
{
    const auto& t = currentTheme();

    setColour (juce::ResizableWindow::backgroundColourId, t.background);
    setColour (juce::Label::textColourId, t.textSecondary);
    setColour (juce::TextButton::buttonColourId, t.knobFace);
    setColour (juce::TextButton::buttonOnColourId, t.accentMod.withAlpha (0.85f));
    setColour (juce::TextButton::textColourOffId, t.textPrimary);
    setColour (juce::TextButton::textColourOnId, t.display);
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

void SPASynthLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                           float sliderPos, float rotaryStartAngle,
                                           float rotaryEndAngle, juce::Slider& slider)
{
    const auto& t = currentTheme();
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (2.0f);
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    const auto lineW = juce::jlimit (1.7f, 2.6f, radius * 0.12f);
    const auto arcRadius = radius - lineW * 0.5f;
    const auto faceRadius = arcRadius - lineW * 1.7f;

    // Drop shadow under the disc (mock: knobs float above the panel).
    {
        const auto shadowR = faceRadius * 1.18f;
        juce::ColourGradient shadow (
            juce::Colours::black.withAlpha (0.55f),
            centre.x, centre.y + faceRadius * 0.28f,
            juce::Colours::transparentBlack,
            centre.x, centre.y + faceRadius * 0.28f + shadowR, true);
        g.setGradientFill (shadow);
        g.fillEllipse (centre.x - shadowR, centre.y + faceRadius * 0.28f - shadowR,
                       shadowR * 2.0f, shadowR * 2.0f);
    }

    // Face: vertical gradient disc, light catches the top.
    const auto hover = slider.isMouseOverOrDragging() ? 0.05f : 0.0f;
    g.setGradientFill (juce::ColourGradient (
        t.knobFace.brighter (0.55f + hover), centre.x, centre.y - faceRadius,
        t.knobFace.darker (0.45f), centre.x, centre.y + faceRadius, false));
    g.fillEllipse (centre.x - faceRadius, centre.y - faceRadius,
                   faceRadius * 2.0f, faceRadius * 2.0f);

    // Bevel: bright top rim, dark bottom rim.
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.drawEllipse (centre.x - faceRadius, centre.y - faceRadius,
                   faceRadius * 2.0f, faceRadius * 2.0f, 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawEllipse (centre.x - faceRadius + 0.5f, centre.y - faceRadius + 1.0f,
                   faceRadius * 2.0f - 1.0f, faceRadius * 2.0f - 1.0f, 0.8f);

    // Hover: a soft accent halo hugging the ring (stays inside the
    // component bounds — the outermost faint pass may kiss the edge).
    const auto accentPreview = slider.getComponentID() == "mod" ? t.accentMod : t.accent;
    if (slider.isMouseOverOrDragging() && slider.isEnabled())
    {
        constexpr float grows[] = { 0.5f, 1.8f, 3.2f };
        constexpr float alphas[] = { 0.20f, 0.12f, 0.06f };
        for (int pass = 0; pass < 3; ++pass)
        {
            g.setColour (accentPreview.withAlpha (alphas[pass]));
            g.drawEllipse (centre.x - arcRadius - grows[pass],
                           centre.y - arcRadius - grows[pass],
                           (arcRadius + grows[pass]) * 2.0f,
                           (arcRadius + grows[pass]) * 2.0f,
                           2.0f);
        }
    }

    // Ring track (groove outside the face).
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                         rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (t.knobTrack.withAlpha (0.9f));
    g.strokePath (track, juce::PathStrokeType (lineW, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    // Value arc with glow.
    const auto accent = slider.getComponentID() == "mod" ? t.accentMod : t.accent;
    const auto isBipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
    const auto fillStart = isBipolar
                         ? rotaryStartAngle + 0.5f * (rotaryEndAngle - rotaryStartAngle)
                         : rotaryStartAngle;
    juce::Path value;
    value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                         fillStart, angle, true);
    const auto arcColour = slider.isEnabled() ? accent : t.textSecondary.withAlpha (0.35f);
    if (slider.isEnabled())
    {
        g.setColour (arcColour.withAlpha (0.30f));
        g.strokePath (value, juce::PathStrokeType (lineW * 2.4f,
                                                   juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
    }
    g.setColour (arcColour);
    g.strokePath (value, juce::PathStrokeType (lineW, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    // Pointer: bright needle on the face.
    const auto tip = centre.getPointOnCircumference (faceRadius * 0.88f, angle);
    const auto inner = centre.getPointOnCircumference (faceRadius * 0.30f, angle);
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.drawLine (inner.x + 0.7f, inner.y + 1.0f, tip.x + 0.7f, tip.y + 1.0f, lineW);
    g.setColour (juce::Colours::white);
    g.drawLine (inner.x, inner.y, tip.x, tip.y, lineW * 0.9f);

    // Press state for label-less knobs that opt in: a value chip across the
    // knob (used by the header master volume).
    if (slider.isMouseButtonDown()
        && slider.getProperties().contains ("inlineValueSuffix"))
    {
        const auto magnitude = std::abs (slider.getValue());
        const auto text = juce::String (slider.getValue(),
                                        magnitude >= 1000.0 ? 0 : 1)
                        + slider.getProperties()["inlineValueSuffix"].toString();
        g.setFont (metrics::smallFont());

        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText (metrics::smallFont(), text, 0.0f, 0.0f);
        const auto w = glyphs.getBoundingBox (0, -1, true).getWidth() + 10.0f;
        const auto chip = juce::Rectangle<float> (w, 14.0f).withCentre (centre);

        g.setColour (t.display.withAlpha (0.92f));
        g.fillRoundedRectangle (chip, 3.0f);
        g.setColour (t.outline);
        g.drawRoundedRectangle (chip, 3.0f, 1.0f);
        g.setColour (t.textPrimary);
        g.drawText (text, chip.toNearestInt(), juce::Justification::centred);
    }
}

void SPASynthLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
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

        // Press state: numeric readout inside the slider, on whichever side
        // of the thumb has room.
        if (slider.isMouseButtonDown())
        {
            const auto text = juce::String (slider.getValue(), 2);
            g.setFont (metrics::smallFont());
            g.setColour (t.textPrimary);
            const auto onLeft = sliderPos > (float) x + (float) width * 0.5f;
            const auto textArea = onLeft
                ? juce::Rectangle<int> (x + 2, y, (int) (sliderPos - (float) x) - 10, height)
                : juce::Rectangle<int> ((int) sliderPos + 10, y,
                                        x + width - (int) sliderPos - 12, height);
            g.drawText (text, textArea,
                        onLeft ? juce::Justification::centredLeft
                               : juce::Justification::centredRight);
        }
        return;
    }

    LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, 0, 0, style, slider);
}

void SPASynthLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                               const juce::Colour& backgroundColour,
                                               bool highlighted, bool down)
{
    const auto& t = currentTheme();
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);

    // Primary action: glossy accent gradient + glow (RANDOMIZE ALL).
    if (button.getComponentID() == "primary")
    {
        g.setColour (t.accent.withAlpha (down ? 0.15f : 0.30f));
        g.fillRoundedRectangle (bounds.expanded (2.5f), metrics::cornerRadius + 2.0f);

        auto top = t.accent.brighter (highlighted ? 0.22f : 0.14f);
        auto bottom = t.accent.darker (down ? 0.35f : 0.18f);
        g.setGradientFill (juce::ColourGradient (top, bounds.getX(), bounds.getY(),
                                                 bottom, bounds.getX(), bounds.getBottom(),
                                                 false));
        g.fillRoundedRectangle (bounds, metrics::cornerRadius);

        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.drawLine (bounds.getX() + metrics::cornerRadius, bounds.getY() + 1.0f,
                    bounds.getRight() - metrics::cornerRadius, bounds.getY() + 1.0f, 1.0f);
        return;
    }

    auto colour = backgroundColour;
    if (button.getToggleState())
        colour = findColour (juce::TextButton::buttonOnColourId);
    if (down)
        colour = colour.darker (0.15f);
    else if (highlighted)
        colour = colour.brighter (0.07f);

    g.setColour (colour);
    g.fillRoundedRectangle (bounds, metrics::cornerRadius);
    g.setColour (t.outline);
    g.drawRoundedRectangle (bounds, metrics::cornerRadius, 1.0f);
}

void SPASynthLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
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
    g.setColour (t.textPrimary);
    g.fillEllipse (knobX, pill.getY() + 2.0f, pillH - 4.0f, pillH - 4.0f);

    g.setColour (t.textSecondary);
    g.setFont (metrics::smallFont());
    g.drawText (button.getButtonText().toUpperCase(),
                bounds.withTrimmedLeft (pillW + 5.0f).toNearestInt(),
                juce::Justification::centredLeft);
}

void SPASynthLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
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

int SPASynthLookAndFeel::getTabButtonBestWidth (juce::TabBarButton& button, int)
{
    juce::GlyphArrangement glyphs;
    glyphs.addLineOfText (metrics::smallFont(), button.getButtonText(), 0.0f, 0.0f);
    return juce::jmax (36, (int) std::ceil (glyphs.getBoundingBox (0, -1, true).getWidth()) + 16);
}

void SPASynthLookAndFeel::drawTabButton (juce::TabBarButton& button, juce::Graphics& g,
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

void SPASynthLookAndFeel::drawTabbedButtonBarBackground (juce::TabbedButtonBar&, juce::Graphics&)
{
}

} // namespace spa::ui
