#pragma once

#include "Theme.h"
#include "../params/ParameterRegistry.h"

namespace arsenal::ui
{

// The mod matrix as a compact routing table: 16 rows of source -> dest with
// a bipolar depth slider, inside a viewport.
class MatrixPanel : public juce::Component
{
public:
    explicit MatrixPanel (juce::AudioProcessorValueTreeState& apvts)
    {
        for (int r = 0; r < params::numModRoutes; ++r)
        {
            auto row = std::make_unique<Row>();

            const auto sourceID = params::id::routeParam (r, params::id::route::source);
            const auto destID = params::id::routeParam (r, params::id::route::dest);
            const auto depthID = params::id::routeParam (r, params::id::route::depth);

            row->source.addItemList (params::find (sourceID)->choices, 1);
            row->dest.addItemList (params::find (destID)->choices, 1);
            row->depth.setSliderStyle (juce::Slider::LinearHorizontal);
            row->depth.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            row->depth.setPopupDisplayEnabled (true, true, this);

            row->sourceAttachment = std::make_unique<
                juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, sourceID, row->source);
            row->destAttachment = std::make_unique<
                juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, destID, row->dest);
            row->depthAttachment = std::make_unique<
                juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, depthID, row->depth);

            content.addAndMakeVisible (row->source);
            content.addAndMakeVisible (row->dest);
            content.addAndMakeVisible (row->depth);
            rows.push_back (std::move (row));
        }

        viewport.setViewedComponent (&content, false);
        viewport.setScrollBarsShown (true, false);
        addAndMakeVisible (viewport);
    }

    void paint (juce::Graphics& g) override
    {
        const auto& t = currentTheme();
        const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (t.surface);
        g.fillRoundedRectangle (bounds, metrics::cornerRadius);
        g.setColour (t.outline);
        g.drawRoundedRectangle (bounds, metrics::cornerRadius, 1.0f);

        g.setColour (t.accent);
        g.setFont (metrics::sectionFont());
        g.drawText ("MOD MATRIX", getLocalBounds().removeFromTop (24).reduced (10, 0),
                    juce::Justification::centredLeft);
    }

    void resized() override
    {
        constexpr int rowHeight = 28;
        auto area = getLocalBounds().withTrimmedTop (24).reduced (6, 4);
        viewport.setBounds (area);

        const auto contentWidth = area.getWidth() - viewport.getScrollBarThickness();
        content.setSize (contentWidth, (int) rows.size() * rowHeight);

        auto y = 0;
        for (auto& row : rows)
        {
            auto r = juce::Rectangle<int> (0, y, contentWidth, rowHeight).reduced (2, 3);
            row->source.setBounds (r.removeFromLeft (contentWidth * 30 / 100));
            r.removeFromLeft (4);
            row->dest.setBounds (r.removeFromLeft (contentWidth * 38 / 100));
            r.removeFromLeft (4);
            row->depth.setBounds (r);
            y += rowHeight;
        }
    }

private:
    struct Row
    {
        juce::ComboBox source, dest;
        juce::Slider depth;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> sourceAttachment,
                                                                                destAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> depthAttachment;
    };

    juce::Component content;
    juce::Viewport viewport;
    std::vector<std::unique_ptr<Row>> rows;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MatrixPanel)
};

} // namespace arsenal::ui
