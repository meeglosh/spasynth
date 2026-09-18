#pragma once

#include "Theme.h"
#include "../params/ParameterRegistry.h"
#include <set>
#include <utility>

namespace spa::ui
{

// "None" is choice index 0 for BOTH the source and destination route choice
// lists -- checked directly against ParameterRegistry.cpp, not assumed:
// modSourceNames() starts { "None", "Env 1 (Amp)", ... } and
// ParameterRegistry::all()'s destNames is built starting from
// juce::StringArray destNames { "None" } before any real destination is
// appended.
inline constexpr int kNoneRouteChoiceIndex = 0;

// Single source of truth for "is this matrix row actually wired" (source AND
// destination both something other than None) -- shared by AssignOverlay
// (which uses it to decide when a one-shot ASSIGN session is done) and the
// auto-depth-fill below, so the two can't ever disagree about what counts as
// a complete route.
inline bool routeIsComplete (juce::AudioProcessorValueTreeState& apvts, int route)
{
    if (route < 0)
        return false;

    auto* sourceP = apvts.getParameter (params::id::routeParam (route, params::id::route::source));
    auto* destP = apvts.getParameter (params::id::routeParam (route, params::id::route::dest));
    if (sourceP == nullptr || destP == nullptr)
        return false;

    const int sourceChoice = (int) sourceP->convertFrom0to1 (sourceP->getValue());
    const int destChoice = (int) destP->convertFrom0to1 (destP->getValue());
    return sourceChoice != kNoneRouteChoiceIndex && destChoice != kNoneRouteChoiceIndex;
}

// Depth is bipolar (-1..+1), defaults to 0, and 0 is its correct centre/
// host-reset/double-click-reset value -- so this must NEVER become a general
// "depth==0 looks wrong, fix it" rule. It exists purely to answer the
// tester complaint that a freshly-made route is silent until Depth is also
// turned up: the moment a row's source+dest BOTH become real (routeIsComplete)
// AND its depth is still exactly the untouched default, nudge it to +0.5.
//
// CALLER CONTRACT, the load-bearing part: this must be invoked ONLY from a
// call site that is itself provably reachable exclusively from a genuine
// user edit -- never from anything a preset load, host session restore,
// reset-to-default or RANDOMIZE ALL can reach (all of those write parameters
// via apvts.replaceState()/setValueNotifyingHost() directly, with no user
// gesture involved, and a saved/rolled route deliberately sitting at 0 must
// survive unchanged). See AssignOverlay::setRouteChoice (called only from
// the overlay's own click handling) and MatrixPanel::Row's GestureGate
// (gated on RouteComboBox::consumeUserGesture(), see its comment) for the
// two call sites that satisfy this.
inline void maybeAutoFillRouteDepth (juce::AudioProcessorValueTreeState& apvts, int route)
{
    if (! routeIsComplete (apvts, route))
        return;

    auto* depthP = apvts.getParameter (params::id::routeParam (route, params::id::route::depth));
    if (depthP == nullptr)
        return;

    const auto zeroNorm = depthP->convertTo0to1 (0.0f);
    if (! juce::approximatelyEqual (depthP->getValue(), zeroNorm))
        return;   // user (or the preset) already put a real value here -- leave it alone.

    depthP->beginChangeGesture();
    depthP->setValueNotifyingHost (depthP->convertTo0to1 (0.5f));
    depthP->endChangeGesture();
}

// "Is modulating this destination currently a no-op" -- e.g. chaos.rate
// remains a valid, serialized mod destination while chaos SYNC is on, but
// SYNC replaces the free-running rate with a tempo division, so a route
// aimed at it does nothing until SYNC is switched off again. A tester read
// this as a broken assignment rather than a temporarily-inert one.
//
// Table-driven ON PURPOSE: destination indices are dense and serialized
// into every preset (see ParameterRegistry's append-only mod-dest order),
// so a destination can never be REMOVED from the list just because it's
// sometimes inert -- only dimmed. Keeping this general (a small table of
// dest -> gate rules) means a future case is a new row here, not a
// hardcoded special case scattered across the matrix/overlay code.
inline bool isModDestinationInert (juce::AudioProcessorValueTreeState& apvts, const juce::String& destParamID)
{
    struct InertRule
    {
        const char* destID;
        const char* gateID;
        bool inertWhenGateAtLeastHalf;   // gate param's normalized-off-threshold convention
    };
    static const InertRule rules[] = {
        { params::id::chaos::rate, params::id::chaos::syncToBpm, true },
    };

    for (auto& rule : rules)
    {
        if (destParamID != rule.destID)
            continue;
        auto* gate = apvts.getRawParameterValue (rule.gateID);
        if (gate == nullptr)
            return false;
        const bool gateOn = gate->load() >= 0.5f;
        return gateOn == rule.inertWhenGateAtLeastHalf;
    }
    return false;
}

// Same, but by dense mod-dest index (what a matrix row's DEST choice
// actually stores) rather than a parameter ID string.
inline bool isModDestIndexInert (juce::AudioProcessorValueTreeState& apvts, int destIndex)
{
    if (destIndex < 0)
        return false;
    for (auto& d : params::modDestinations())
        if (d.index == destIndex)
            return isModDestinationInert (apvts, d.def->id);
    return false;
}

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

    explicit MatrixPanel (juce::AudioProcessorValueTreeState& apvtsIn) : apvts (apvtsIn)
    {
        content.owner = this;

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

            // Auto-fill Depth to +0.5 the moment THIS row's own dropdowns make
            // it a complete route, but ONLY on a genuine user pick -- see
            // RouteComboBox and GestureGate below. Wired after the attachments
            // above so the attachment's own ComboBox::Listener (added first,
            // inside the attachment's constructor) has already applied the
            // resulting parameter value by the time these gates run (JUCE
            // calls listeners in add order), so routeIsComplete() sees the
            // up-to-date choice.
            row->sourceGate.box = &row->source;
            row->sourceGate.onUserChange = [this, r] { maybeAutoFillRouteDepth (apvts, r); };
            row->source.addListener (&row->sourceGate);

            row->destGate.box = &row->dest;
            row->destGate.onUserChange = [this, r] { maybeAutoFillRouteDepth (apvts, r); };
            row->dest.addListener (&row->destGate);

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

        // Repaint the matrix whenever anything an inert-destination rule
        // gates might have changed (currently just chaos SYNC), so a dimmed
        // row un-dims live without needing ASSIGN mode or a manual refresh.
        // AudioProcessorValueTreeState::Listener callbacks can fire from the
        // audio thread (host automation), so this defers the actual repaint
        // via AsyncUpdater -- same idiom as ChaosPanel/DependentEnable.
        inertWatcher.target = &content;
        apvts.addParameterListener (params::id::chaos::syncToBpm, &inertWatcher);
    }

    ~MatrixPanel() override
    {
        apvts.removeParameterListener (params::id::chaos::syncToBpm, &inertWatcher);
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

    // Test hook: applies exactly what picking `choiceIndex` from row
    // `route`'s real source/dest dropdown popup would do, INCLUDING marking
    // the box's own "a real popup just closed" flag (see RouteComboBox) --
    // without needing to drive real PopupMenu mouse timing headlessly (same
    // rationale as simulateClick/simulateDoubleClick above). Goes through
    // the actual ComboBox (setSelectedId, JUCE's own item-id-is-index+1
    // convention from addItemList's base id of 1), so it exercises the
    // identical GestureGate path a live click would.
    void simulateUserComboPick (int route, bool isSourceCombo, int choiceIndex)
    {
        if (route < 0 || route >= (int) rows.size())
            return;
        auto& row = *rows[(size_t) route];
        auto& box = isSourceCombo ? row.source : row.dest;
        box.markUserGestureForTest();
        box.setSelectedId (choiceIndex + 1, juce::sendNotificationSync);
    }

    // Test hook: opens row `route`'s source/dest dropdown and dismisses it
    // WITHOUT picking anything -- see RouteComboBox's guard against exactly
    // this leaving a stale pending gesture behind for some later,
    // unrelated, purely programmatic change to that same box to misread.
    void simulateUserOpensThenAbandonsCombo (int route, bool isSourceCombo)
    {
        if (route < 0 || route >= (int) rows.size())
            return;
        auto& row = *rows[(size_t) route];
        auto& box = isSourceCombo ? row.source : row.dest;
        box.markUserGestureForTest();
        box.simulatePopupClosedWithoutSelectionForTest();
    }

    // Mod-route reveal (see Controls.h's RevealClickSlider and
    // ContentComponent::revealMatrixRoutes): highlights every row in
    // `routes` in the same violet as an assigned knob, and scrolls the
    // viewport so the FIRST one is visible. Persists until explicitly
    // cleared -- no fade, no timer -- since the point is to keep working on
    // the row after finding it. Replaces whatever was previously
    // highlighted (a fresh reveal always starts from a clean slate).
    void setRevealedRoutes (const std::vector<int>& routes)
    {
        revealedRoutes.clear();
        for (auto r : routes)
            revealedRoutes.insert (r);
        content.repaint();
        if (! routes.empty())
            scrollRouteIntoView (routes.front());
    }

    // Clicking anywhere that isn't an assigned knob, or Esc, or entering
    // ASSIGN mode all clear the reveal (see ContentComponent). No-op (no
    // repaint) if nothing was highlighted, so this is safe to call
    // unconditionally on every click.
    void clearRevealedRoutes()
    {
        if (revealedRoutes.empty())
            return;
        revealedRoutes.clear();
        content.repaint();
    }

    bool isRouteRevealedForTest (int route) const { return revealedRoutes.count (route) != 0; }
    int getRevealedRouteCountForTest() const { return (int) revealedRoutes.size(); }
    int getRowHeightForTest() const { return rowHeight; }
    int getViewPositionYForTest() const { return viewport.getViewPositionY(); }
    int getViewHeightForTest() const { return viewport.getViewHeight(); }

    // Waiting-for-other-half state (see AssignOverlay::setRouteChoice /
    // ContentComponent's onRouteWritten wiring): set right after an ASSIGN
    // click writes one half of a route, and it's the only "row" concept
    // driven from outside a real user click on the row itself, so it's kept
    // separate from revealedRoutes (which is knob-reveal only, a different
    // feature). Pass -1 to clear. A no-op repaint guard, same as
    // clearRevealedRoutes, since this is called on every ASSIGN write.
    void setWaitingRoute (int route)
    {
        waitingRoute = route;
        // Unconditional (not gated on the value actually changing): this is
        // called right after every ASSIGN write, including ones that leave
        // `route` the same waiting row it already was, and the inert-dim
        // wash (a different row concept, painted in the same pass) needs a
        // fresh look at the just-written DEST choice regardless.
        content.repaint();
    }
    bool isRouteWaitingForTest (int route) const { return waitingRoute == route; }

    // Test hooks for the waiting-ring geometry (see waitingRingBounds's own
    // comment): the exact ring rectangle paintWaitingHighlight draws, plus
    // the three controls it must never overlap, all in `content`'s local
    // coordinate space (what paint() itself works in).
    juce::Rectangle<int> getWaitingRingBoundsForTest (int route) const { return waitingRingBounds (route); }
    juce::Rectangle<int> getRowSourceBoundsForTest (int route) const
    {
        return (route < 0 || route >= (int) rows.size()) ? juce::Rectangle<int>() : rows[(size_t) route]->source.getBounds();
    }
    juce::Rectangle<int> getRowDestBoundsForTest (int route) const
    {
        return (route < 0 || route >= (int) rows.size()) ? juce::Rectangle<int>() : rows[(size_t) route]->dest.getBounds();
    }
    juce::Rectangle<int> getRowDepthBoundsForTest (int route) const
    {
        return (route < 0 || route >= (int) rows.size()) ? juce::Rectangle<int>() : rows[(size_t) route]->depth.getBounds();
    }

    // Test hook: is `route`'s row currently painted dimmed because its
    // destination is inert right now (see isModDestIndexInert)? Reads the
    // combo's live selection exactly like paintInertRows does.
    bool isRouteDimmedForTest (int route) const
    {
        if (route < 0 || route >= (int) rows.size())
            return false;
        const int destIndex = rows[(size_t) route]->dest.getSelectedItemIndex() - 1;
        return isModDestIndexInert (apvts, destIndex);
    }

    void paint (juce::Graphics& g) override
    {
        draw::panel (g, getLocalBounds().toFloat());
        draw::sectionHeader (g, getLocalBounds(), "Mod Matrix", {},
                             currentTheme().accentMod);
    }

    void resized() override
    {
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
    // A juce::ComboBox that remembers, one-shot, whether ITS OWN dropdown was
    // just opened by a real user gesture. showPopup() is virtual and, per
    // JUCE's own ComboBox source, is reachable ONLY from mouseDown/mouseUp/
    // mouseDrag, keyPressed (Return/arrow-open), showPopupIfNotActive(), and
    // the accessibility press/showMenu actions -- every one of them a live
    // user action. Nothing in ComboBoxParameterAttachment's programmatic
    // sync path (setSelectedItemIndex(), called from setValue() whenever the
    // underlying parameter changes -- preset load, host restore, reset,
    // randomize) ever calls showPopup(); it writes the selection directly.
    // So consumeUserGesture() answers "did a person just pick something from
    // this box's own popup" with no false positives from any programmatic
    // write, however that write reaches the box (sync or the attachment's
    // AsyncUpdater). A plain juce::ComboBox::Listener can't make this
    // distinction on its own -- both paths end up calling the exact same
    // comboBoxChanged() -- which is why this subclass exists.
    //
    // IMPORTANT: a pending gesture must not outlive its popup. showPopup()
    // alone is not enough -- opening a dropdown, looking at it, and
    // dismissing it with Esc or an outside click (no selection at all) is
    // completely ordinary use, and juce::ComboBox gives no notification for
    // that case (comboBoxChanged only ever fires from a real setSelectedId,
    // which a cancelled popup never calls). Left unguarded, the flag would
    // sit there indefinitely; the NEXT time this exact box is synced
    // programmatically for any reason (a preset load landing on this row,
    // say) it would be misread as a fresh user pick and silently rewrite
    // that preset's deliberately-zero Depth. hidePopup() -- called the
    // instant the popup closes, selection or not -- is not virtual, so it
    // can't be overridden directly; isPopupActive() (which hidePopup() is
    // what flips false) is public, so a short poll while the popup is open
    // is the only observable signal available. The poll interval is a
    // deliberate trade-off: generous enough that a genuine selection's own
    // (asynchronous) comboBoxChanged -- posted essentially the instant the
    // popup closes -- always has time to consume the flag first, so a real
    // pick is never missed; short enough that nothing else a person could
    // possibly do next (open the preset browser, click a preset) can land
    // inside the window. The failure mode if this margin were ever somehow
    // too tight is a missed auto-fill nudge (cosmetic), never a wrongly
    // rewritten value -- the guard only ever makes consumeUserGesture()
    // return false early, never true when it shouldn't.
    struct RouteComboBox : public juce::ComboBox, private juce::Timer
    {
        void showPopup() override
        {
            userGesturePending = true;
            startTimer (40);
            juce::ComboBox::showPopup();
        }

        bool consumeUserGesture()
        {
            return std::exchange (userGesturePending, false);
        }

        // Test hook only: sets exactly the flag a real showPopup() call
        // would, so a test can exercise the GestureGate path via
        // setSelectedId() without driving real PopupMenu mouse timing
        // headlessly (see MatrixPanel::simulateUserComboPick).
        void markUserGestureForTest() { userGesturePending = true; }

        // Test hook only: applies exactly what timerCallback() below does
        // the instant it observes the popup has closed -- lets a test
        // exercise "opened, then dismissed without picking anything"
        // deterministically and synchronously, the same way
        // MatrixPanel::simulateClick()/simulateDoubleClick() apply exactly
        // what a real click does without needing real popup-menu timing.
        void simulatePopupClosedWithoutSelectionForTest() { clearPendingGesture(); }

    private:
        void clearPendingGesture()
        {
            userGesturePending = false;
            stopTimer();
        }

        void timerCallback() override
        {
            if (! isPopupActive())
                clearPendingGesture();
        }

        bool userGesturePending = false;
    };

    // Thin juce::ComboBox::Listener that only forwards to onUserChange when
    // the box it watches reports a real user pick (see RouteComboBox above);
    // a programmatic sync of the same box still calls comboBoxChanged() (JUCE
    // notifies every listener on the box, not just the attachment's own) but
    // is silently dropped here because consumeUserGesture() returns false.
    struct GestureGate : public juce::ComboBox::Listener
    {
        RouteComboBox* box = nullptr;
        std::function<void()> onUserChange;

        void comboBoxChanged (juce::ComboBox*) override
        {
            if (box != nullptr && box->consumeUserGesture() && onUserChange)
                onUserChange();
        }
    };

    struct Row
    {
        RouteComboBox source, dest;
        juce::Slider depth;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> sourceAttachment,
                                                                                destAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> depthAttachment;
        GestureGate sourceGate, destGate;
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

    static constexpr int rowHeight = 25;

    // Scrolls the viewport the minimum amount needed to bring `route`'s row
    // fully into view -- a no-op if it's already visible, so a reveal of an
    // already-visible row doesn't yank the scroll position around.
    void scrollRouteIntoView (int route)
    {
        if (route < 0 || route >= (int) rows.size())
            return;
        const auto rowTop = route * rowHeight;
        const auto rowBottom = rowTop + rowHeight;
        const auto curY = viewport.getViewPositionY();
        const auto viewH = viewport.getViewHeight();
        if (rowTop < curY)
            viewport.setViewPosition (viewport.getViewPositionX(), rowTop);
        else if (rowBottom > curY + viewH)
            viewport.setViewPosition (viewport.getViewPositionX(), rowBottom - viewH);
    }

    // A translucent violet band (+ outline) behind each revealed row's
    // controls -- the small gaps `resized()` leaves between rows/columns
    // (Row bounds are `.reduced(2,3)` and there are 4px gutters between the
    // source/dest/depth columns) let it read clearly even though the combo
    // boxes paint their own opaque backgrounds over most of the row. Fixed
    // modAssignedColour() (violet), deliberately never Theme::assignGlow/
    // assignSelected (blue/yellow) -- see the class comment on
    // setRevealedRoutes for why this must read as "the same violet as the
    // knob", not as another ASSIGN-mode visual. Pure read of `revealedRoutes`
    // -- never mutates state (paint() must never do that).
    void paintRevealHighlights (juce::Graphics& g) const
    {
        if (revealedRoutes.empty())
            return;
        const auto colour = modAssignedColour();
        for (auto r : revealedRoutes)
        {
            if (r < 0 || r >= (int) rows.size())
                continue;
            const juce::Rectangle<int> rowBounds (0, r * rowHeight, content.getWidth(), rowHeight);
            g.setColour (colour.withAlpha (0.22f));
            g.fillRect (rowBounds);
            g.setColour (colour.withAlpha (0.85f));
            g.drawRect (rowBounds, 2);
        }
    }

    // Waiting-row highlight (see setWaitingRoute): amber, distinct from both
    // the reveal violet and the ASSIGN overlay's own blue/yellow. Originally
    // also carried a right-aligned "WAITING FOR SOURCE/DEST" label, but the
    // row has NO free space anywhere -- source/dest/depth (see resized())
    // pack its full width with only 4px gutters between them -- so that text
    // was painted straight across the depth slider (the slider handle showed
    // through it), which read as a glitch rather than a deliberate state.
    // Text placed inside either combo would equally overlap a real control.
    // Dropped in favour of exactly two elements that both sit clear of every
    // control: a translucent row wash + outline (painted in the background
    // pass, i.e. genuinely BEHIND the opaque combo/slider faces, not
    // fighting them for pixels) and a ring drawn just outside the specific
    // empty field's own bounds (expanded 2px, well inside the 4px column
    // gutters -- getWaitingRingBoundsForTest() below is the exact rectangle,
    // and a test asserts it never intersects the row's other controls at any
    // width). The ring alone already answers "which half" unambiguously.
    // Reads combo selections live (index 0 == None, see kNoneRouteChoiceIndex);
    // never mutates.
    void paintWaitingHighlight (juce::Graphics& g) const
    {
        if (waitingRoute < 0 || waitingRoute >= (int) rows.size())
            return;
        auto& row = *rows[(size_t) waitingRoute];
        const bool sourceEmpty = row.source.getSelectedItemIndex() <= 0;
        const bool destEmpty = row.dest.getSelectedItemIndex() <= 0;
        if (! sourceEmpty && ! destEmpty)
            return;   // completed since -- caller clears this, but paint must never assume timing

        const auto colour = currentTheme().assignWaiting;
        const juce::Rectangle<int> rowBounds (0, waitingRoute * rowHeight, content.getWidth(), rowHeight);
        g.setColour (colour.withAlpha (0.16f));
        g.fillRect (rowBounds);
        g.setColour (colour.withAlpha (0.9f));
        g.drawRect (rowBounds, 2);

        const auto ring = waitingRingBounds (waitingRoute);
        if (! ring.isEmpty())
        {
            g.setColour (colour);
            g.drawRoundedRectangle (ring.toFloat(), 4.0f, 2.0f);
        }
    }

    // The exact rectangle paintWaitingHighlight rings around the still-empty
    // field of `route` -- factored out so a test can assert it never
    // intersects the row's other controls without duplicating paint's own
    // geometry (and so it can never drift from what's actually drawn).
    // Empty if `route` isn't the waiting row or the row has since completed.
    juce::Rectangle<int> waitingRingBounds (int route) const
    {
        if (route < 0 || route >= (int) rows.size())
            return {};
        auto& row = *rows[(size_t) route];
        const bool sourceEmpty = row.source.getSelectedItemIndex() <= 0;
        const bool destEmpty = row.dest.getSelectedItemIndex() <= 0;
        if (! sourceEmpty && ! destEmpty)
            return {};
        auto& emptyBox = sourceEmpty ? row.source : row.dest;
        return emptyBox.getBounds().expanded (2);
    }

    // Dims any row whose DEST is currently inert (isModDestIndexInert) --
    // painted OVER the children (paintOverChildren), not in the background
    // pass, so the wash actually darkens the combo boxes/slider drawn on top
    // of it rather than just showing through the gaps between them.
    void paintInertRows (juce::Graphics& g) const
    {
        for (int r = 0; r < (int) rows.size(); ++r)
        {
            const int destIndex = rows[(size_t) r]->dest.getSelectedItemIndex() - 1;
            if (! isModDestIndexInert (apvts, destIndex))
                continue;
            const juce::Rectangle<int> rowBounds (0, r * rowHeight, content.getWidth(), rowHeight);
            g.setColour (currentTheme().background.withAlpha (0.6f));
            g.fillRect (rowBounds);
            g.setColour (currentTheme().textSecondary.withAlpha (0.85f));
            g.setFont (metrics::smallFont());
            g.drawText ("INERT WHILE SYNCED", rowBounds.reduced (4, 0), juce::Justification::centredRight);
        }
    }

    // Message-thread-deferred repaint trigger for the inert-dim wash -- see
    // the constructor comment on why this can't touch `target` directly from
    // parameterChanged().
    struct InertGateWatcher : public juce::AudioProcessorValueTreeState::Listener,
                              private juce::AsyncUpdater
    {
        juce::Component* target = nullptr;

    private:
        void parameterChanged (const juce::String&, float) override { triggerAsyncUpdate(); }
        void handleAsyncUpdate() override { if (target != nullptr) target->repaint(); }
    };

    // Nested (not a lambda/std::function member) so paint() has zero extra
    // indirection cost at 30+ fps; a nested class is a member of MatrixPanel
    // for access purposes (C++11+), so it can reach paintRevealHighlights()
    // directly via `owner`.
    struct RowsHost : public juce::Component
    {
        MatrixPanel* owner = nullptr;
        void paint (juce::Graphics& g) override
        {
            owner->paintRevealHighlights (g);
            owner->paintWaitingHighlight (g);
        }
        void paintOverChildren (juce::Graphics& g) override { owner->paintInertRows (g); }
    };

    juce::AudioProcessorValueTreeState& apvts;
    RowsHost content;
    juce::Viewport viewport;
    std::vector<std::unique_ptr<Row>> rows;
    AssignButton assignBtn { "ASSIGN" };
    AssignMode assignMode = AssignMode::off;
    std::set<int> revealedRoutes;
    int waitingRoute = -1;
    InertGateWatcher inertWatcher;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MatrixPanel)
};

} // namespace spa::ui
