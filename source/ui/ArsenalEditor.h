#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "../params/ParameterRegistry.h"
#include "../params/Randomizer.h"

namespace arsenal
{

class ArsenalProcessor;

// Checkpoint-2 editor: branded top bar (logo, title, master volume, disabled
// RANDOMIZE ALL placeholder), a per-slot wavetable strip (name + load/factory
// controls), and an auto-generated parameter panel. Bespoke layout replaces
// the generic panel as sections are built out.
class ArsenalEditor : public juce::AudioProcessorEditor,
                      private juce::ChangeListener
{
public:
    explicit ArsenalEditor (ArsenalProcessor&);
    ~ArsenalEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void refreshSlotLabels();
    void chooseWavetable (int slot);
    void chooseSample (int slot);

    ArsenalProcessor& arsenalProcessor;

    std::unique_ptr<juce::Drawable> logo;
    juce::Label title;
    juce::TextButton randomizeButton { "RANDOMIZE ALL" };
    juce::Slider wildnessSlider;
    juce::Label wildnessLabel;
    std::array<juce::TextButton, params::numLockGroups> lockButtons;
    juce::Slider masterSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> masterAttachment;

    struct SlotControls
    {
        juce::Label header;
        juce::Label tableName;
        juce::TextButton loadButton { "WT..." };
        juce::TextButton factoryButton { "Factory" };
        juce::Label sampleName;
        juce::TextButton sfxButton { "SFX..." };
    };
    std::array<SlotControls, params::numOscSlots> slotControls;
    std::unique_ptr<juce::FileChooser> fileChooser;

    juce::GenericAudioProcessorEditor genericPanel;
    juce::Viewport genericViewport;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArsenalEditor)
};

} // namespace arsenal
