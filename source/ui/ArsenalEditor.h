#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

namespace arsenal
{

class ArsenalProcessor;

// Checkpoint-1 editor: branded top bar (logo, title, master volume, disabled
// RANDOMIZE ALL placeholder) over an auto-generated parameter panel. The
// bespoke layout replaces the generic panel as sections are built out.
class ArsenalEditor : public juce::AudioProcessorEditor
{
public:
    explicit ArsenalEditor (ArsenalProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    ArsenalProcessor& processor;

    std::unique_ptr<juce::Drawable> logo;
    juce::Label title;
    juce::TextButton randomizeButton { "RANDOMIZE ALL" };
    juce::Slider masterSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> masterAttachment;

    juce::GenericAudioProcessorEditor genericPanel;
    juce::Viewport genericViewport;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArsenalEditor)
};

} // namespace arsenal
