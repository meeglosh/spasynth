#include "ArsenalEditor.h"
#include "../ArsenalProcessor.h"
#include "../library/Library.h"

#include "BinaryData.h"

namespace arsenal
{

namespace ui
{

ContentComponent::ContentComponent (ArsenalProcessor& p, std::function<void()> themeToggled)
    : processor (p), onThemeToggled (std::move (themeToggled)),
      filterPanel (p.getAPVTS()),
      chaosPanel (p.getAPVTS()),
      macrosPanel (p.getAPVTS(), params::Section::macros),
      matrixPanel (p.getAPVTS())
{
    logoDark = juce::Drawable::createFromImageData (ArsenalAssets::SPAudio_logo_white_svg,
                                                    ArsenalAssets::SPAudio_logo_white_svgSize);
    logoLight = juce::Drawable::createFromImageData (ArsenalAssets::SPAudio_logo_white_svg,
                                                     ArsenalAssets::SPAudio_logo_white_svgSize);

    title.setText ("ARSENAL", juce::dontSendNotification);
    title.setFont (metrics::titleFont());
    addAndMakeVisible (title);

    subtitle.setText ("Silverplatter Audio", juce::dontSendNotification);
    subtitle.setFont (metrics::smallFont());
    addAndMakeVisible (subtitle);

    prevPresetButton.onClick = [this] { processor.getPresetManager().loadPrevious(); };
    addAndMakeVisible (prevPresetButton);
    nextPresetButton.onClick = [this] { processor.getPresetManager().loadNext(); };
    addAndMakeVisible (nextPresetButton);
    presetNameButton.onClick = [this] { showPresetMenu(); };
    addAndMakeVisible (presetNameButton);
    savePresetButton.onClick = [this] { saveUserPreset(); };
    addAndMakeVisible (savePresetButton);

    randomizeButton.onClick = [this] { processor.randomizeAll(); };
    addAndMakeVisible (randomizeButton);

    wildnessSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    wildnessSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    wildnessSlider.setRange (0.0, 1.0, 0.0);
    wildnessSlider.setValue (processor.getRandomWildness(), juce::dontSendNotification);
    wildnessSlider.onValueChange = [this]
    {
        processor.setRandomWildness ((float) wildnessSlider.getValue());
    };
    wildnessSlider.setTooltip ("Chaos amount: how wild RANDOMIZE ALL rolls");
    addAndMakeVisible (wildnessSlider);

    wildnessLabel.setText ("WILD", juce::dontSendNotification);
    wildnessLabel.setFont (metrics::smallFont());
    wildnessLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (wildnessLabel);

    themeButton.setTooltip ("Switch light/dark theme");
    themeButton.onClick = [this]
    {
        setDarkTheme (! currentTheme().isDark);
        if (onThemeToggled)
            onThemeToggled();
    };
    addAndMakeVisible (themeButton);

    masterSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    masterSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    masterSlider.setPopupDisplayEnabled (true, true, this);
    masterSlider.setTooltip ("Master volume");
    masterAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.getAPVTS(), params::id::masterGain, masterSlider);
    addAndMakeVisible (masterSlider);

    for (int g = 0; g < params::numLockGroups; ++g)
    {
        auto& button = lockButtons[(size_t) g];
        button.setButtonText (params::lockGroupName ((params::LockGroup) g));
        button.setClickingTogglesState (true);
        button.setToggleState (processor.isLockGroupLocked (g), juce::dontSendNotification);
        button.setTooltip ("Lock this section: RANDOMIZE ALL keeps its current settings");
        button.onClick = [this, g]
        {
            processor.setLockGroupLocked (g, lockButtons[(size_t) g].getToggleState());
        };
        addAndMakeVisible (button);
    }

    for (int s = 0; s < params::numOscSlots; ++s)
    {
        oscStrips[(size_t) s] = std::make_unique<OscStrip> (processor, s);
        addAndMakeVisible (*oscStrips[(size_t) s]);
    }

    const auto tabBg = juce::Colours::transparentBlack;
    envTabs.addTab ("AMP", tabBg, new EnvPanel (processor.getAPVTS(), "ampEnv"), true);
    envTabs.addTab ("ENV 2", tabBg, new EnvPanel (processor.getAPVTS(), "env2"), true);
    envTabs.addTab ("ENV 3", tabBg, new EnvPanel (processor.getAPVTS(), "env3"), true);
    addAndMakeVisible (envTabs);

    for (int i = 0; i < params::numLFOs; ++i)
        lfoTabs.addTab ("LFO " + juce::String (i + 1), tabBg,
                        new LFOPanel (processor.getAPVTS(), i), true);
    addAndMakeVisible (lfoTabs);

    addAndMakeVisible (filterPanel);
    addAndMakeVisible (chaosPanel);
    addAndMakeVisible (macrosPanel);

    fxTabs.addTab ("DIST", tabBg, new SectionPanel (processor.getAPVTS(),
                   params::Section::fxDist, "Distortion"), true);
    fxTabs.addTab ("CHORUS", tabBg, new SectionPanel (processor.getAPVTS(),
                   params::Section::fxChorus, "Chorus"), true);
    fxTabs.addTab ("DELAY", tabBg, new SectionPanel (processor.getAPVTS(),
                   params::Section::fxDelay, "Delay"), true);
    fxTabs.addTab ("REVERB", tabBg, new SectionPanel (processor.getAPVTS(),
                   params::Section::fxReverb, "Reverb"), true);
    fxTabs.addTab ("EQ", tabBg, new SectionPanel (processor.getAPVTS(),
                   params::Section::fxEQ, "EQ"), true);
    addAndMakeVisible (fxTabs);

    addAndMakeVisible (matrixPanel);

    processor.addChangeListener (this);
    processor.getPresetManager().addChangeListener (this);
    refreshAll();

    setSize (metrics::baseWidth, metrics::baseHeight);
}

ContentComponent::~ContentComponent()
{
    processor.getPresetManager().removeChangeListener (this);
    processor.removeChangeListener (this);
}

void ContentComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refreshAll();
}

void ContentComponent::refreshAll()
{
    const auto& t = currentTheme();

    title.setColour (juce::Label::textColourId, t.accent);
    subtitle.setColour (juce::Label::textColourId,
                        t.isDark ? t.textSecondary : t.textSecondary.brighter (0.6f));
    wildnessLabel.setColour (juce::Label::textColourId,
                             t.isDark ? t.textSecondary : juce::Colour (0xffb9bbbd));
    randomizeButton.setColour (juce::TextButton::buttonColourId, t.accent);
    randomizeButton.setColour (juce::TextButton::textColourOffId,
                               t.isDark ? t.display : juce::Colours::white);
    themeButton.setButtonText (t.isDark ? juce::String::fromUTF8 ("\xe2\x98\xbc")
                                        : juce::String::fromUTF8 ("\xe2\x98\xbe"));

    const auto presetName = processor.getPresetManager().getCurrentName();
    presetNameButton.setButtonText (
        presetName == "Init" && ! library::findLibraryRoot().isDirectory()
            ? "Set library folder..." : presetName);

    repaint();
}

void ContentComponent::paint (juce::Graphics& g)
{
    const auto& t = currentTheme();
    g.fillAll (t.background);

    // Header is always dark (Massive X does this in its light theme too).
    auto header = getLocalBounds().removeFromTop (metrics::headerHeight);
    g.setColour (t.header);
    g.fillRect (header);

    if (logoDark != nullptr)
        logoDark->drawWithin (g, header.removeFromLeft (52).reduced (10).toFloat(),
                              juce::RectanglePlacement::centred, 1.0f);

    // Footer strip.
    auto footer = getLocalBounds().removeFromBottom (metrics::footerHeight);
    g.setColour (t.header);
    g.fillRect (footer);
    g.setColour (t.textSecondary);
    g.setFont (metrics::smallFont());
    g.drawText (juce::String::fromUTF8 ("ARSENAL  \xc2\xb7  SILVERPLATTER AUDIO"),
                footer.reduced (10, 0), juce::Justification::centredRight);
    g.drawText ("v0.1", footer.reduced (10, 0), juce::Justification::centredLeft);

    // Caption for the randomizer lock strip.
    auto lockCaption = getLocalBounds().withTrimmedTop (metrics::headerHeight)
                           .removeFromTop (metrics::lockRowHeight)
                           .reduced (metrics::unit, 0).removeFromLeft (44);
    g.setColour (t.textSecondary);
    g.setFont (metrics::smallFont());
    g.drawText ("LOCKS", lockCaption, juce::Justification::centredLeft);
}

void ContentComponent::resized()
{
    auto bounds = getLocalBounds();

    // --- Header -------------------------------------------------------------
    auto header = bounds.removeFromTop (metrics::headerHeight);
    header.removeFromLeft (52);  // logo
    auto titleArea = header.removeFromLeft (150).reduced (0, 7);
    title.setBounds (titleArea.removeFromTop (24));
    subtitle.setBounds (titleArea);

    auto right = header.removeFromRight (346).reduced (0, 9);
    masterSlider.setBounds (right.removeFromRight (40));
    themeButton.setBounds (right.removeFromRight (32).reduced (2, 6));
    auto wildArea = right.removeFromRight (44);
    wildnessLabel.setBounds (wildArea.removeFromBottom (11));
    wildnessSlider.setBounds (wildArea);
    randomizeButton.setBounds (right.reduced (4, 3));

    auto presetArea = header.reduced (metrics::unit, 12);
    prevPresetButton.setBounds (presetArea.removeFromLeft (26));
    savePresetButton.setBounds (presetArea.removeFromRight (52));
    nextPresetButton.setBounds (presetArea.removeFromRight (26));
    presetNameButton.setBounds (presetArea.reduced (3, 0));

    // --- Lock strip -----------------------------------------------------------
    auto lockRow = bounds.removeFromTop (metrics::lockRowHeight).reduced (metrics::unit, 2);
    lockRow.removeFromLeft (46);  // "LOCKS" caption painted behind
    const auto lockWidth = lockRow.getWidth() / params::numLockGroups;
    for (auto& button : lockButtons)
        button.setBounds (lockRow.removeFromLeft (lockWidth).reduced (2, 1));

    bounds.removeFromBottom (metrics::footerHeight);

    // --- Module grid ----------------------------------------------------------
    auto main = bounds.reduced (metrics::unit, 4);
    constexpr int gap = 6;

    // Row 1: three oscillators + filter.
    auto row1 = main.removeFromTop (juce::roundToInt ((float) main.getHeight() * 0.40f));
    const auto oscW = (row1.getWidth() - 3 * gap) * 26 / 100;
    for (int s = 0; s < params::numOscSlots; ++s)
    {
        oscStrips[(size_t) s]->setBounds (row1.removeFromLeft (oscW));
        row1.removeFromLeft (gap);
    }
    filterPanel.setBounds (row1);
    main.removeFromTop (gap);

    // Row 2: envelopes, LFOs, chaos, macros.
    auto row2 = main.removeFromTop (juce::roundToInt ((float) main.getHeight() * 0.48f));
    envTabs.setBounds (row2.removeFromLeft (row2.getWidth() * 24 / 100));
    row2.removeFromLeft (gap);
    lfoTabs.setBounds (row2.removeFromLeft (row2.getWidth() * 31 / 100));
    row2.removeFromLeft (gap);
    chaosPanel.setBounds (row2.removeFromLeft (row2.getWidth() * 62 / 100));
    row2.removeFromLeft (gap);
    macrosPanel.setBounds (row2);
    main.removeFromTop (gap);

    // Row 3: FX + matrix.
    auto row3 = main;
    fxTabs.setBounds (row3.removeFromLeft (row3.getWidth() * 44 / 100));
    row3.removeFromLeft (gap);
    matrixPanel.setBounds (row3);
}

void ContentComponent::showPresetMenu()
{
    auto& pm = processor.getPresetManager();
    const auto presets = pm.getPresets();  // snapshot for stable indices

    juce::PopupMenu menu;

    for (const auto& category : pm.getCategories())
    {
        juce::PopupMenu sub;
        for (size_t i = 0; i < presets.size(); ++i)
            if (presets[i].category == category)
                sub.addItem ((int) i + 1, presets[i].name);
        menu.addSubMenu (category, sub);
    }

    if (! presets.empty())
        menu.addSeparator();

    constexpr int chooseLibraryItem = 1000000;
    constexpr int rescanItem = 1000001;
    menu.addItem (chooseLibraryItem, "Set Library Folder...");
    menu.addItem (rescanItem, "Rescan Library && Presets");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (presetNameButton),
                        [this] (int result)
    {
        if (result == 0)
            return;
        if (result == chooseLibraryItem)
            chooseLibraryFolder();
        else if (result == rescanItem)
            processor.refreshLibrary();
        else
            processor.getPresetManager().loadPreset (result - 1);
    });
}

void ContentComponent::chooseLibraryFolder()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Locate the Silverplatter Audio library folder",
        juce::File::getSpecialLocation (juce::File::userHomeDirectory));

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                            | juce::FileBrowserComponent::canSelectDirectories,
                              [this] (const juce::FileChooser& fc)
    {
        const auto folder = fc.getResult();
        if (! folder.isDirectory())
            return;

        library::setLibraryRoot (folder);
        processor.refreshLibrary();
        refreshAll();
    });
}

void ContentComponent::saveUserPreset()
{
    auto& pm = processor.getPresetManager();
    pm.getUserPresetFolder().createDirectory();

    fileChooser = std::make_unique<juce::FileChooser> (
        "Save preset",
        pm.getUserPresetFolder().getChildFile ("My Preset"
            + juce::String (library::PresetManager::presetExtension)),
        "*" + juce::String (library::PresetManager::presetExtension));

    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode
                            | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
    {
        if (fc.getResult() != juce::File())
            processor.getPresetManager().saveUserPreset (
                fc.getResult().getFileNameWithoutExtension());
    });
}

} // namespace ui

// ============================ ArsenalEditor ================================

ArsenalEditor::ArsenalEditor (ArsenalProcessor& p)
    : juce::AudioProcessorEditor (p), arsenalProcessor (p)
{
    setLookAndFeel (&lookAndFeel);

    content = std::make_unique<ui::ContentComponent> (p, [this] { applyTheme(); });
    addAndMakeVisible (*content);

    constexpr auto baseW = ui::metrics::baseWidth;
    constexpr auto baseH = ui::metrics::baseHeight;

    setResizable (true, true);
    if (auto* constrainer = getConstrainer())
    {
        constrainer->setFixedAspectRatio ((double) baseW / baseH);
        constrainer->setSizeLimits (baseW * 55 / 100, baseH * 55 / 100,
                                    baseW * 2, baseH * 2);
    }

    // Restore the remembered window scale.
    const auto scale = juce::jlimit (0.55, 2.0,
        (double) arsenalProcessor.getAPVTS().state.getProperty ("uiScale", 1.0));
    setSize (juce::roundToInt (baseW * scale), juce::roundToInt (baseH * scale));
}

ArsenalEditor::~ArsenalEditor()
{
    setLookAndFeel (nullptr);
}

void ArsenalEditor::applyTheme()
{
    lookAndFeel.refreshPalette();
    sendLookAndFeelChange();
    content->refreshAll();
    repaint();
}

void ArsenalEditor::resized()
{
    if (content == nullptr)
        return;

    const auto scale = (float) getWidth() / (float) ui::metrics::baseWidth;
    content->setTransform (juce::AffineTransform::scale (scale));
    content->setTopLeftPosition (0, 0);

    arsenalProcessor.getAPVTS().state.setProperty ("uiScale", (double) scale, nullptr);
}

} // namespace arsenal
