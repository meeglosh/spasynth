#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include "Controls.h"
#include "../dsp/Telemetry.h"
#include "../params/ParameterRegistry.h"
#include <cmath>

namespace spa::ui
{

// Scrolling limiter meter, a simplified Pro-L-style view: a centred output-level
// waveform with the gain reduction painted as an amber region descending from
// the top, both scrolling right to left in real time from the Telemetry ring.
class LimiterDisplay : public juce::Component,
                       private juce::Timer
{
public:
    explicit LimiterDisplay (const dsp::Telemetry& tel) : telemetry (tel)
    {
        setInterceptsMouseClicks (false, false);
        startTimerHz (30);
    }

    ~LimiterDisplay() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        const auto& t = currentTheme();
        const auto area = getLocalBounds().toFloat().reduced (1.0f);

        g.setColour (t.display);
        g.fillRoundedRectangle (area, 4.0f);

        const float cy = area.getCentreY();
        const float halfH = area.getHeight() * 0.46f;

        // Reference grid: centre line + gain-reduction ticks from the top.
        g.setColour (t.outline.withAlpha (0.5f));
        g.drawHorizontalLine ((int) cy, area.getX(), area.getRight());
        for (int db = 3; db <= 12; db += 3)
        {
            const float y = area.getY() + (float) db / grRange * area.getHeight() * 0.5f;
            g.setColour (t.outline.withAlpha (0.25f));
            g.drawHorizontalLine ((int) y, area.getX(), area.getRight());
        }

        const int N = dsp::Telemetry::limiterHistory;
        const int show = juce::jmin (N - 1, 480);
        const int wr = telemetry.limWrite.load (std::memory_order_acquire);
        const auto frameAt = [&] (int c)
        {
            int idx = (wr - show + c) % N;
            if (idx < 0) idx += N;
            return idx;
        };
        const auto xOf = [&] (int c)
        {
            return area.getX() + area.getWidth() * (float) c / (float) (show - 1);
        };

        // Output-level waveform (centred, filled).
        juce::Path top, bot;
        for (int c = 0; c < show; ++c)
        {
            const float lvl = juce::jlimit (0.0f, 1.0f,
                                            telemetry.limOut[(size_t) frameAt (c)].load (std::memory_order_relaxed));
            const float x = xOf (c);
            const float yt = cy - lvl * halfH, yb = cy + lvl * halfH;
            if (c == 0) { top.startNewSubPath (x, yt); bot.startNewSubPath (x, yb); }
            else        { top.lineTo (x, yt);          bot.lineTo (x, yb); }
        }
        juce::Path fill = top;
        for (int c = show - 1; c >= 0; --c)
        {
            const float lvl = juce::jlimit (0.0f, 1.0f,
                                            telemetry.limOut[(size_t) frameAt (c)].load (std::memory_order_relaxed));
            fill.lineTo (xOf (c), cy + lvl * halfH);
        }
        fill.closeSubPath();
        g.setColour (t.accent.withAlpha (0.22f));
        g.fillPath (fill);
        g.setColour (t.accent);
        g.strokePath (top, juce::PathStrokeType (1.2f));
        g.strokePath (bot, juce::PathStrokeType (1.2f));

        // Gain reduction: amber region from the top, depth proportional to GR.
        const juce::Colour amber (0xffe0a83a);
        juce::Path gr;
        gr.startNewSubPath (area.getX(), area.getY());
        float lastGr = 0.0f;
        for (int c = 0; c < show; ++c)
        {
            const float grDb = telemetry.limGrDb[(size_t) frameAt (c)].load (std::memory_order_relaxed);
            lastGr = grDb;
            const float amt = juce::jlimit (0.0f, 1.0f, -grDb / grRange);
            gr.lineTo (xOf (c), area.getY() + amt * area.getHeight() * 0.5f);
        }
        gr.lineTo (area.getRight(), area.getY());
        gr.closeSubPath();
        g.setColour (amber.withAlpha (0.45f));
        g.fillPath (gr);

        // Current GR readout.
        g.setColour (amber);
        g.setFont (juce::Font (juce::FontOptions (11.0f)));
        g.drawText (lastGr < -0.05f ? "GR " + juce::String (lastGr, 1) + " dB" : "GR 0.0 dB",
                    area.reduced (7.0f, 4.0f).removeFromTop (14.0f),
                    juce::Justification::topLeft);

        g.setColour (t.outline);
        g.drawRoundedRectangle (area, 4.0f, 1.0f);
    }

    void timerCallback() override { repaint(); }

private:
    static constexpr float grRange = 18.0f;   // dB shown from the top to the centre

    const dsp::Telemetry& telemetry;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LimiterDisplay)
};

// Limiter tab: a large scrolling meter over a compact control strip.
class LimiterPanel : public juce::Component
{
public:
    LimiterPanel (juce::AudioProcessorValueTreeState& apvts, const dsp::Telemetry& tel)
        : display (tel),
          enable  (apvts, params::id::fx::limEnable, "ON"),
          drive   (apvts, params::id::fx::limDrive, "Drive"),
          ceiling (apvts, params::id::fx::limCeiling, "Ceiling"),
          release (apvts, params::id::fx::limRelease, "Release"),
          link    (apvts, params::id::fx::limStereoLink, "Link"),
          character (apvts, params::id::fx::limCharacter),
          autoRel (apvts, params::id::fx::limAutoRelease, "AUTO"),
          truePeak (apvts, params::id::fx::limTruePeak, "TRUE PK"),
          lookahead (apvts, params::id::fx::limLookahead, "LOOK")
    {
        title.setText ("LIMITER", juce::dontSendNotification);
        title.setFont (metrics::sectionFont());
        title.setColour (juce::Label::textColourId, currentTheme().accent);
        addAndMakeVisible (title);
        addAndMakeVisible (display);
        for (auto* c : std::initializer_list<juce::Component*> {
                 &enable, &drive, &ceiling, &release, &link, &character,
                 &autoRel, &truePeak, &lookahead })
            addAndMakeVisible (*c);
    }

    void paint (juce::Graphics& g) override { g.fillAll (currentTheme().panel); }

    void resized() override
    {
        auto r = getLocalBounds().reduced (7, 5);
        title.setBounds (r.removeFromTop (16));
        r.removeFromTop (2);

        // Compact control strip along the bottom; the meter takes the rest so it
        // is as large as the tab allows.
        auto strip = r.removeFromBottom (58);
        display.setBounds (r.reduced (0, 2));

        enable.setBounds (strip.removeFromLeft (54).reduced (2, 20));
        strip.removeFromLeft (6);
        for (auto* k : { &drive, &ceiling, &release, &link })
        {
            k->setBounds (strip.removeFromLeft (58));
            strip.removeFromLeft (2);
        }
        strip.removeFromLeft (6);
        auto toggles = strip.removeFromRight (juce::jmin (240, strip.getWidth()));
        auto tRow = toggles.removeFromTop (toggles.getHeight() / 2 + 2);
        autoRel.setBounds (tRow.removeFromLeft (tRow.getWidth() / 2).reduced (2, 2));
        truePeak.setBounds (tRow.reduced (2, 2));
        lookahead.setBounds (toggles.removeFromLeft (toggles.getWidth() / 2).reduced (2, 2));
        character.setBounds (toggles.reduced (2, 3));
        // Any gap between the knobs and the toggles is intentional breathing room.
        juce::ignoreUnused (strip);
    }

private:
    juce::Label title;
    LimiterDisplay display;
    Toggle enable;
    Knob drive, ceiling, release, link;
    Choice character;
    Toggle autoRel, truePeak, lookahead;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LimiterPanel)
};

} // namespace spa::ui
