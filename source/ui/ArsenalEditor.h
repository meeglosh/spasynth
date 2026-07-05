#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "../params/ParameterRegistry.h"
#include "../params/Randomizer.h"
#include "ArsenalLookAndFeel.h"
#include "SectionPanel.h"
#include "MatrixPanel.h"

namespace arsenal
{

class ArsenalProcessor;

namespace ui
{

// A SectionPanel inside a viewport: sections that fit show no scrollbar,
// dense ones scroll instead of clipping.
class ScrollableSection : public juce::Component
{
public:
    ScrollableSection (juce::AudioProcessorValueTreeState& apvts, params::Section section,
                       const juce::String& title = {}, const juce::StringArray& exclude = {})
        : panel (apvts, section, title, exclude)
    {
        viewport.setViewedComponent (&panel, false);
        viewport.setScrollBarsShown (true, false);
        addAndMakeVisible (viewport);
    }

    void resized() override
    {
        viewport.setBounds (getLocalBounds());
        const auto w = getWidth() - viewport.getScrollBarThickness();
        panel.setSize (juce::jmax (60, w),
                       juce::jmax (getHeight(), panel.heightForWidth (w)));
    }

private:
    juce::Viewport viewport;
    SectionPanel panel;
};

// One oscillator slot page: wavetable/SFX loaders on top, the slot's
// registry parameters underneath.
class OscPanel : public juce::Component
{
public:
    OscPanel (ArsenalProcessor&, int slotIndex);

    void refreshNames();
    void resized() override;

private:
    void chooseWavetable();
    void chooseSample();

    ArsenalProcessor& processor;
    const int slot;

    juce::Label tableName, sampleName;
    juce::TextButton loadWTButton { "WT..." }, factoryButton { "Factory" },
                     loadSFXButton { "SFX..." };
    ScrollableSection section;
    std::unique_ptr<juce::FileChooser> fileChooser;
};

// Everything inside the plugin window at base size; the editor shell scales
// this whole component for resizing.
class ContentComponent : public juce::Component,
                         private juce::ChangeListener
{
public:
    ContentComponent (ArsenalProcessor&, std::function<void()> onThemeToggled);
    ~ContentComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void refreshAll();

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void showPresetMenu();
    void chooseLibraryFolder();
    void saveUserPreset();

    ArsenalProcessor& processor;
    std::function<void()> onThemeToggled;

    std::unique_ptr<juce::Drawable> logoDark, logoLight;

    juce::Label title, subtitle;
    juce::TextButton prevPresetButton { "<" }, nextPresetButton { ">" };
    juce::TextButton presetNameButton, savePresetButton { "Save" };
    juce::TextButton randomizeButton { "RANDOMIZE ALL" };
    juce::Slider wildnessSlider;
    juce::Label wildnessLabel;
    juce::TextButton themeButton;
    juce::Slider masterSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> masterAttachment;
    std::array<juce::TextButton, params::numLockGroups> lockButtons;

    juce::TabbedComponent oscTabs { juce::TabbedButtonBar::TabsAtTop };
    std::array<OscPanel*, params::numOscSlots> oscPanels {};  // owned by oscTabs
    ScrollableSection filterPanel, chaosPanel, macrosPanel;
    juce::TabbedComponent envTabs { juce::TabbedButtonBar::TabsAtTop };
    juce::TabbedComponent lfoTabs { juce::TabbedButtonBar::TabsAtTop };
    juce::TabbedComponent fxTabs { juce::TabbedButtonBar::TabsAtTop };
    MatrixPanel matrixPanel;

    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ContentComponent)
};

} // namespace ui

// Shell: hosts the fixed-layout content at base size and scales it
// proportionally — industry-standard plugin resizing. Window scale is
// remembered across sessions.
class ArsenalEditor : public juce::AudioProcessorEditor
{
public:
    explicit ArsenalEditor (ArsenalProcessor&);
    ~ArsenalEditor() override;

    void resized() override;

private:
    void applyTheme();

    ArsenalProcessor& arsenalProcessor;
    ui::ArsenalLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltips { this };
    std::unique_ptr<ui::ContentComponent> content;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArsenalEditor)
};

} // namespace arsenal
