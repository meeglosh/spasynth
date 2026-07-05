#pragma once

#include "Theme.h"
#include "../params/ParameterRegistry.h"

namespace spa::ui
{

// A titled panel that builds its controls straight from the parameter
// registry for one section: rotary knobs for floats/ints, combo boxes for
// choices, pill toggles for bools. Every section of the synth gets a
// consistent, professional look with zero per-panel layout code — and new
// registry parameters appear automatically.
class SectionPanel : public juce::Component
{
public:
    // excludeKeys: parameter IDs to skip (when a bespoke control elsewhere
    // covers them). title empty = use the section name.
    SectionPanel (juce::AudioProcessorValueTreeState& apvts, params::Section section,
                  const juce::String& title = {},
                  const juce::StringArray& excludeIDs = {},
                  bool drawFrame = true);

    void paint (juce::Graphics&) override;
    void resized() override;

    // Height needed for a given width (grid wraps).
    int heightForWidth (int width) const;

private:
    struct Control
    {
        std::unique_ptr<juce::Component> component;
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> comboAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> buttonAttachment;
        bool wide = false;   // combos + toggles span two cells
    };

    static constexpr int cellWidth = 66;
    static constexpr int headerHeight = 20;

    int cellHeight = 72;   // compacted in bare (embedded) mode
    juce::String panelTitle;
    bool framed = true;
    std::vector<Control> controls;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SectionPanel)
};

} // namespace spa::ui
