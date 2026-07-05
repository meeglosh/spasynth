#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "params/ParameterRegistry.h"
#include "dsp/ArsenalVoice.h"

namespace arsenal
{

class ArsenalProcessor : public juce::AudioProcessor
{
public:
    ArsenalProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Arsenal"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }

private:
    void updateVoiceParams();

    juce::AudioProcessorValueTreeState apvts;

    dsp::Wavetable wavetable;
    dsp::VoiceParams voiceParams;   // written on audio thread, read by voices
    juce::Synthesiser synth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> masterGain;

    // Cached raw parameter pointers (atomic floats owned by the APVTS).
    struct Raw
    {
        std::atomic<float>* masterGain = nullptr;
        std::atomic<float>* oscPosition = nullptr;
        std::atomic<float>* oscCoarse = nullptr;
        std::atomic<float>* oscFine = nullptr;
        std::atomic<float>* oscLevel = nullptr;
        std::atomic<float>* oscPan = nullptr;
        std::atomic<float>* filterType = nullptr;
        std::atomic<float>* filterCutoff = nullptr;
        std::atomic<float>* filterResonance = nullptr;
        std::atomic<float>* filterDrive = nullptr;
        std::atomic<float>* ampAttack = nullptr;
        std::atomic<float>* ampDecay = nullptr;
        std::atomic<float>* ampSustain = nullptr;
        std::atomic<float>* ampRelease = nullptr;
    } raw;

    static constexpr int numVoices = 16;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArsenalProcessor)
};

} // namespace arsenal
