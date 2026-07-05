#include "ArsenalEditor.h"
#include "../ArsenalProcessor.h"
#include "Theme.h"

#include "BinaryData.h"

namespace arsenal
{

ArsenalEditor::ArsenalEditor (ArsenalProcessor& p)
    : juce::AudioProcessorEditor (p), arsenalProcessor (p), genericPanel (p)
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
        arsenalProcessor.getAPVTS(), params::id::masterGain, masterSlider);
    addAndMakeVisible (masterSlider);

    for (int s = 0; s < params::numOscSlots; ++s)
    {
        auto& sc = slotControls[(size_t) s];

        sc.header.setText ("OSC " + params::id::oscSlotLetter (s), juce::dontSendNotification);
        sc.header.setFont (ui::theme::labelFont());
        sc.header.setColour (juce::Label::textColourId, ui::theme::accent);
        addAndMakeVisible (sc.header);

        sc.tableName.setFont (ui::theme::labelFont());
        sc.tableName.setColour (juce::Label::textColourId, ui::theme::textSecondary);
        addAndMakeVisible (sc.tableName);

        sc.loadButton.onClick = [this, s] { chooseWavetable (s); };
        addAndMakeVisible (sc.loadButton);

        sc.factoryButton.onClick = [this, s] { arsenalProcessor.setFactoryWavetable (s); };
        addAndMakeVisible (sc.factoryButton);

        sc.sampleName.setFont (ui::theme::labelFont());
        sc.sampleName.setColour (juce::Label::textColourId, ui::theme::textSecondary);
        addAndMakeVisible (sc.sampleName);

        sc.sfxButton.onClick = [this, s] { chooseSample (s); };
        addAndMakeVisible (sc.sfxButton);
    }

    genericViewport.setViewedComponent (&genericPanel, false);
    genericViewport.setScrollBarsShown (true, false);
    addAndMakeVisible (genericViewport);

    arsenalProcessor.addChangeListener (this);
    refreshSlotLabels();

    setResizable (true, true);
    setResizeLimits (720, 480, 2400, 1600);
    setSize (960, 700);
}

ArsenalEditor::~ArsenalEditor()
{
    arsenalProcessor.removeChangeListener (this);
}

void ArsenalEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refreshSlotLabels();
}

void ArsenalEditor::refreshSlotLabels()
{
    for (int s = 0; s < params::numOscSlots; ++s)
    {
        auto& sc = slotControls[(size_t) s];
        const auto error = arsenalProcessor.getWavetableError (s);

        sc.tableName.setText (error.isNotEmpty() ? "! " + error
                                                 : arsenalProcessor.getWavetableName (s),
                              juce::dontSendNotification);
        sc.tableName.setColour (juce::Label::textColourId,
                                error.isNotEmpty() ? ui::theme::accent
                                                   : ui::theme::textSecondary);

        const auto sampleError = arsenalProcessor.getSampleError (s);
        const auto sampleName = arsenalProcessor.getSampleName (s);
        sc.sampleName.setText (sampleError.isNotEmpty() ? "! " + sampleError
                               : sampleName.isNotEmpty() ? sampleName
                                                         : "(no SFX loaded)",
                               juce::dontSendNotification);
        sc.sampleName.setColour (juce::Label::textColourId,
                                 sampleError.isNotEmpty() ? ui::theme::accent
                                                          : ui::theme::textSecondary);
    }
}

void ArsenalEditor::chooseSample (int slot)
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Load SFX/sample for Osc " + params::id::oscSlotLetter (slot),
        juce::File::getSpecialLocation (juce::File::userHomeDirectory),
        "*.wav;*.aif;*.aiff;*.flac;*.mp3");

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                            | juce::FileBrowserComponent::canSelectFiles,
                              [this, slot] (const juce::FileChooser& fc)
    {
        const auto file = fc.getResult();
        if (file.existsAsFile())
            arsenalProcessor.loadSampleFromFile (slot, file);
    });
}

void ArsenalEditor::chooseWavetable (int slot)
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Load wavetable for Osc " + params::id::oscSlotLetter (slot),
        juce::File::getSpecialLocation (juce::File::userHomeDirectory),
        "*.wav;*.aif;*.aiff;*.flac");

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                            | juce::FileBrowserComponent::canSelectFiles,
                              [this, slot] (const juce::FileChooser& fc)
    {
        const auto file = fc.getResult();
        if (file.existsAsFile())
            arsenalProcessor.loadWavetableFromFile (slot, file);
    });
}

void ArsenalEditor::paint (juce::Graphics& g)
{
    g.fillAll (ui::theme::background);

    auto topBar = getLocalBounds().removeFromTop (ui::theme::topBarHeight);
    g.setColour (ui::theme::surface);
    g.fillRect (topBar);

    auto slotStrip = getLocalBounds().withTop (topBar.getBottom())
                                     .withHeight (ui::theme::slotStripHeight);
    g.setColour (ui::theme::surfaceRaised);
    g.fillRect (slotStrip);
    g.setColour (ui::theme::outline);
    g.drawHorizontalLine (slotStrip.getBottom(), 0.0f, (float) getWidth());

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

    auto strip = bounds.removeFromTop (ui::theme::slotStripHeight)
                       .reduced (ui::theme::unit, ui::theme::unit / 2);
    const auto slotWidth = strip.getWidth() / params::numOscSlots;

    for (int s = 0; s < params::numOscSlots; ++s)
    {
        auto area = strip.removeFromLeft (slotWidth).reduced (ui::theme::unit / 2, 0);
        auto& sc = slotControls[(size_t) s];

        sc.header.setBounds (area.removeFromLeft (56));

        auto wtRow = area.removeFromTop (area.getHeight() / 2);
        sc.factoryButton.setBounds (wtRow.removeFromRight (60).reduced (0, 3));
        sc.loadButton.setBounds (wtRow.removeFromRight (52).reduced (0, 3));
        sc.tableName.setBounds (wtRow);

        sc.sfxButton.setBounds (area.removeFromRight (112).reduced (0, 3));
        sc.sampleName.setBounds (area);
    }

    genericPanel.setSize (bounds.getWidth() - genericViewport.getScrollBarThickness(),
                          juce::jmax (bounds.getHeight(), genericPanel.getHeight()));
    genericViewport.setBounds (bounds);
}

} // namespace arsenal
