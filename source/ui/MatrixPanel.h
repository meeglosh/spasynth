#pragma once

#include "Theme.h"
#include "../params/ParameterRegistry.h"
#include <set>
#include <utility>
#include <array>
#include <vector>
#include <functional>

namespace spa::ui
{

// Declared here (not included from AssignOverlay.h, which includes THIS
// header) to avoid a header cycle -- same signature, defined once in
// SPASynthEditor.cpp. See AssignOverlay.h's declaration comment for why
// every popup must go through this rather than PopupMenu::showMenuAsync.
void showPopupAnchored (juce::Component& anchor, juce::PopupMenu& menu,
                        const juce::PopupMenu::Options& options,
                        std::function<void (int)> callback);

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
    // oneShot  : entered by a single click from off; self-exits the instant
    //            THIS session's own write happens (one destination or one
    //            source pick), regardless of whether the row it touched is
    //            now fully populated -- see AssignOverlay's onOneShotComplete.
    //            A SOURCE-only (or DEST-only) session inherently leaves the
    //            row half-filled, by design (Phil's follow-up request), so
    //            "the row completed" can no longer be the exit condition.
    // latched  : entered by a double click from off (or by promotion, see
    //            nextModeOnDoubleClick); behaves like the old ASSIGN mode,
    //            staying on through any number of assignments until another
    //            click or Esc.
    enum class AssignMode { off, oneShot, latched };

    // Which half of the matrix this ASSIGN session targets (Phil's tester
    // request: two separate single-purpose buttons instead of one that
    // arms both a destination pick AND a source pick at once, which read as
    // two competing invitations at the moment the user most needed to know
    // what to click next).
    enum class AssignKind { source, dest };

    // Bundles mode + kind so the pure state-machine steps below can return
    // both at once (a kind switch can change the mode too -- see
    // nextModeOnClick's comment).
    struct AssignState { AssignMode mode; AssignKind kind; };

    // Pure state-machine steps, exposed so tests can drive them directly
    // instead of depending on real mouse double-click timing.
    //
    // Single click on the button of kind `clicked`, per the product-owner
    // spec:
    //  - mode == off              -> oneShot, kind = clicked (arm that half)
    //  - mode != off, kind != K   -> oneShot, kind = clicked (SWITCH to the
    //                                other half -- clicking DEST while a
    //                                SOURCE session is live does not turn
    //                                assign off, it re-arms for DEST)
    //  - mode != off, kind == K   -> off (clicking the ALREADY-active
    //                                button's own kind again turns it off)
    static AssignState nextStateOnClick (AssignState current, AssignKind clicked)
    {
        if (current.mode == AssignMode::off)
            return { AssignMode::oneShot, clicked };
        if (current.kind != clicked)
            return { AssignMode::oneShot, clicked };
        return { AssignMode::off, current.kind };
    }

    // A genuine double click is, at the JUCE level, two ordinary clicks
    // followed by a mouseDoubleClick callback (Component::internalMouseUp:
    // the second click's mouseUp -- which fires its own click/onClick --
    // runs BEFORE mouseDoubleClick). So by the time this runs,
    // nextStateOnClick has already been applied twice by the two clicks
    // themselves -- `afterClick` is that already-applied, and now stale,
    // result.
    //
    // Product-owner rule: a double click ALWAYS means latch THAT BUTTON'S
    // kind, unconditionally, regardless of what mode/kind the two individual
    // clicks landed on -- "the same way caps lock does not care what the
    // shift key was doing". So this ignores `afterClick` entirely and always
    // returns { latched, clicked }; the parameter stays for symmetry with
    // nextStateOnClick and because a real double click's mouseDoubleClick
    // callback naturally has "the state after click 2" as its input, even
    // though this function doesn't need it.
    static AssignState nextStateOnDoubleClick (AssignState /*afterClick*/, AssignKind clicked)
    {
        return { AssignMode::latched, clicked };
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
            row->source.addListener (&row->sourceGate);

            row->destGate.box = &row->dest;
            row->dest.addListener (&row->destGate);

            // Empty-row compaction (Mike's request): a combo pick that
            // leaves the row fully empty pulls later rows up. Wired after
            // the auto-fill-depth lambdas above so a row that goes EMPTY
            // (auto-fill is a no-op there) still gets checked -- both fire
            // on the same real user gesture.
            row->sourceGate.onUserChange = [this, r] { maybeAutoFillRouteDepth (apvts, r); maybeCompactAfterUserEdit (r); };
            row->destGate.onUserChange = [this, r] { maybeAutoFillRouteDepth (apvts, r); maybeCompactAfterUserEdit (r); };

            row->grip.owner = this;
            row->grip.rowIndex = r;
            content.addAndMakeVisible (row->grip);
            content.addAndMakeVisible (row->source);
            content.addAndMakeVisible (row->dest);
            content.addAndMakeVisible (row->depth);
            rows.push_back (std::move (row));
        }

        viewport.setViewedComponent (&content, false);
        viewport.setScrollBarsShown (true, false);
        addAndMakeVisible (viewport);

        // ASSIGN toggles: click-to-route mode (see AssignOverlay), one button
        // per half of the matrix (Phil's tester request -- see AssignKind).
        // Styled like any other header text button; focus-grab disabled per
        // the QWERTY rule (Controls.h's Knob comment has the full
        // explanation). Toggle state is driven entirely by our own mode
        // state machine (applyMode), not JUCE's built-in click-toggles-state,
        // so it can't fight the double-click promotion logic -- see
        // nextStateOnDoubleClick.
        assignSourceBtn.setClickingTogglesState (false);
        assignSourceBtn.setWantsKeyboardFocus (false);
        assignSourceBtn.setMouseClickGrabsKeyboardFocus (false);
        assignSourceBtn.setComponentID ("matrixAssignSource");
        assignSourceBtn.onClick = [this] { applyMode (nextStateOnClick ({ assignMode, assignKind }, AssignKind::source)); };
        assignSourceBtn.onDoubleClick = [this] { applyMode (nextStateOnDoubleClick ({ assignMode, assignKind }, AssignKind::source)); };
        addAndMakeVisible (assignSourceBtn);

        assignDestBtn.setClickingTogglesState (false);
        assignDestBtn.setWantsKeyboardFocus (false);
        assignDestBtn.setMouseClickGrabsKeyboardFocus (false);
        assignDestBtn.setComponentID ("matrixAssignDest");
        assignDestBtn.onClick = [this] { applyMode (nextStateOnClick ({ assignMode, assignKind }, AssignKind::dest)); };
        assignDestBtn.onDoubleClick = [this] { applyMode (nextStateOnDoubleClick ({ assignMode, assignKind }, AssignKind::dest)); };
        addAndMakeVisible (assignDestBtn);

        // Repaint the matrix whenever anything the dim wash (paintInertRows)
        // reads might have changed: an inert-destination rule (currently
        // just chaos SYNC) OR any route's own source/dest choice --
        // rowMissingEitherHalf reads those live too. Tester report (Paul):
        // picking both halves of a row from its dropdowns left it looking
        // greyed out until hovering over it -- content (RowsHost) is what
        // paints the wash in paintOverChildren, but nothing ever called
        // content.repaint() when a route's own source/dest changed, only
        // when chaos::syncToBpm did; a combo repainting ITSELF (to show its
        // new selected text) doesn't repaint the wash layered on top of it.
        // Listening on the APVTS param directly (every route, both halves)
        // rather than only each combo's onChange/GestureGate means preset
        // load, host automation, undo and ASSIGN-mode writes -- none of
        // which go through a combo's own user-gesture path -- repaint too.
        // AudioProcessorValueTreeState::Listener callbacks can fire from the
        // audio thread (host automation), so this defers the actual repaint
        // via AsyncUpdater -- same idiom as ChaosPanel/DependentEnable.
        inertWatcher.target = &content;
        inertWatcher.repaintCounterForTest = &contentRepaintCountForTest;
        apvts.addParameterListener (params::id::chaos::syncToBpm, &inertWatcher);
        for (int r = 0; r < params::numModRoutes; ++r)
        {
            apvts.addParameterListener (params::id::routeParam (r, params::id::route::source), &inertWatcher);
            apvts.addParameterListener (params::id::routeParam (r, params::id::route::dest), &inertWatcher);
        }
    }

    ~MatrixPanel() override
    {
        apvts.removeParameterListener (params::id::chaos::syncToBpm, &inertWatcher);
        for (int r = 0; r < params::numModRoutes; ++r)
        {
            apvts.removeParameterListener (params::id::routeParam (r, params::id::route::source), &inertWatcher);
            apvts.removeParameterListener (params::id::routeParam (r, params::id::route::dest), &inertWatcher);
        }
    }

    juce::Button& assignSourceButton() { return assignSourceBtn; }
    juce::Button& assignDestButton() { return assignDestBtn; }
    // Union of both ASSIGN buttons' bounds, for the overlay's click
    // exclusion (see ContentComponent's wiring). Safe to treat as a single
    // rectangle: the two buttons sit adjacent in the header with nothing
    // else between them, so their union contains no other component's area.
    juce::Rectangle<int> assignButtonsBounds() const
    {
        return assignSourceBtn.getBounds().getUnion (assignDestBtn.getBounds());
    }
    bool isAssignOn() const { return assignMode != AssignMode::off; }
    AssignMode getAssignMode() const { return assignMode; }
    AssignKind getAssignKind() const { return assignKind; }
    // Sets the toggle and fires onAssignToggled if the state actually
    // changed (used by ContentComponent::keyPressed's Esc handling and by
    // AssignOverlay::onOneShotComplete's self-exit). Only off/one-shot are
    // meaningful entry points here (nothing currently forces latch on
    // programmatically). Turning OFF keeps whatever kind was already active
    // (the `kind` argument only matters when turning on); turning ON with
    // the session already on the same kind is a no-op.
    void setAssignOn (bool on, AssignKind kind = AssignKind::dest)
    {
        if (on)
        {
            if (isAssignOn() && assignKind == kind)
                return;
            applyMode ({ AssignMode::oneShot, kind });
        }
        else
        {
            if (! isAssignOn())
                return;
            applyMode ({ AssignMode::off, assignKind });
        }
    }

    std::function<void (AssignMode, AssignKind)> onAssignToggled;

    // Test hooks: apply exactly what a real single/double click on the
    // button of kind `kind` does, without needing real mouse-event timing
    // (see the class comment on nextStateOnDoubleClick for why a double
    // click is three separate applyMode calls -- click 1's onClick, click
    // 2's onClick, then mouseDoubleClick -- and simulateDoubleClick
    // reproduces all three from whatever the current state is, exactly like
    // AssignButton would).
    void simulateClick (AssignKind kind) { applyMode (nextStateOnClick ({ assignMode, assignKind }, kind)); }
    void simulateDoubleClick (AssignKind kind)
    {
        applyMode (nextStateOnClick ({ assignMode, assignKind }, kind));
        applyMode (nextStateOnClick ({ assignMode, assignKind }, kind));
        applyMode (nextStateOnDoubleClick ({ assignMode, assignKind }, kind));
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

    // Test hook: is `route`'s row currently painted dimmed -- either because
    // its destination is inert right now (see isModDestIndexInert) or
    // because the row is missing either half (source or dest still "None").
    // The latter replaced the old "waiting for its other half" callout
    // (Phil's follow-up request: don't chase the user about a half-filled
    // row, just show it quietly as not-live-yet, same visual language as an
    // inert destination) -- see paintInertRows. Reads the combos' live
    // selection exactly like paintInertRows does.
    bool isRouteDimmedForTest (int route) const
    {
        if (route < 0 || route >= (int) rows.size())
            return false;
        auto& row = *rows[(size_t) route];
        const int destIndex = row.dest.getSelectedItemIndex() - 1;
        return isModDestIndexInert (apvts, destIndex) || rowMissingEitherHalf (row);
    }

    // Test hook: how many times the deferred dim-wash repaint (InertGateWatcher's
    // handleAsyncUpdate, the thing that actually calls content.repaint()) has
    // fired since construction. isRouteDimmedForTest alone can't catch a
    // stale-paint regression -- it re-reads the combos' live selection every
    // time it's called, so it always reports the CORRECT answer regardless of
    // whether the on-screen wash was ever told to redraw. This counter
    // answers the separate question a test actually needs: was the component
    // that paints the wash ever asked to repaint after the change.
    int getContentRepaintCountForTest() const { return contentRepaintCountForTest; }

    void paint (juce::Graphics& g) override
    {
        draw::panel (g, getLocalBounds().toFloat());
        draw::sectionHeader (g, getLocalBounds(), "Mod Matrix", {},
                             currentTheme().accentMod);
    }

    void resized() override
    {
        {
            // Two buttons now share the space the single ASSIGN button used
            // to have; DEST first (rightmost) since it's the more commonly
            // used half, SOURCE to its left. See the class comment on
            // AssignKind for why the header no longer has room for the
            // "Mod Matrix" title to be threatened by this -- both buttons
            // stay clear of it, same as the old single button did.
            auto headerArea = getLocalBounds().removeFromTop (metrics::sectionHeaderHeight);
            assignDestBtn.setBounds (headerArea.removeFromRight (56).reduced (5, 5));
            assignSourceBtn.setBounds (headerArea.removeFromRight (56).reduced (5, 5));
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
            row->grip.setBounds (r.removeFromLeft (rowGripWidth));
            r.removeFromLeft (2);
            // Percentages now apply to the width LEFT after the grip, not
            // the full contentWidth, so the combos keep the same relative
            // 30/38/rest split rather than being squeezed by a fixed amount
            // subtracted from an unchanged split (comboTextFitsCellTest
            // measures other combos, not these, but the ratio is kept
            // deliberately consistent with the pre-grip layout anyway).
            const auto remaining = r.getWidth();
            row->source.setBounds (r.removeFromLeft (remaining * 30 / 100));
            r.removeFromLeft (4);
            row->dest.setBounds (r.removeFromLeft (remaining * 38 / 100));
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

    // Grip-dots handle at the left of a row (visual language copied from
    // DraggableTabButton -- see SPASynthLookAndFeel.cpp's comment on
    // tabGripReserve). Left-drag vertically to reorder the row; right-click
    // opens the "Clear row" menu (see MatrixPanel::showRowMenu). Both are
    // routed through the owning MatrixPanel since a single grip only knows
    // its own row index -- the actual parameter moves need every row.
    struct RowGrip : public juce::Component
    {
        MatrixPanel* owner = nullptr;
        int rowIndex = -1;

        RowGrip()
        {
            // Same QWERTY-focus rule as every other clickable control in
            // this panel -- see Controls.h's Knob for the full explanation.
            setWantsKeyboardFocus (false);
            setMouseClickGrabsKeyboardFocus (false);
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (owner == nullptr)
                return;
            if (e.mods.isPopupMenu())
                owner->showRowMenu (rowIndex, *this);
            else
                owner->beginRowDrag (rowIndex);
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (owner == nullptr || e.mods.isPopupMenu())
                return;
            auto* parent = getParentComponent();
            const auto p = parent != nullptr ? parent->getLocalPoint (this, e.position)
                                              : e.position;
            owner->updateRowDrag ((int) p.y);
        }

        void mouseUp (const juce::MouseEvent& e) override
        {
            if (owner != nullptr && ! e.mods.isPopupMenu())
                owner->endRowDrag();
        }

        void paint (juce::Graphics& g) override
        {
            const auto b = getLocalBounds().toFloat();
            const float x = b.getCentreX() - 1.5f;
            const float cy = b.getCentreY();
            constexpr float d = 1.5f, gap = 3.5f;
            g.setColour (currentTheme().textSecondary.withAlpha (isMouseOver() ? 0.75f : 0.45f));
            for (int col = 0; col < 2; ++col)
                for (int row = -1; row <= 1; ++row)
                    g.fillEllipse (x + (float) col * gap, cy + (float) row * gap - d * 0.5f, d, d);
        }
    };

    struct Row
    {
        RowGrip grip;
        RouteComboBox source, dest;
        juce::Slider depth;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> sourceAttachment,
                                                                                destAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> depthAttachment;
        GestureGate sourceGate, destGate;
    };

    // The three per-route parameter keys, in registry order -- see
    // params::id::route. Kept as one array so a move/clear/compact touches
    // whatever the registry defines, not a hardcoded pair, per Mike's
    // instruction to read the registry for the full set.
    static constexpr const char* routeKeys[] = { params::id::route::source,
                                                  params::id::route::dest,
                                                  params::id::route::depth };
    static constexpr int numRouteKeys = (int) (sizeof (routeKeys) / sizeof (routeKeys[0]));

    struct RouteSnapshot { std::array<float, numRouteKeys> norm {}; };

    RouteSnapshot readRouteSnapshot (int r) const
    {
        RouteSnapshot s;
        for (int k = 0; k < numRouteKeys; ++k)
        {
            auto* p = apvts.getParameter (params::id::routeParam (r, routeKeys[k]));
            s.norm[(size_t) k] = p != nullptr ? p->getValue() : 0.0f;
        }
        return s;
    }

    void writeRouteSnapshot (int r, const RouteSnapshot& s)
    {
        for (int k = 0; k < numRouteKeys; ++k)
        {
            auto* p = apvts.getParameter (params::id::routeParam (r, routeKeys[k]));
            if (p == nullptr)
                continue;
            p->beginChangeGesture();
            p->setValueNotifyingHost (s.norm[(size_t) k]);
            p->endChangeGesture();
        }
    }

    void copyRoute (int dst, int src) { writeRouteSnapshot (dst, readRouteSnapshot (src)); }

    bool isRouteEmpty (int r) const
    {
        auto* srcP = apvts.getParameter (params::id::routeParam (r, params::id::route::source));
        auto* dstP = apvts.getParameter (params::id::routeParam (r, params::id::route::dest));
        if (srcP == nullptr || dstP == nullptr)
            return true;
        const int s = (int) srcP->convertFrom0to1 (srcP->getValue());
        const int d = (int) dstP->convertFrom0to1 (dstP->getValue());
        return s == kNoneRouteChoiceIndex && d == kNoneRouteChoiceIndex;
    }

    // Writes the canonical blank route (both choices None, depth back to its
    // registry default of 0) -- used both by "Clear row" and by compaction's
    // tail cleanup.
    void clearRoute (int r)
    {
        for (int half = 0; half < 2; ++half)
        {
            const auto key = half == 0 ? params::id::route::source : params::id::route::dest;
            auto* p = apvts.getParameter (params::id::routeParam (r, key));
            if (p == nullptr)
                continue;
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 ((float) kNoneRouteChoiceIndex));
            p->endChangeGesture();
        }
        auto* depthP = apvts.getParameter (params::id::routeParam (r, params::id::route::depth));
        if (depthP != nullptr)
        {
            depthP->beginChangeGesture();
            depthP->setValueNotifyingHost (depthP->convertTo0to1 (0.0f));
            depthP->endChangeGesture();
        }
    }

    // Moves route `from`'s full parameter set (source/dest/depth, moved
    // together, never independently -- see the Row struct) to position `to`,
    // shifting whatever sits between up or down by one. Pure parameter
    // writes via setValueNotifyingHost with begin/endChangeGesture, exactly
    // like any other user edit, so presets/sessions/undo/host automation see
    // ordinary changes -- nothing here is DSP-aware, and the DSP sums all
    // 16 routes regardless of slot, so this can never change the sound
    // (routeOrderNeverChangesSoundTest pins that).
    void moveRoute (int from, int to)
    {
        if (from == to || from < 0 || to < 0
            || from >= params::numModRoutes || to >= params::numModRoutes)
            return;

        const auto saved = readRouteSnapshot (from);
        if (from < to)
            for (int i = from; i < to; ++i)
                copyRoute (i, i + 1);
        else
            for (int i = from; i > to; --i)
                copyRoute (i, i - 1);
        writeRouteSnapshot (to, saved);
        content.repaint();
    }

    // Called after any DIRECT user edit that might have just made row `r`
    // fully empty (a combo pick to None, or "Clear row") -- never from
    // preset/session load or host automation, which write via
    // apvts.replaceState()/setValueNotifyingHost() with no user gesture and
    // must leave a saved gap exactly where it was (Mike's explicit
    // requirement). Pulls every later non-empty row up to close the gap,
    // pushing the resulting empties to the bottom.
    void maybeCompactAfterUserEdit (int r)
    {
        if (! isRouteEmpty (r))
            return;

        std::vector<RouteSnapshot> nonEmpty;
        for (int i = r + 1; i < params::numModRoutes; ++i)
            if (! isRouteEmpty (i))
                nonEmpty.push_back (readRouteSnapshot (i));

        int pos = r;
        for (auto& snap : nonEmpty)
            writeRouteSnapshot (pos++, snap);
        for (int i = pos; i < params::numModRoutes; ++i)
            clearRoute (i);

        content.repaint();
    }

    void showRowMenu (int r, juce::Component& anchor)
    {
        juce::PopupMenu menu;
        menu.addItem ("Clear row", [this, r]
        {
            clearRoute (r);
            maybeCompactAfterUserEdit (r);
        });
        showPopupAnchored (anchor, menu, juce::PopupMenu::Options().withTargetComponent (&anchor), nullptr);
    }

    void beginRowDrag (int row)
    {
        dragging = true;
        dragFromRow = row;
        dragToRow = row;
        content.repaint();
    }

    void updateRowDrag (int contentY)
    {
        if (! dragging)
            return;
        const int target = juce::jlimit (0, (int) rows.size() - 1, contentY / rowHeight);
        if (target != dragToRow)
        {
            dragToRow = target;
            content.repaint();
        }
    }

    void endRowDrag()
    {
        if (! dragging)
            return;
        dragging = false;
        const auto from = dragFromRow, to = dragToRow;
        dragFromRow = dragToRow = -1;
        if (from != to)
            moveRoute (from, to);
        else
            content.repaint();
    }

    // Drop-indicator line at the boundary the dragged row would land on --
    // painted over children so it reads above the combo boxes/slider.
    void paintDragIndicator (juce::Graphics& g) const
    {
        if (! dragging || dragToRow < 0)
            return;
        const int boundary = dragToRow * rowHeight + (dragToRow > dragFromRow ? rowHeight : 0);
        g.setColour (currentTheme().accentMod);
        g.fillRect (0, boundary - 1, content.getWidth(), 2);
    }

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

    // Single place that changes the mode: keeps BOTH buttons' toggle state,
    // the "assignLatched" property the lock glyph reads (see
    // SPASynthLookAndFeel::drawButtonText -- gated on componentID prefix
    // "matrixAssign" so it fires for whichever button is latched), and the
    // outside world (via onAssignToggled) all in sync, so no caller can
    // leave a button visually on but functionally off or vice versa. Only
    // the active kind's button ever lights; the other is always cleared.
    void applyMode (AssignState newState)
    {
        assignMode = newState.mode;
        assignKind = newState.kind;
        const bool on = assignMode != AssignMode::off;
        const bool sourceOn = on && assignKind == AssignKind::source;
        const bool destOn = on && assignKind == AssignKind::dest;
        assignSourceBtn.setToggleState (sourceOn, juce::dontSendNotification);
        assignDestBtn.setToggleState (destOn, juce::dontSendNotification);
        assignSourceBtn.getProperties().set ("assignLatched", sourceOn && assignMode == AssignMode::latched);
        assignDestBtn.getProperties().set ("assignLatched", destOn && assignMode == AssignMode::latched);
        assignSourceBtn.repaint();
        assignDestBtn.repaint();
        if (onAssignToggled)
            onAssignToggled (assignMode, assignKind);
    }

    static constexpr int rowHeight = 25;
    static constexpr int rowGripWidth = 14;

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

    // Is `row` missing either half (source or dest still "None")? Shared by
    // isRouteDimmedForTest and paintInertRows -- see paintInertRows' comment
    // for why this replaced the old "waiting for its other half" callout.
    // Reads combo selections live (index 0 == None, see kNoneRouteChoiceIndex).
    static bool rowMissingEitherHalf (const Row& row)
    {
        return row.source.getSelectedItemIndex() <= 0 || row.dest.getSelectedItemIndex() <= 0;
    }

    // Dims any row whose DEST is currently inert (isModDestIndexInert), OR
    // that is simply not live yet because one half is still "None"
    // (rowMissingEitherHalf). Both cases use the same quiet dim wash --
    // painted OVER the children (paintOverChildren), not in the background
    // pass, so the wash actually darkens the combo boxes/slider drawn on top
    // of it rather than just showing through the gaps between them.
    //
    // Phil's follow-up tester request: don't complain that a row is
    // half-filled (the old "waiting for its other half" amber callout,
    // removed -- it fought the two-button model directly, since a
    // SOURCE-only or DEST-only session is now a deliberately valid, common
    // end state, not a state to chase the user out of). A plain, quiet
    // statement that the row isn't live yet is enough, in the same visual
    // language already used for an inert destination -- no highlight, no
    // colour call-out, nothing that draws the eye or nags. The
    // "INERT WHILE SYNCED" text itself is reserved for the genuinely-inert
    // case only; an incomplete-but-otherwise-fine row gets the wash with no
    // label (there is nothing actionable to say about it).
    void paintInertRows (juce::Graphics& g) const
    {
        for (int r = 0; r < (int) rows.size(); ++r)
        {
            auto& row = *rows[(size_t) r];
            const int destIndex = row.dest.getSelectedItemIndex() - 1;
            const bool inert = isModDestIndexInert (apvts, destIndex);
            const bool incomplete = rowMissingEitherHalf (row);
            if (! inert && ! incomplete)
                continue;
            const juce::Rectangle<int> rowBounds (0, r * rowHeight, content.getWidth(), rowHeight);
            g.setColour (currentTheme().background.withAlpha (0.6f));
            g.fillRect (rowBounds);
            if (inert)
            {
                g.setColour (currentTheme().textSecondary.withAlpha (0.85f));
                g.setFont (metrics::smallFont());
                g.drawText ("INERT WHILE SYNCED", rowBounds.reduced (4, 0), juce::Justification::centredRight);
            }
        }
    }

    // Message-thread-deferred repaint trigger for the inert-dim wash -- see
    // the constructor comment on why this can't touch `target` directly from
    // parameterChanged(). `repaintCounterForTest`, when set, is bumped every
    // time the deferred repaint actually fires, so a test can observe "was
    // the wash-painting component ever told to repaint" without relying on
    // isRouteDimmedForTest's live-state read (which can't distinguish a
    // correct live value from a stale one still on screen -- see
    // modMatrixDimWashRepaintTest).
    struct InertGateWatcher : public juce::AudioProcessorValueTreeState::Listener,
                              private juce::AsyncUpdater
    {
        juce::Component* target = nullptr;
        int* repaintCounterForTest = nullptr;

    private:
        void parameterChanged (const juce::String&, float) override { triggerAsyncUpdate(); }
        void handleAsyncUpdate() override
        {
            if (target != nullptr)
                target->repaint();
            if (repaintCounterForTest != nullptr)
                ++(*repaintCounterForTest);
        }
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
        }
        void paintOverChildren (juce::Graphics& g) override
        {
            owner->paintInertRows (g);
            owner->paintDragIndicator (g);
        }
    };

    juce::AudioProcessorValueTreeState& apvts;
    RowsHost content;
    juce::Viewport viewport;
    std::vector<std::unique_ptr<Row>> rows;
    AssignButton assignSourceBtn { "SOURCE" };
    AssignButton assignDestBtn { "DEST" };
    AssignMode assignMode = AssignMode::off;
    AssignKind assignKind = AssignKind::dest;
    std::set<int> revealedRoutes;
    InertGateWatcher inertWatcher;
    int contentRepaintCountForTest = 0;
    bool dragging = false;
    int dragFromRow = -1, dragToRow = -1;

public:
    // Test hooks for the drag-reorder feature -- exercise exactly what a
    // real grip drag does (moveRoute) or observe the live drag state,
    // without needing real mouse-drag timing (same rationale as
    // simulateClick/simulateUserComboPick above).
    void moveRouteForTest (int from, int to) { moveRoute (from, to); }
    void clearRouteForTest (int r) { clearRoute (r); maybeCompactAfterUserEdit (r); }
    bool isRouteEmptyForTest (int r) const { return isRouteEmpty (r); }
    juce::Component& getGripForTest (int route) const { return rows[(size_t) route]->grip; }
    void beginRowDragForTest (int row) { beginRowDrag (row); }
    void updateRowDragForTest (int contentY) { updateRowDrag (contentY); }
    void endRowDragForTest() { endRowDrag(); }
    bool isDraggingForTest() const { return dragging; }
    int getDragFromRowForTest() const { return dragFromRow; }
    int getDragToRowForTest() const { return dragToRow; }
    void showRowMenuForTest (int r) { showRowMenu (r, rows[(size_t) r]->grip); }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MatrixPanel)
};

} // namespace spa::ui
