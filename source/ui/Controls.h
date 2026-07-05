#pragma once

#include "Theme.h"
#include "../params/ParameterRegistry.h"

namespace arsenal::ui
{

// A thin-ring knob with its label underneath — the atomic control of the UI.
// Set modColoured for modulation-domain knobs (cyan ring).
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID,
          const juce::String& labelText, bool modColoured = false)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setPopupDisplayEnabled (true, false, this);   // value on drag; hover gets a glow
        if (modColoured)
            slider.setComponentID ("mod");
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            apvts, paramID, slider);
        addAndMakeVisible (slider);

        label.setText (labelText.toUpperCase(), juce::dontSendNotification);
        label.setFont (metrics::smallFont());
        label.setJustificationType (juce::Justification::centred);
        label.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (label);
    }

    void resized() override
    {
        auto area = getLocalBounds();
        label.setBounds (area.removeFromBottom (13));
        slider.setBounds (area);
    }

    juce::Slider slider;
    juce::Label label;

private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// Labelled combo box row.
class Choice : public juce::Component
{
public:
    Choice (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID)
    {
        if (const auto* def = params::find (paramID))
            combo.addItemList (def->choices, 1);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            apvts, paramID, combo);
        addAndMakeVisible (combo);
    }

    void resized() override { combo.setBounds (getLocalBounds()); }

    juce::ComboBox combo;

private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
};

// Pill toggle bound to a bool parameter.
class Toggle : public juce::Component
{
public:
    Toggle (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID,
            const juce::String& text)
        : button (text)
    {
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            apvts, paramID, button);
        addAndMakeVisible (button);
    }

    void resized() override { button.setBounds (getLocalBounds()); }

    juce::ToggleButton button;

private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};

} // namespace arsenal::ui
