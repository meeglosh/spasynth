#pragma once

#include "Theme.h"
#include "../params/ParameterRegistry.h"

namespace spa::ui
{

// The mod matrix as a compact routing table: 16 rows of source -> dest with
// a bipolar depth slider, inside a viewport.
class MatrixPanel : public juce::Component
{
public:
    // off      : ASSIGN is not active.
    // oneShot  : entered by a single click from off; switches itself back to
    //            off the moment the matrix row just written becomes fully
    //            populated (source AND dest both set) -- see AssignOverlay's
    //            onOneShotComplete.
    // latched  : entered by a double click from off (or by promotion, see
    //            nextModeOnDoubleClick); behaves like the old ASSIGN mode,
    //            staying on through any number of assignments until another
    //            click or Esc.
    enum class AssignMode { off, oneShot, latched };

    // Pure state-machine steps, exposed so tests can drive them directly
    // instead of depending on real mouse double-click timing.
    //
    // A single click always toggles: off->oneShot, anything else->off.
    static AssignMode nextModeOnClick (AssignMode current)
    {
        return current == AssignMode::off ? AssignMode::oneShot : AssignMode::off;
    }

    // A genuine double click is, at the JUCE level, two ordinary clicks
    // followed by a mouseDoubleClick callback (Component::internalMouseUp:
    // the second click's mouseUp -- which fires its own click/onClick --
    // runs BEFORE mouseDoubleClick). So by the time this runs,
    // nextModeOnClick has already been applied twice by the two clicks
    // themselves (e.g. off->oneShot->off, or oneShot->off->oneShot) --
    // `afterClick` is that already-applied, and now stale, result.
    //
    // Product-owner rule: a double click ALWAYS means latch, unconditionally,
    // regardless of what mode the two individual clicks landed on -- "the
    // same way caps lock does not care what the shift key was doing". So
    // this ignores `afterClick` entirely and always returns latched; the
    // parameter stays for symmetry with nextModeOnClick and because a real
    // double click's mouseDoubleClick callback naturally has "the mode after
    // click 2" as its input, even though this function doesn't need it.
    static AssignMode nextModeOnDoubleClick (AssignMode /*afterClick*/)
    {
        return AssignMode::latched;
    }

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
            row->depth.setComponentID ("mod");
            // Don't steal keyboard focus from the on-screen keyboard's QWERTY
            // note input on click (see Controls.h's Knob for the same fix).
            row->source.setWantsKeyboardFocus (false);
            row->dest.setWantsKeyboardFocus (false);
            row->depth.setWantsKeyboardFocus (false);
            // setWantsKeyboardFocus alone doesn't stop a click from grabbing
            // focus -- see Controls.h's Knob for the full explanation.
            row->source.setMouseClickGrabsKeyboardFocus (false);
            row->dest.setMouseClickGrabsKeyboardFocus (false);
            row->depth.setMouseClickGrabsKeyboardFocus (false);
            row->source.getProperties().set ("paramID", sourceID);
            row->dest.getProperties().set ("paramID", destID);
            row->depth.getProperties().set ("paramID", depthID);

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

        // ASSIGN toggle: click-to-route mode (see AssignOverlay). Styled like
        // any other header text button; focus-grab disabled per the QWERTY
        // rule (Controls.h's Knob comment has the full explanation).
        // Toggle state is driven entirely by our own mode state machine
        // (applyMode), not JUCE's built-in click-toggles-state, so it can't
        // fight the double-click promotion logic -- see nextModeOnDoubleClick.
        assignBtn.setClickingTogglesState (false);
        assignBtn.setWantsKeyboardFocus (false);
        assignBtn.setMouseClickGrabsKeyboardFocus (false);
        assignBtn.setComponentID ("matrixAssign");
        assignBtn.onClick = [this]
        {
            applyMode (nextModeOnClick (assignMode));
        };
        assignBtn.onDoubleClick = [this]
        {
            applyMode (nextModeOnDoubleClick (assignMode));
        };
        addAndMakeVisible (assignBtn);
    }

    juce::Button& assignButton() { return assignBtn; }
    bool isAssignOn() const { return assignMode != AssignMode::off; }
    AssignMode getAssignMode() const { return assignMode; }
    // Sets the toggle and fires onAssignToggled if the state actually
    // changed (used by ContentComponent::keyPressed's Esc handling). Only
    // off/one-shot are meaningful entry points here (Esc always wants off;
    // nothing currently forces latch on programmatically).
    void setAssignOn (bool on)
    {
        if (isAssignOn() == on)
            return;
        applyMode (on ? AssignMode::oneShot : AssignMode::off);
    }

    std::function<void(AssignMode)> onAssignToggled;

    // Test hooks: apply exactly what a real single/double click does,
    // without needing real mouse-event timing (see the class comment on
    // nextModeOnDoubleClick for why a double click is three separate
    // applyMode calls -- click 1's onClick, click 2's onClick, then
    // mouseDoubleClick -- and simulateDoubleClick reproduces all three from
    // whatever the current mode is, exactly like AssignButton would).
    void simulateClick() { applyMode (nextModeOnClick (assignMode)); }
    void simulateDoubleClick()
    {
        applyMode (nextModeOnClick (assignMode));
        applyMode (nextModeOnClick (assignMode));
        applyMode (nextModeOnDoubleClick (assignMode));
    }

    void paint (juce::Graphics& g) override
    {
        draw::panel (g, getLocalBounds().toFloat());
        draw::sectionHeader (g, getLocalBounds(), "Mod Matrix", {},
                             currentTheme().accentMod);
    }

    void resized() override
    {
        constexpr int rowHeight = 25;
        {
            auto headerArea = getLocalBounds().removeFromTop (metrics::sectionHeaderHeight);
            assignBtn.setBounds (headerArea.removeFromRight (70).reduced (6, 5));
        }
        auto area = getLocalBounds().withTrimmedTop (metrics::sectionHeaderHeight).reduced (6, 4);

        // The panel is never tall enough to show all 16 rows at once (this
        // is a scrolling list by design -- see the class comment), but the
        // viewport's own bottom edge rarely lands on a row boundary, so
        // whatever row straddles it gets rendered half-cut against the
        // panel's bottom. Clamp the viewport to a whole number of rows so
        // the last VISIBLE row is always fully shown; the leftover pixels
        // become a small bottom margin instead of a sliced row.
        const auto visibleRows = juce::jmax (1, area.getHeight() / rowHeight);
        area = area.withHeight (juce::jmin (area.getHeight(), visibleRows * rowHeight));
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

    // Plain TextButton has no virtual double-click hook of its own; this adds
    // one without touching its click-toggling (disabled -- see the
    // constructor comment) or its normal onClick delivery, which still fires
    // once per click including both clicks of a double click.
    struct AssignButton : public juce::TextButton
    {
        using juce::TextButton::TextButton;
        std::function<void()> onDoubleClick;
        void mouseDoubleClick (const juce::MouseEvent& e) override
        {
            juce::TextButton::mouseDoubleClick (e);
            if (onDoubleClick)
                onDoubleClick();
        }
    };

    // Single place that changes the mode: keeps the button's toggle state,
    // the "assignLatched" property the lock glyph reads (see
    // SPASynthLookAndFeel::drawButtonText), and the outside world (via
    // onAssignToggled) all in sync, so no caller can leave the button
    // visually on but functionally off or vice versa.
    void applyMode (AssignMode newMode)
    {
        assignMode = newMode;
        assignBtn.setToggleState (newMode != AssignMode::off, juce::dontSendNotification);
        assignBtn.getProperties().set ("assignLatched", newMode == AssignMode::latched);
        assignBtn.repaint();
        if (onAssignToggled)
            onAssignToggled (assignMode);
    }

    juce::Component content;
    juce::Viewport viewport;
    std::vector<std::unique_ptr<Row>> rows;
    AssignButton assignBtn { "ASSIGN" };
    AssignMode assignMode = AssignMode::off;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MatrixPanel)
};

} // namespace spa::ui
