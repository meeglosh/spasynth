#include "SPASynthEditor.h"
#include "../SPASynthProcessor.h"
#include "../library/Library.h"

#include "BinaryData.h"

namespace spa
{

namespace ui
{

namespace
{
    // Drop-down colour picker for the two accents. Changes apply (and
    // persist) live as the user drags around the colour space.
    class AccentPicker : public juce::Component,
                         private juce::ChangeListener
    {
    public:
        explicit AccentPicker (std::function<void()> changed)
            : onChanged (std::move (changed))
        {
            const auto& t = currentTheme();

            const auto setUpLabel = [this] (juce::Label& l, const juce::String& text)
            {
                l.setText (text, juce::dontSendNotification);
                l.setFont (metrics::sectionFont());
                l.setJustificationType (juce::Justification::centred);
                addAndMakeVisible (l);
            };
            setUpLabel (audioLabel, "AUDIO ACCENT");
            setUpLabel (modLabel, "MOD ACCENT");

            audioSelector.setCurrentColour (t.accent, juce::dontSendNotification);
            modSelector.setCurrentColour (t.accentMod, juce::dontSendNotification);
            for (auto* selector : { &audioSelector, &modSelector })
            {
                selector->addChangeListener (this);
                addAndMakeVisible (*selector);
            }

            resetButton.onClick = [this]
            {
                resetAccentColors();
                const auto& theme = currentTheme();
                audioSelector.setCurrentColour (theme.accent, juce::dontSendNotification);
                modSelector.setCurrentColour (theme.accentMod, juce::dontSendNotification);
                if (onChanged)
                    onChanged();
            };
            addAndMakeVisible (resetButton);

            setSize (480, 260);
        }

        ~AccentPicker() override
        {
            audioSelector.removeChangeListener (this);
            modSelector.removeChangeListener (this);
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced (8);
            auto footer = area.removeFromBottom (26);
            resetButton.setBounds (footer.withSizeKeepingCentre (120, 24));
            area.removeFromBottom (6);

            auto left = area.removeFromLeft (area.getWidth() / 2 - 4);
            audioLabel.setBounds (left.removeFromTop (16));
            audioSelector.setBounds (left);

            area.removeFromLeft (8);
            modLabel.setBounds (area.removeFromTop (16));
            modSelector.setBounds (area);
        }

    private:
        void changeListenerCallback (juce::ChangeBroadcaster*) override
        {
            setAccentColors (audioSelector.getCurrentColour().withAlpha (1.0f),
                             modSelector.getCurrentColour().withAlpha (1.0f));
            if (onChanged)
                onChanged();
        }

        static constexpr int selectorFlags = juce::ColourSelector::showColourAtTop
                                           | juce::ColourSelector::showColourspace;

        std::function<void()> onChanged;
        juce::Label audioLabel, modLabel;
        juce::ColourSelector audioSelector { selectorFlags }, modSelector { selectorFlags };
        juce::TextButton resetButton { "RESET TO DEFAULT" };
    };
}

void ContentComponent::AccentButton::paintButton (juce::Graphics& g,
                                                  bool highlighted, bool down)
{
    const auto& t = currentTheme();
    auto bounds = getLocalBounds().toFloat();
    const auto diameter = juce::jmin (bounds.getWidth(), bounds.getHeight()) - 4.0f;
    const auto circle = bounds.withSizeKeepingCentre (diameter, diameter);

    juce::Path half;
    half.addPieSegment (circle, 0.0f, juce::MathConstants<float>::pi, 0.0f);
    g.setColour (t.accent);
    g.fillPath (half);

    half.clear();
    half.addPieSegment (circle, juce::MathConstants<float>::pi,
                        juce::MathConstants<float>::twoPi, 0.0f);
    g.setColour (t.accentMod);
    g.fillPath (half);

    g.setColour (t.outline);
    g.drawEllipse (circle, 1.0f);

    if (highlighted || down)
    {
        g.setColour (juce::Colours::white.withAlpha (down ? 0.25f : 0.12f));
        g.fillEllipse (circle);
    }
}

ContentComponent::ContentComponent (SPASynthProcessor& p, std::function<void()> themeChanged)
    : processor (p), onThemeChanged (std::move (themeChanged)),
      chaosPanel (p),
      arpPanel (p.getAPVTS()),
      matrixPanel (p.getAPVTS()),
      outputMeter (p.getTelemetry())
{
    logoDark = juce::Drawable::createFromImageData (SPAAssets::SPAudio_logo_white_svg,
                                                    SPAAssets::SPAudio_logo_white_svgSize);
    logoLight = juce::Drawable::createFromImageData (SPAAssets::SPAudio_logo_white_svg,
                                                     SPAAssets::SPAudio_logo_white_svgSize);

    prevPresetButton.onClick = [this] { processor.getPresetManager().loadPrevious(); };
    addAndMakeVisible (prevPresetButton);
    nextPresetButton.onClick = [this] { processor.getPresetManager().loadNext(); };
    addAndMakeVisible (nextPresetButton);
    presetNameButton.onClick = [this] { togglePresetBrowser(); };
    presetNameButton.setTooltip ("Browse presets");
    addAndMakeVisible (presetNameButton);
    savePresetButton.onClick = [this] { saveUserPreset(); };
    addAndMakeVisible (savePresetButton);

    randomizeButton.setComponentID ("primary");
    randomizeButton.onClick = [this] { processor.randomizeAll(); };
    addAndMakeVisible (randomizeButton);

    wildnessSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    wildnessSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    wildnessSlider.setRange (0.0, 1.0, 0.0);
    wildnessSlider.setValue (processor.getRandomWildness(), juce::dontSendNotification);
    wildnessSlider.onValueChange = [this]
    {
        processor.setRandomWildness ((float) wildnessSlider.getValue());
        if (wildnessSlider.isMouseButtonDown())
            wildnessLabel.setText (juce::String (juce::roundToInt (
                wildnessSlider.getValue() * 100.0)) + "%", juce::dontSendNotification);
    };
    wildnessSlider.onDragEnd = [this]
    {
        wildnessLabel.setText ("WILD", juce::dontSendNotification);
    };
    wildnessSlider.setTooltip ("Chaos amount: how wild RANDOMIZE ALL rolls");
    addAndMakeVisible (wildnessSlider);

    wildnessLabel.setText ("WILD", juce::dontSendNotification);
    wildnessLabel.setFont (metrics::smallFont());
    wildnessLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (wildnessLabel);

    glideModeBox.setTooltip ("Portamento: Always, or Legato (only while a key is held)");
    glideModeBox.getProperties().set ("paramID", juce::String (params::id::glideMode));
    if (const auto* def = params::find (params::id::glideMode))
        glideModeBox.addItemList (def->choices, 1);
    glideModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processor.getAPVTS(), params::id::glideMode, glideModeBox);
    addAndMakeVisible (glideModeBox);

    glideSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    glideSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    glideSlider.getProperties().set ("inlineValueSuffix", " ms");
    glideSlider.setTooltip ("Glide time");
    glideSlider.getProperties().set ("paramID", juce::String (params::id::glideTime));
    glideAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.getAPVTS(), params::id::glideTime, glideSlider);
    addAndMakeVisible (glideSlider);

    glideLabel.setText ("GLIDE", juce::dontSendNotification);
    glideLabel.setFont (metrics::smallFont());
    glideLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (glideLabel);

    accentButton.setTooltip ("Customize the accent colors");
    accentButton.onClick = [this] { showAccentPicker(); };
    addAndMakeVisible (accentButton);

    masterSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    masterSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    masterSlider.getProperties().set ("inlineValueSuffix", " dB");  // LnF chip on drag
    masterSlider.setTooltip ("Master volume");
    masterSlider.getProperties().set ("paramID", juce::String (params::id::masterGain));
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

        // Macros have no panel to lock; the enum slot stays (persisted lock
        // masks keep their bit layout) but the button is hidden.
        if ((params::LockGroup) g == params::LockGroup::macros)
            button.setVisible (false);
    }

    for (int s = 0; s < params::numOscSlots; ++s)
    {
        oscStrips[(size_t) s] = std::make_unique<OscStrip> (processor, s);
        addAndMakeVisible (*oscStrips[(size_t) s]);
    }

    const auto tabBg = juce::Colours::transparentBlack;
    envTabs.addTab ("AMP", tabBg, new EnvPanel (processor, "ampEnv", 0), true);
    envTabs.addTab ("ENV 2", tabBg, new EnvPanel (processor, "env2", 1), true);
    envTabs.addTab ("ENV 3", tabBg, new EnvPanel (processor, "env3", 2), true);
    addAndMakeVisible (envTabs);

    for (int i = 0; i < params::numLFOs; ++i)
        lfoTabs.addTab ("LFO " + juce::String (i + 1), tabBg,
                        new LFOPanel (processor, i), true);
    addAndMakeVisible (lfoTabs);

    filterTabs.addTab ("FILTER 1", juce::Colours::transparentBlack,
                       new FilterPanel (processor, 1), true);
    filterTabs.addTab ("FILTER 2", juce::Colours::transparentBlack,
                       new FilterPanel (processor, 2), true);
    addAndMakeVisible (filterTabs);
    addAndMakeVisible (chaosPanel);
    addAndMakeVisible (arpPanel);

    fxTabs.addTab ("DIST", tabBg, new FXPanel (processor.getAPVTS(),
                   FXDisplay::Kind::distortion, params::Section::fxDist, "Distortion"), true);
    fxTabs.addTab ("CHORUS", tabBg, new FXPanel (processor.getAPVTS(),
                   FXDisplay::Kind::chorus, params::Section::fxChorus, "Chorus"), true);
    fxTabs.addTab ("DELAY", tabBg, new FXPanel (processor.getAPVTS(),
                   FXDisplay::Kind::delay, params::Section::fxDelay, "Delay"), true);
    fxTabs.addTab ("REVERB", tabBg, new FXPanel (processor.getAPVTS(),
                   FXDisplay::Kind::reverb, params::Section::fxReverb, "Reverb"), true);
    fxTabs.addTab ("EQ", tabBg, new FXPanel (processor.getAPVTS(),
                   FXDisplay::Kind::eq, params::Section::fxEQ, "EQ"), true);
    addAndMakeVisible (fxTabs);

    addAndMakeVisible (matrixPanel);
    addAndMakeVisible (outputMeter);

    // Added last so the drawer slides over every module.
    presetBrowser = std::make_unique<PresetBrowser> (
        processor,
        [this] { togglePresetBrowser(); },
        [this] { chooseLibraryFolder(); });
    addChildComponent (*presetBrowser);

    processor.addChangeListener (this);
    processor.getPresetManager().addChangeListener (this);
    refreshAll();

    // Right-clicks anywhere inside get routed here for MIDI Learn.
    addMouseListener (this, true);

    setSize (metrics::baseWidth, metrics::baseHeight);
}

void ContentComponent::mouseDown (const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu())
        return;

    // Find the parameter under the click (the control or an ancestor).
    juce::String paramID;
    for (auto* c = e.eventComponent; c != nullptr && c != this; c = c->getParentComponent())
    {
        const auto value = c->getProperties()["paramID"];
        if (! value.isVoid())
        {
            paramID = value.toString();
            break;
        }
    }

    if (paramID.isEmpty())
        return;

    auto& learn = processor.getMidiLearn();
    auto* param = processor.getAPVTS().getParameter (paramID);
    const auto assignedCC = learn.getAssignedCC (paramID);
    const auto armedHere = learn.isArmed() && learn.getArmedParamID() == paramID;

    juce::PopupMenu menu;
    menu.addSectionHeader (param != nullptr ? param->getName (48) : paramID);

    if (armedHere)
        menu.addItem (3, "Cancel MIDI Learn (move a hardware control...)");
    else
        menu.addItem (1, "MIDI Learn");

    if (assignedCC >= 0)
        menu.addItem (2, "Remove assignment (CC " + juce::String (assignedCC) + ")");

    menu.showMenuAsync (juce::PopupMenu::Options().withMousePosition(),
                        [this, paramID] (int result)
    {
        auto& midiLearn = processor.getMidiLearn();
        if (result == 1)
            midiLearn.armLearn (paramID);
        else if (result == 2)
            midiLearn.clearAssignment (paramID);
        else if (result == 3)
            midiLearn.cancelLearn();
    });
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

    wildnessLabel.setColour (juce::Label::textColourId, t.textSecondary);
    glideLabel.setColour (juce::Label::textColourId, t.textSecondary);
    randomizeButton.setColour (juce::TextButton::buttonColourId, t.accent);
    randomizeButton.setColour (juce::TextButton::textColourOffId, t.display);

    const auto presetName = processor.getPresetManager().getCurrentName();
    presetNameButton.setButtonText (
        presetName == "Init" && ! library::findLibraryRoot().isDirectory()
            ? "Set library folder..." : presetName);

    if (presetBrowser != nullptr)
        presetBrowser->refresh();   // theme colours

    repaint();
}

void ContentComponent::paint (juce::Graphics& g)
{
    const auto& t = currentTheme();
    g.fillAll (t.background);

    // Brand band: the big tracked wordmark, centred (per the redesign mock).
    auto band = getLocalBounds().removeFromTop (metrics::brandBandHeight);
    g.setColour (t.header.darker (0.25f));
    g.fillRect (band);
    // Tracked text carries trailing kern space, so plain centred drawText
    // shifts the visible glyphs off-centre (worse the bigger the tracking).
    // Centre on the actual glyph bounding box instead.
    const auto drawTrackedCentred = [&g] (const juce::Font& font, const juce::String& text,
                                          juce::Rectangle<int> area, juce::Colour colour)
    {
        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText (font, text, 0.0f, 0.0f);
        const auto box = glyphs.getBoundingBox (0, -1, true);
        glyphs.moveRangeOfGlyphs (0, -1,
                                  (float) area.getCentreX() - box.getCentreX(),
                                  (float) area.getCentreY() - box.getCentreY());
        g.setColour (colour);
        glyphs.draw (g);
    };

    // The brand band is always dark, so its ink is always light.
    drawTrackedCentred (metrics::wordmarkFont(), "SPASYNTH",
                        band.withTrimmedBottom (9), juce::Colour (0xffe7ecef));
    drawTrackedCentred (metrics::brandSubFont(), "SILVERPLATTER AUDIO",
                        band.withTrimmedTop (21), juce::Colour (0xff7f8d97));

    // Header strip.
    auto header = getLocalBounds().withTrimmedTop (metrics::brandBandHeight)
                      .removeFromTop (metrics::headerHeight);
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
    g.drawText (juce::String::fromUTF8 ("SPASYNTH  \xc2\xb7  SILVERPLATTER AUDIO"),
                footer.reduced (10, 0), juce::Justification::centredRight);
    g.drawText ("v0.1", footer.reduced (10, 0), juce::Justification::centredLeft);
    g.setColour (juce::Colour (0xffe7ecef).withAlpha (0.8f));
    g.setFont (metrics::labelFont());
    g.drawText ("SPASynth", footer, juce::Justification::centred);

    // Caption for the randomizer lock strip.
    auto lockCaption = getLocalBounds()
                           .withTrimmedTop (metrics::brandBandHeight + metrics::headerHeight)
                           .removeFromTop (metrics::lockRowHeight)
                           .reduced (metrics::unit, 0).removeFromLeft (44);
    g.setColour (t.textSecondary);
    g.setFont (metrics::smallFont());
    g.drawText ("LOCKS", lockCaption, juce::Justification::centredLeft);
}

void ContentComponent::resized()
{
    auto bounds = getLocalBounds();
    bounds.removeFromTop (metrics::brandBandHeight);

    // --- Header -------------------------------------------------------------
    auto header = bounds.removeFromTop (metrics::headerHeight);
    header.removeFromLeft (52);  // logo (wordmark lives in the brand band)

    auto right = header.removeFromRight (476).reduced (0, 9);
    outputMeter.setBounds (right.removeFromRight (14).reduced (0, 2));
    right.removeFromRight (4);
    masterSlider.setBounds (right.removeFromRight (40));
    accentButton.setBounds (right.removeFromRight (32).reduced (2, 6));
    auto glideArea = right.removeFromRight (44);
    glideLabel.setBounds (glideArea.removeFromBottom (11));
    glideSlider.setBounds (glideArea);
    glideModeBox.setBounds (right.removeFromRight (66).reduced (0, 5));
    right.removeFromRight (6);
    // WILD sits right beside RANDOMIZE ALL — it shapes what the button rolls.
    auto wildArea = right.removeFromRight (44);
    wildnessLabel.setBounds (wildArea.removeFromBottom (11));
    wildnessSlider.setBounds (wildArea);
    randomizeButton.setBounds (right.reduced (4, 3));

    auto presetArea = header.reduced (metrics::unit, 12);
    prevPresetButton.setBounds (presetArea.removeFromLeft (26));
    savePresetButton.setBounds (presetArea.removeFromRight (52));
    presetArea.removeFromRight (8);   // breathing room between > and SAVE
    nextPresetButton.setBounds (presetArea.removeFromRight (26));
    presetNameButton.setBounds (presetArea.reduced (3, 0));

    // --- Lock strip -----------------------------------------------------------
    auto lockRow = bounds.removeFromTop (metrics::lockRowHeight).reduced (metrics::unit, 2);
    lockRow.removeFromLeft (46);  // "LOCKS" caption painted behind
    int visibleLocks = 0;
    for (auto& button : lockButtons)
        visibleLocks += button.isVisible() ? 1 : 0;
    const auto lockWidth = lockRow.getWidth() / juce::jmax (1, visibleLocks);
    for (auto& button : lockButtons)
        if (button.isVisible())
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
    filterTabs.setBounds (row1);
    main.removeFromTop (gap);

    // Row 2: envelopes, LFOs, chaos, macros.
    auto row2 = main.removeFromTop (juce::roundToInt ((float) main.getHeight() * 0.48f));
    envTabs.setBounds (row2.removeFromLeft (row2.getWidth() * 22 / 100));
    row2.removeFromLeft (gap);
    lfoTabs.setBounds (row2.removeFromLeft (row2.getWidth() * 28 / 100));
    row2.removeFromLeft (gap);
    chaosPanel.setBounds (row2.removeFromLeft (row2.getWidth() * 52 / 100));
    row2.removeFromLeft (gap);
    arpPanel.setBounds (row2);
    main.removeFromTop (gap);

    // Row 3: FX + matrix.
    auto row3 = main;
    fxTabs.setBounds (row3.removeFromLeft (row3.getWidth() * 44 / 100));
    row3.removeFromLeft (gap);
    matrixPanel.setBounds (row3);

    // --- Preset drawer (overlay, left) ----------------------------------------
    const auto drawerArea = getLocalBounds()
                                .withTrimmedTop (metrics::brandBandHeight + metrics::headerHeight)
                                .withTrimmedBottom (metrics::footerHeight)
                                .removeFromLeft (320);
    presetBrowser->setOpenBounds (drawerArea);
    presetBrowser->setBounds (presetBrowserOpen
                                  ? drawerArea
                                  : drawerArea.translated (-drawerArea.getWidth() - 12, 0));
}

void ContentComponent::showAccentPicker()
{
    auto picker = std::make_unique<AccentPicker> ([this]
    {
        if (onThemeChanged)
            onThemeChanged();
    });

    // Parent to the editor shell (outside this component's scale transform).
    if (auto* top = getTopLevelComponent())
        juce::CallOutBox::launchAsynchronously (
            std::move (picker),
            top->getLocalArea (&accentButton, accentButton.getLocalBounds()),
            top);
}

void ContentComponent::togglePresetBrowser()
{
    presetBrowserOpen = ! presetBrowserOpen;

    const auto open = presetBrowser->getOpenBounds();
    const auto closed = open.translated (-open.getWidth() - 12, 0);

    presetBrowser->setVisible (true);
    presetBrowser->toFront (false);
    juce::Desktop::getInstance().getAnimator().animateComponent (
        presetBrowser.get(), presetBrowserOpen ? open : closed,
        1.0f, 170, false, 1.0, 0.7);

    if (presetBrowserOpen)
        presetBrowser->grabKeyboardFocus();   // Esc closes
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

// ============================ SPASynthEditor ================================

SPASynthEditor::SPASynthEditor (SPASynthProcessor& p)
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
        constrainer->setSizeLimits (baseW * 40 / 100, baseH * 40 / 100,
                                    baseW * 2, baseH * 2);
    }

    // Restore the remembered window scale, clamped so the whole editor
    // (including the resize corner) always fits the host's screen — on small
    // displays the default must shrink, never open with edges off-screen.
    auto maxScale = 2.0;
    if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
    {
        const auto usable = display->userBounds;
        maxScale = juce::jmin (
            2.0,
            (double) (usable.getWidth() - 40) / baseW,
            (double) (usable.getHeight() - 140) / baseH);   // headroom for DAW chrome
    }

    const auto saved = (double) arsenalProcessor.getAPVTS().state.getProperty ("uiScale", 1.0);
    const auto scale = juce::jlimit (0.4, juce::jmax (0.4, maxScale), saved);
    setSize (juce::roundToInt (baseW * scale), juce::roundToInt (baseH * scale));
}

SPASynthEditor::~SPASynthEditor()
{
    setLookAndFeel (nullptr);
}

void SPASynthEditor::applyTheme()
{
    lookAndFeel.refreshPalette();
    sendLookAndFeelChange();
    content->refreshAll();
    repaint();
}

void SPASynthEditor::resized()
{
    if (content == nullptr)
        return;

    const auto scale = (float) getWidth() / (float) ui::metrics::baseWidth;
    content->setTransform (juce::AffineTransform::scale (scale));
    content->setTopLeftPosition (0, 0);

    arsenalProcessor.getAPVTS().state.setProperty ("uiScale", (double) scale, nullptr);
}

} // namespace spa
