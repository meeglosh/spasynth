#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "../params/ParameterRegistry.h"
#include "../params/Randomizer.h"
#include "ArsenalLookAndFeel.h"
#include "ModulePanels.h"
#include "SectionPanel.h"
#include "MatrixPanel.h"

namespace arsenal
{

class ArsenalProcessor;

namespace ui
{

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
    void mouseDown (const juce::MouseEvent&) override;   // right-click = MIDI Learn
    void refreshAll();

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void showPresetMenu();
    void chooseLibraryFolder();
    void saveUserPreset();

    ArsenalProcessor& processor;
    std::function<void()> onThemeToggled;

    std::unique_ptr<juce::Drawable> logoDark, logoLight;

    // Header.
    juce::Label title, subtitle;
    juce::TextButton prevPresetButton { "<" }, nextPresetButton { ">" };
    juce::TextButton presetNameButton, savePresetButton { "SAVE" };
    juce::TextButton randomizeButton { "RANDOMIZE ALL" };
    juce::Slider wildnessSlider;
    juce::Label wildnessLabel;
    juce::TextButton themeButton;
    juce::Slider masterSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> masterAttachment;
    std::array<juce::TextButton, params::numLockGroups> lockButtons;

    // Modules.
    std::array<std::unique_ptr<OscStrip>, params::numOscSlots> oscStrips;
    FilterPanel filterPanel;
    juce::TabbedComponent envTabs { juce::TabbedButtonBar::TabsAtTop };
    juce::TabbedComponent lfoTabs { juce::TabbedButtonBar::TabsAtTop };
    ChaosPanel chaosPanel;
    ArpPanel arpPanel;
    juce::TabbedComponent fxTabs { juce::TabbedButtonBar::TabsAtTop };
    MatrixPanel matrixPanel;
    OutputMeter outputMeter;

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
