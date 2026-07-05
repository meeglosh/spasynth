#include "Displays.h"
#include "../ArsenalProcessor.h"

namespace arsenal::ui
{

// ========================== DisplayComponent ===============================

DisplayComponent::DisplayComponent (juce::AudioProcessorValueTreeState& state,
                                    juce::StringArray paramIDs)
    : apvts (state), watched (std::move (paramIDs))
{
    setInterceptsMouseClicks (false, false);
    for (const auto& id : watched)
        apvts.addParameterListener (id, this);
    startTimerHz (20);
}

DisplayComponent::~DisplayComponent()
{
    for (const auto& id : watched)
        apvts.removeParameterListener (id, this);
}

float DisplayComponent::value (const juce::String& paramID) const
{
    auto* param = apvts.getParameter (paramID);
    return param != nullptr ? param->convertFrom0to1 (param->getValue()) : 0.0f;
}

void DisplayComponent::timerCallback()
{
    if (dirty.exchange (false))
        repaint();
}

void DisplayComponent::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    draw::displayWell (g, bounds);
    paintDisplay (g, bounds.reduced (3.0f));
}

// ============================ WaveDisplay ==================================

WaveDisplay::WaveDisplay (ArsenalProcessor& p, int slotIndex)
    : DisplayComponent (p.getAPVTS(),
                        { params::id::oscSlot (slotIndex, params::id::osc::mode),
                          params::id::oscSlot (slotIndex, params::id::osc::position),
                          params::id::oscSlot (slotIndex, params::id::osc::grainPos),
                          params::id::oscSlot (slotIndex, params::id::osc::sampleStart) }),
      processor (p), slot (slotIndex)
{
    processor.addChangeListener (this);
}

WaveDisplay::~WaveDisplay()
{
    processor.removeChangeListener (this);
}

void WaveDisplay::changeListenerCallback (juce::ChangeBroadcaster*)
{
    markDirty();
}

void WaveDisplay::paintDisplay (juce::Graphics& g, juce::Rectangle<float> area)
{
    const auto& t = currentTheme();
    const auto mode = (params::OscMode) (int) value (
        params::id::oscSlot (slot, params::id::osc::mode));

    if (mode == params::OscMode::wavetable)
    {
        const auto table = processor.getWavetable (slot);
        if (table == nullptr || table->getNumFrames() == 0)
            return;

        // Interpolated frame at the current position, mip 0.
        const auto position = value (params::id::oscSlot (slot, params::id::osc::position));
        const auto framePos = position * (float) (table->getNumFrames() - 1);
        const auto frameA = juce::jmin ((int) framePos, table->getNumFrames() - 1);
        const auto frameB = juce::jmin (frameA + 1, table->getNumFrames() - 1);
        const auto frac = framePos - (float) frameA;
        const auto* a = table->getFrame (0, frameA);
        const auto* b = table->getFrame (0, frameB);

        juce::Path wave;
        constexpr int steps = 128;
        for (int i = 0; i <= steps; ++i)
        {
            const auto idx = (int) ((float) i / steps * (dsp::Wavetable::tableSize - 1));
            const auto sample = a[idx] + frac * (b[idx] - a[idx]);
            const auto x = area.getX() + area.getWidth() * (float) i / steps;
            const auto y = area.getCentreY() - sample * area.getHeight() * 0.42f;
            if (i == 0)
                wave.startNewSubPath (x, y);
            else
                wave.lineTo (x, y);
        }

        draw::glowStroke (g, wave, t.accent);
        return;
    }

    // Sample / granular: waveform overview.
    const auto sample = processor.getSample (slot);
    if (sample == nullptr || sample->lengthSamples() < 2)
    {
        g.setColour (t.textSecondary.withAlpha (0.6f));
        g.setFont (metrics::labelFont());
        g.drawText ("no SFX loaded", area.toNearestInt(), juce::Justification::centred);
        return;
    }

    const auto* audio = sample->audio.getReadPointer (0);
    const auto numSamples = sample->lengthSamples();
    const auto columns = juce::jmax (32, (int) area.getWidth() / 2);

    juce::Path fill;
    fill.startNewSubPath (area.getX(), area.getCentreY());
    for (int c = 0; c < columns; ++c)
    {
        const auto start = (int) ((juce::int64) c * numSamples / columns);
        const auto end = (int) ((juce::int64) (c + 1) * numSamples / columns);
        float peak = 0.0f;
        const auto stride = juce::jmax (1, (end - start) / 64);
        for (int i = start; i < end; i += stride)
            peak = juce::jmax (peak, std::abs (audio[i]));

        const auto x = area.getX() + area.getWidth() * (float) c / (float) (columns - 1);
        fill.lineTo (x, area.getCentreY() - peak * area.getHeight() * 0.46f);
    }
    for (int c = columns - 1; c >= 0; --c)
    {
        const auto start = (int) ((juce::int64) c * numSamples / columns);
        const auto end = (int) ((juce::int64) (c + 1) * numSamples / columns);
        float peak = 0.0f;
        const auto stride = juce::jmax (1, (end - start) / 64);
        for (int i = start; i < end; i += stride)
            peak = juce::jmax (peak, std::abs (audio[i]));

        const auto x = area.getX() + area.getWidth() * (float) c / (float) (columns - 1);
        fill.lineTo (x, area.getCentreY() + peak * area.getHeight() * 0.46f);
    }
    fill.closeSubPath();

    g.setColour (t.accent.withAlpha (0.55f));
    g.fillPath (fill);

    // Position marker: grain position (granular) or start offset (sample).
    const auto markerParam = mode == params::OscMode::granular
                           ? params::id::osc::grainPos : params::id::osc::sampleStart;
    const auto marker = value (params::id::oscSlot (slot, markerParam));
    const auto mx = area.getX() + area.getWidth() * juce::jlimit (0.0f, 1.0f, marker);
    g.setColour (t.textPrimary);
    g.drawLine (mx, area.getY(), mx, area.getBottom(), 1.2f);
}

// ============================ EnvDisplay ===================================

EnvDisplay::EnvDisplay (juce::AudioProcessorValueTreeState& state, juce::String idPrefix)
    : DisplayComponent (state, { idPrefix + ".attack", idPrefix + ".decay",
                                 idPrefix + ".sustain", idPrefix + ".release" }),
      prefix (std::move (idPrefix))
{
}

void EnvDisplay::paintDisplay (juce::Graphics& g, juce::Rectangle<float> area)
{
    const auto& t = currentTheme();

    // Perceptual (sqrt) time scaling keeps short envelopes readable.
    const auto a = std::sqrt (value (prefix + ".attack") / 10.0f);
    const auto d = std::sqrt (value (prefix + ".decay") / 10.0f);
    const auto s = value (prefix + ".sustain");
    const auto r = std::sqrt (value (prefix + ".release") / 20.0f);
    const auto total = juce::jmax (0.05f, a + d + 0.25f + r);

    const auto xAt = [&] (float seg) { return area.getX() + area.getWidth() * seg / total; };
    const auto yAt = [&] (float level) { return area.getBottom() - level * area.getHeight() * 0.92f; };

    juce::Path curve;
    curve.startNewSubPath (area.getX(), yAt (0.0f));
    curve.quadraticTo (xAt (a * 0.4f), yAt (0.85f), xAt (a), yAt (1.0f));                 // attack
    curve.quadraticTo (xAt (a + d * 0.3f), yAt (s + (1.0f - s) * 0.25f), xAt (a + d), yAt (s));
    curve.lineTo (xAt (a + d + 0.25f), yAt (s));                                           // sustain
    curve.quadraticTo (xAt (a + d + 0.25f + r * 0.3f), yAt (s * 0.25f), area.getRight(), yAt (0.0f));

    auto fill = curve;
    fill.lineTo (area.getRight(), area.getBottom());
    fill.lineTo (area.getX(), area.getBottom());
    fill.closeSubPath();

    g.setColour (t.accentMod.withAlpha (0.18f));
    g.fillPath (fill);
    draw::glowStroke (g, curve, t.accentMod, 1.6f);
}

// ============================ LFODisplay ===================================

LFODisplay::LFODisplay (juce::AudioProcessorValueTreeState& state, int lfoIndex)
    : DisplayComponent (state, { params::id::lfoParam (lfoIndex, params::id::lfo::shape),
                                 params::id::lfoParam (lfoIndex, params::id::lfo::phase),
                                 params::id::lfoParam (lfoIndex, params::id::lfo::unipolar) }),
      lfo (lfoIndex)
{
}

void LFODisplay::paintDisplay (juce::Graphics& g, juce::Rectangle<float> area)
{
    const auto& t = currentTheme();
    const auto shape = (params::LFOShape) (int) value (
        params::id::lfoParam (lfo, params::id::lfo::shape));
    const auto phaseOffset = value (params::id::lfoParam (lfo, params::id::lfo::phase));
    const auto unipolar = value (params::id::lfoParam (lfo, params::id::lfo::unipolar)) >= 0.5f;

    juce::Random shRandom (42 + lfo);  // stable S&H preview
    float shValue = shRandom.nextFloat() * 2.0f - 1.0f;
    int shCycle = 0;

    juce::Path curve;
    constexpr int steps = 160;
    for (int i = 0; i <= steps; ++i)
    {
        const auto raw = (float) i / steps + phaseOffset;
        const auto ph = raw - std::floor (raw);

        float v = 0.0f;
        switch (shape)
        {
            case params::LFOShape::sine:     v = std::sin (juce::MathConstants<float>::twoPi * ph); break;
            case params::LFOShape::triangle: v = 1.0f - 4.0f * std::abs (ph - 0.5f); break;
            case params::LFOShape::sawUp:    v = 2.0f * ph - 1.0f; break;
            case params::LFOShape::sawDown:  v = 1.0f - 2.0f * ph; break;
            case params::LFOShape::square:   v = ph < 0.5f ? 1.0f : -1.0f; break;
            case params::LFOShape::sampleHold:
            {
                const auto cycle = (int) (raw * 6.0f);   // several steps across the view
                if (cycle != shCycle)
                {
                    shCycle = cycle;
                    shValue = shRandom.nextFloat() * 2.0f - 1.0f;
                }
                v = shValue;
                break;
            }
        }

        if (unipolar)
            v = v * 0.5f + 0.5f;

        const auto x = area.getX() + area.getWidth() * (float) i / steps;
        const auto y = area.getCentreY() - v * area.getHeight() * 0.42f;
        if (i == 0)
            curve.startNewSubPath (x, y);
        else
            curve.lineTo (x, y);
    }

    draw::glowStroke (g, curve, t.accentMod, 1.6f);
}

// =========================== FilterDisplay =================================

FilterDisplay::FilterDisplay (juce::AudioProcessorValueTreeState& state)
    : DisplayComponent (state, { params::id::filter1Type, params::id::filter1Cutoff,
                                 params::id::filter1Resonance })
{
}

void FilterDisplay::paintDisplay (juce::Graphics& g, juce::Rectangle<float> area)
{
    const auto& t = currentTheme();
    const auto type = (params::FilterType) (int) value (params::id::filter1Type);
    const auto cutoff = value (params::id::filter1Cutoff);
    const auto res = value (params::id::filter1Resonance);
    const auto q = 0.5f + res * 4.5f;

    const bool is24 = type == params::FilterType::lp24 || type == params::FilterType::hp24
                   || type == params::FilterType::bp24 || type == params::FilterType::notch24;

    juce::Path curve;
    constexpr int steps = 140;
    for (int i = 0; i <= steps; ++i)
    {
        // 20 Hz .. 20 kHz log axis.
        const auto freq = 20.0f * std::pow (1000.0f, (float) i / steps);
        const auto w = freq / juce::jmax (20.0f, cutoff);
        const auto w2 = w * w;
        const auto denom = std::sqrt ((1.0f - w2) * (1.0f - w2) + w2 / (q * q));

        float mag;
        switch (type)
        {
            case params::FilterType::lp12: case params::FilterType::lp24:
                mag = 1.0f / denom; break;
            case params::FilterType::hp12: case params::FilterType::hp24:
                mag = w2 / denom; break;
            case params::FilterType::bp12: case params::FilterType::bp24:
                mag = (w / q) / denom; break;
            case params::FilterType::notch12: case params::FilterType::notch24:
            default:
                mag = std::abs (1.0f - w2) / denom; break;
        }

        if (is24)
            mag *= mag;

        const auto dB = juce::jlimit (-36.0f, 18.0f,
                                      juce::Decibels::gainToDecibels (mag, -60.0f));
        const auto x = area.getX() + area.getWidth() * (float) i / steps;
        const auto y = juce::jmap (dB, -36.0f, 18.0f, area.getBottom(), area.getY());
        if (i == 0)
            curve.startNewSubPath (x, y);
        else
            curve.lineTo (x, y);
    }

    draw::glowStroke (g, curve, t.accent, 1.6f);
}

// ============================ ChaosDisplay =================================

ChaosDisplay::ChaosDisplay (juce::AudioProcessorValueTreeState& state)
    : DisplayComponent (state, { params::id::chaos::depth, params::id::chaos::rate,
                                 params::id::chaos::mix, params::id::chaos::enable })
{
}

void ChaosDisplay::paintDisplay (juce::Graphics& g, juce::Rectangle<float> area)
{
    const auto& t = currentTheme();
    const auto enabled = value (params::id::chaos::enable) >= 0.5f;
    const auto depth = value (params::id::chaos::depth);
    const auto mix = value (params::id::chaos::mix);
    const auto rateNorm = apvts.getParameter (params::id::chaos::rate)->getValue();

    // A representative walker trace, deterministic per settings.
    juce::Random rng (1234);
    const auto stepsPerView = 4.0f + rateNorm * 60.0f;
    float walker = 0.0f, target = 0.0f;
    float phase = 1.0f;

    juce::Path curve;
    constexpr int steps = 180;
    for (int i = 0; i <= steps; ++i)
    {
        phase += stepsPerView / steps;
        if (phase >= 1.0f)
        {
            phase -= std::floor (phase);
            target = rng.nextFloat() * 2.0f - 1.0f;
        }
        walker += (target - walker) * juce::jmin (1.0f, stepsPerView / steps * 4.0f);

        const auto amp = enabled ? depth * mix : 0.0f;
        const auto x = area.getX() + area.getWidth() * (float) i / steps;
        const auto y = area.getCentreY() - walker * amp * area.getHeight() * 0.45f;
        if (i == 0)
            curve.startNewSubPath (x, y);
        else
            curve.lineTo (x, y);
    }

    draw::glowStroke (g, curve, enabled ? t.accentMod : t.textSecondary.withAlpha (0.4f), 1.6f);
}

} // namespace arsenal::ui
