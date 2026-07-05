#pragma once

#include "Theme.h"
#include "../params/ParameterRegistry.h"

namespace arsenal
{
class ArsenalProcessor;

namespace ui
{

// Base for the module scopes: watches a set of parameters and repaints (at a
// throttled rate) when any of them move. Subclasses implement paintDisplay().
class DisplayComponent : public juce::Component,
                         private juce::Timer,
                         private juce::AudioProcessorValueTreeState::Listener
{
public:
    DisplayComponent (juce::AudioProcessorValueTreeState&, juce::StringArray paramIDs);
    ~DisplayComponent() override;

    void paint (juce::Graphics&) final;
    void markDirty() { dirty.store (true); }

protected:
    virtual void paintDisplay (juce::Graphics&, juce::Rectangle<float>) = 0;

    juce::AudioProcessorValueTreeState& apvts;
    float value (const juce::String& paramID) const;   // real-world value

private:
    void parameterChanged (const juce::String&, float) override { dirty.store (true); }
    void timerCallback() override;

    juce::StringArray watched;
    std::atomic<bool> dirty { true };
};

// Oscillator scope: wavetable frame at the current position, or the loaded
// sample's waveform overview with a grain-position marker in granular mode.
class WaveDisplay : public DisplayComponent,
                    private juce::ChangeListener
{
public:
    WaveDisplay (ArsenalProcessor&, int slot);
    ~WaveDisplay() override;

private:
    void paintDisplay (juce::Graphics&, juce::Rectangle<float>) override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    ArsenalProcessor& processor;
    const int slot;
};

// ADSR curve with translucent fill.
class EnvDisplay : public DisplayComponent
{
public:
    EnvDisplay (juce::AudioProcessorValueTreeState&, juce::String idPrefix);

private:
    void paintDisplay (juce::Graphics&, juce::Rectangle<float>) override;
    juce::String prefix;
};

// One cycle of the LFO shape with the phase offset applied.
class LFODisplay : public DisplayComponent
{
public:
    LFODisplay (juce::AudioProcessorValueTreeState&, int lfoIndex);

private:
    void paintDisplay (juce::Graphics&, juce::Rectangle<float>) override;
    const int lfo;
};

// Approximate filter magnitude response.
class FilterDisplay : public DisplayComponent
{
public:
    explicit FilterDisplay (juce::AudioProcessorValueTreeState&);

private:
    void paintDisplay (juce::Graphics&, juce::Rectangle<float>) override;
};

// A representative chaos walk: regenerates deterministically from
// depth/rate/mix so the display "feels" like the settings.
class ChaosDisplay : public DisplayComponent
{
public:
    explicit ChaosDisplay (juce::AudioProcessorValueTreeState&);

private:
    void paintDisplay (juce::Graphics&, juce::Rectangle<float>) override;
};

} // namespace ui
} // namespace arsenal
