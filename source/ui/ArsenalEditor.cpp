#include "ArsenalEditor.h"
#include "../ArsenalProcessor.h"
#include "../library/Library.h"

#include "BinaryData.h"

namespace arsenal
{

namespace ui
{

// ============================ OscPanel =====================================

OscPanel::OscPanel (ArsenalProcessor& p, int slotIndex)
    : processor (p), slot (slotIndex),
      section (p.getAPVTS(), params::oscSection (slotIndex), "Oscillator")
{
    tableName.setFont (metrics::labelFont());
    addAndMakeVisible (tableName);
    sampleName.setFont (metrics::labelFont());
    addAndMakeVisible (sampleName);

    loadWTButton.onClick = [this] { chooseWavetable(); };
    addAndMakeVisible (loadWTButton);
    factoryButton.onClick = [this] { processor.setFactoryWavetable (slot); };
    addAndMakeVisible (factoryButton);
    loadSFXButton.onClick = [this] { chooseSample(); };
    addAndMakeVisible (loadSFXButton);

    addAndMakeVisible (section);
    refreshNames();
}

void OscPanel::refreshNames()
{
    const auto wtError = processor.getWavetableError (slot);
    tableName.setText ("WT: " + (wtError.isNotEmpty() ? "! " + wtError
                                                      : processor.getWavetableName (slot)),
                       juce::dontSendNotification);

    const auto sfxError = processor.getSampleError (slot);
    const auto sfx = processor.getSampleName (slot);
    sampleName.setText ("SFX: " + (sfxError.isNotEmpty() ? "! " + sfxError
                                   : sfx.isNotEmpty() ? sfx : "(none)"),
                        juce::dontSendNotification);
}

void OscPanel::resized()
{
    auto area = getLocalBounds().reduced (4);

    auto wtRow = area.removeFromTop (26);
    factoryButton.setBounds (wtRow.removeFromRight (64).reduced (0, 2));
    loadWTButton.setBounds (wtRow.removeFromRight (56).reduced (2, 2));
    tableName.setBounds (wtRow);

    auto sfxRow = area.removeFromTop (26);
    loadSFXButton.setBounds (sfxRow.removeFromRight (122).reduced (2, 2));
    sampleName.setBounds (sfxRow);

    section.setBounds (area);
}

void OscPanel::chooseWavetable()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Load wavetable for Osc " + params::id::oscSlotLetter (slot),
        juce::File::getSpecialLocation (juce::File::userHomeDirectory),
        "*.wav;*.aif;*.aiff;*.flac");

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                            | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
    {
        if (fc.getResult().existsAsFile())
            processor.loadWavetableFromFile (slot, fc.getResult());
    });
}

void OscPanel::chooseSample()
{
    const auto libraryRoot = library::findLibraryRoot();
    fileChooser = std::make_unique<juce::FileChooser> (
        "Load SFX/sample for Osc " + params::id::oscSlotLetter (slot),
        libraryRoot.isDirectory() ? libraryRoot
            : juce::File::getSpecialLocation (juce::File::userHomeDirectory),
        "*.wav;*.aif;*.aiff;*.flac;*.mp3");

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                            | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
    {
        if (fc.getResult().existsAsFile())
            processor.loadSampleFromFile (slot, fc.getResult());
    });
}

// ========================= ContentComponent ================================

ContentComponent::ContentComponent (ArsenalProcessor& p, std::function<void()> themeToggled)
    : processor (p), onThemeToggled (std::move (themeToggled)),
      filterPanel (p.getAPVTS(), params::Section::filter1),
      chaosPanel (p.getAPVTS(), params::Section::chaos, "Organic Chaos"),
      macrosPanel (p.getAPVTS(), params::Section::macros),
      matrixPanel (p.getAPVTS())
{
    logoDark = juce::Drawable::createFromImageData (ArsenalAssets::SPAudio_logo_white_svg,
                                                    ArsenalAssets::SPAudio_logo_white_svgSize);
    logoLight = juce::Drawable::createFromImageData (ArsenalAssets::SPAudio_logo_black_svg,
                                                     ArsenalAssets::SPAudio_logo_black_svgSize);

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

    const auto tabBg = juce::Colours::transparentBlack;
    for (int s = 0; s < params::numOscSlots; ++s)
    {
        auto* panel = new OscPanel (processor, s);
        oscPanels[(size_t) s] = panel;
        oscTabs.addTab ("OSC " + params::id::oscSlotLetter (s), tabBg, panel, true);
    }
    addAndMakeVisible (oscTabs);

    envTabs.addTab ("AMP", tabBg, new ScrollableSection (processor.getAPVTS(),
                    params::Section::ampEnv, "Amp Envelope"), true);
    envTabs.addTab ("ENV 2", tabBg, new ScrollableSection (processor.getAPVTS(),
                    params::Section::env2), true);
    envTabs.addTab ("ENV 3", tabBg, new ScrollableSection (processor.getAPVTS(),
                    params::Section::env3), true);
    addAndMakeVisible (envTabs);

    for (int i = 0; i < params::numLFOs; ++i)
        lfoTabs.addTab ("LFO " + juce::String (i + 1), tabBg,
                        new ScrollableSection (processor.getAPVTS(),
                                               params::lfoSection (i)), true);
    addAndMakeVisible (lfoTabs);

    fxTabs.addTab ("DIST", tabBg, new ScrollableSection (processor.getAPVTS(),
                   params::Section::fxDist, "Distortion"), true);
    fxTabs.addTab ("CHORUS", tabBg, new ScrollableSection (processor.getAPVTS(),
                   params::Section::fxChorus, "Chorus"), true);
    fxTabs.addTab ("DELAY", tabBg, new ScrollableSection (processor.getAPVTS(),
                   params::Section::fxDelay, "Delay"), true);
    fxTabs.addTab ("REVERB", tabBg, new ScrollableSection (processor.getAPVTS(),
                   params::Section::fxReverb, "Reverb"), true);
    fxTabs.addTab ("EQ", tabBg, new ScrollableSection (processor.getAPVTS(),
                   params::Section::fxEQ, "EQ"), true);
    addAndMakeVisible (fxTabs);

    addAndMakeVisible (filterPanel);
    addAndMakeVisible (chaosPanel);
    addAndMakeVisible (macrosPanel);
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

    title.setColour (juce::Label::textColourId, t.textPrimary);
    subtitle.setColour (juce::Label::textColourId, t.textSecondary);
    wildnessLabel.setColour (juce::Label::textColourId, t.textSecondary);
    randomizeButton.setColour (juce::TextButton::buttonColourId, t.accent);
    randomizeButton.setColour (juce::TextButton::textColourOffId,
                               t.isDark ? t.background : juce::Colours::white);
    themeButton.setButtonText (t.isDark ? juce::String::fromUTF8 ("\xe2\x98\xbc")   // sun
                                        : juce::String::fromUTF8 ("\xe2\x98\xbe")); // moon

    for (auto* panel : oscPanels)
        if (panel != nullptr)
            panel->refreshNames();

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

    auto header = getLocalBounds().removeFromTop (metrics::headerHeight);
    g.setColour (t.header);
    g.fillRect (header);
    g.setColour (t.outline);
    g.drawHorizontalLine (header.getBottom(), 0.0f, (float) getWidth());

    // Glyph-only logo; the wordmark stays out of the way by design.
    auto* logo = t.isDark ? logoDark.get() : logoLight.get();
    if (logo != nullptr)
        logo->drawWithin (g, header.removeFromLeft (64).reduced (12).toFloat(),
                          juce::RectanglePlacement::centred, 1.0f);
}

void ContentComponent::resized()
{
    auto bounds = getLocalBounds();

    auto header = bounds.removeFromTop (metrics::headerHeight);
    header.removeFromLeft (64);  // logo
    auto titleArea = header.removeFromLeft (130).reduced (0, 8);
    title.setBounds (titleArea.removeFromTop (26));
    subtitle.setBounds (titleArea);

    auto right = header.removeFromRight (330).reduced (0, metrics::unit);
    masterSlider.setBounds (right.removeFromRight (48));
    themeButton.setBounds (right.removeFromRight (34).reduced (2, 8));
    auto wildArea = right.removeFromRight (46);
    wildnessLabel.setBounds (wildArea.removeFromBottom (12));
    wildnessSlider.setBounds (wildArea);
    randomizeButton.setBounds (right.reduced (4, 4));

    auto presetArea = header.reduced (metrics::unit, 14);
    prevPresetButton.setBounds (presetArea.removeFromLeft (26));
    savePresetButton.setBounds (presetArea.removeFromRight (48));
    nextPresetButton.setBounds (presetArea.removeFromRight (26));
    presetNameButton.setBounds (presetArea.reduced (3, 0));

    auto lockRow = bounds.removeFromTop (metrics::lockRowHeight).reduced (metrics::unit, 3);
    const auto lockWidth = lockRow.getWidth() / params::numLockGroups;
    for (auto& button : lockButtons)
        button.setBounds (lockRow.removeFromLeft (lockWidth).reduced (2, 0));

    auto main = bounds.reduced (metrics::unit);
    constexpr int gap = 6;

    oscTabs.setBounds (main.removeFromLeft (500));
    main.removeFromLeft (gap);

    const auto rowHeight = (main.getHeight() - 2 * gap) / 3;

    auto row1 = main.removeFromTop (rowHeight);
    filterPanel.setBounds (row1.removeFromLeft (190));
    row1.removeFromLeft (gap);
    envTabs.setBounds (row1.removeFromLeft ((row1.getWidth() - gap) / 2));
    row1.removeFromLeft (gap);
    lfoTabs.setBounds (row1);
    main.removeFromTop (gap);

    auto row2 = main.removeFromTop (rowHeight);
    macrosPanel.setBounds (row2.removeFromRight (170));
    row2.removeFromRight (gap);
    chaosPanel.setBounds (row2);
    main.removeFromTop (gap);

    auto row3 = main;
    fxTabs.setBounds (row3.removeFromLeft ((row3.getWidth() - gap) * 45 / 100));
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
