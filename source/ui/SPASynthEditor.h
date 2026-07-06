#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "../params/ParameterRegistry.h"
#include "../params/Randomizer.h"
#include "SPASynthLookAndFeel.h"
#include "ModulePanels.h"
#include "SectionPanel.h"
#include "MatrixPanel.h"
#include "PresetBrowser.h"

namespace spa
{

class SPASynthProcessor;

namespace ui
{

// Everything inside the plugin window at base size; the editor shell scales
// this whole component for resizing.
class ContentComponent : public juce::Component,
                         private juce::ChangeListener
{
public:
    ContentComponent (SPASynthProcessor&, std::function<void()> onThemeChanged);
    ~ContentComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;   // right-click = MIDI Learn
    void refreshAll();

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void togglePresetBrowser();
    void showAccentPicker();
    void chooseLibraryFolder();
    void saveUserPreset();

    // Header button showing the two accents as a split circle; clicking
    // drops down the colour picker.
    struct AccentButton : juce::Button
    {
        AccentButton() : juce::Button ("accentColors") {}
        void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    };

    SPASynthProcessor& processor;
    std::function<void()> onThemeChanged;   // LnF palette refresh + repaint

    std::unique_ptr<juce::Drawable> logoDark, logoLight;

    // Header.
    juce::TextButton prevPresetButton { "<" }, nextPresetButton { ">" };
    juce::TextButton presetNameButton, savePresetButton { "SAVE" };
    juce::TextButton randomizeButton { "RANDOMIZE ALL" };
    juce::Slider wildnessSlider;
    juce::Label wildnessLabel;
    juce::ComboBox glideModeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> glideModeAttachment;
    juce::Slider glideSlider;
    juce::Label glideLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> glideAttachment;
    AccentButton accentButton;
    juce::Slider masterSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> masterAttachment;
    std::array<juce::TextButton, params::numLockGroups> lockButtons;

    // Modules.
    std::array<std::unique_ptr<OscStrip>, params::numOscSlots> oscStrips;
    juce::TabbedComponent filterTabs { juce::TabbedButtonBar::TabsAtTop };
    juce::TabbedComponent envTabs { juce::TabbedButtonBar::TabsAtTop };
    juce::TabbedComponent lfoTabs { juce::TabbedButtonBar::TabsAtTop };
    ChaosPanel chaosPanel;
    ArpPanel arpPanel;
    juce::TabbedComponent fxTabs { juce::TabbedButtonBar::TabsAtTop };
    MatrixPanel matrixPanel;
    OutputMeter outputMeter;

    // Preset drawer: slides over the left side of the module grid.
    std::unique_ptr<PresetBrowser> presetBrowser;
    bool presetBrowserOpen = false;

    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ContentComponent)
};

} // namespace ui

// Shell: hosts the fixed-layout content at base size and scales it
// proportionally — industry-standard plugin resizing. Window scale is
// remembered across sessions.
class SPASynthEditor : public juce::AudioProcessorEditor
{
public:
    explicit SPASynthEditor (SPASynthProcessor&);
    ~SPASynthEditor() override;

    void resized() override;

private:
    void applyTheme();

    SPASynthProcessor& arsenalProcessor;
    ui::SPASynthLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltips { this };
    std::unique_ptr<ui::ContentComponent> content;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SPASynthEditor)
};

} // namespace spa
