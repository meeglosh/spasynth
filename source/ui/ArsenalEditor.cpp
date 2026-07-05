#include "ArsenalEditor.h"
#include "../ArsenalProcessor.h"
#include "Theme.h"

#include "BinaryData.h"

namespace arsenal
{

ArsenalEditor::ArsenalEditor (ArsenalProcessor& p)
    : juce::AudioProcessorEditor (p), processor (p), genericPanel (p)
{
    logo = juce::Drawable::createFromImageData (ArsenalAssets::SPAudio_logo_banner_white_svg,
                                                ArsenalAssets::SPAudio_logo_banner_white_svgSize);

    title.setText ("ARSENAL", juce::dontSendNotification);
    title.setFont (ui::theme::titleFont());
    title.setColour (juce::Label::textColourId, ui::theme::textPrimary);
    addAndMakeVisible (title);

    // Placeholder until checkpoint 7 — visible so the layout accounts for it.
    randomizeButton.setEnabled (false);
    randomizeButton.setColour (juce::TextButton::buttonColourId, ui::theme::accent);
    addAndMakeVisible (randomizeButton);

    masterSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    masterSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 16);
    masterAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.getAPVTS(), "global.masterGain", masterSlider);
    addAndMakeVisible (masterSlider);

    genericViewport.setViewedComponent (&genericPanel, false);
    genericViewport.setScrollBarsShown (true, false);
    addAndMakeVisible (genericViewport);

    setResizable (true, true);
    setResizeLimits (720, 480, 2400, 1600);
    setSize (960, 640);
}

void ArsenalEditor::paint (juce::Graphics& g)
{
    g.fillAll (ui::theme::background);

    auto topBar = getLocalBounds().removeFromTop (ui::theme::topBarHeight);
    g.setColour (ui::theme::surface);
    g.fillRect (topBar);
    g.setColour (ui::theme::outline);
    g.drawHorizontalLine (topBar.getBottom(), 0.0f, (float) getWidth());

    if (logo != nullptr)
    {
        auto logoArea = topBar.reduced (ui::theme::unit * 2, ui::theme::unit)
                              .removeFromLeft (160)
                              .toFloat();
        logo->drawWithin (g, logoArea, juce::RectanglePlacement::xLeft
                                     | juce::RectanglePlacement::centred, 1.0f);
    }
}

void ArsenalEditor::resized()
{
    auto bounds = getLocalBounds();
    auto topBar = bounds.removeFromTop (ui::theme::topBarHeight)
                        .reduced (ui::theme::unit * 2, ui::theme::unit);

    topBar.removeFromLeft (160 + ui::theme::unit * 2);  // logo space
    title.setBounds (topBar.removeFromLeft (140));

    masterSlider.setBounds (topBar.removeFromRight (72));
    randomizeButton.setBounds (topBar.removeFromRight (160).reduced (0, ui::theme::unit));

    genericPanel.setSize (bounds.getWidth() - genericViewport.getScrollBarThickness(),
                          juce::jmax (bounds.getHeight(), genericPanel.getHeight()));
    genericViewport.setBounds (bounds);
}

} // namespace arsenal
