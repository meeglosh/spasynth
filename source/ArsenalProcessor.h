#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "params/ParameterRegistry.h"
#include "dsp/ArsenalVoice.h"

namespace arsenal
{

class ArsenalProcessor : public juce::AudioProcessor,
                         public juce::ChangeBroadcaster,
                         private juce::Timer
{
public:
    ArsenalProcessor();
    ~ArsenalProcessor() override;

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

    // Wavetable slot management (message thread). Loads run on a background
    // thread; ChangeBroadcaster fires when a slot's table changes.
    void loadWavetableFromFile (int slot, const juce::File& file);
    void setFactoryWavetable (int slot);
    juce::String getWavetableName (int slot) const;
    juce::String getWavetableError (int slot) const;

private:
    void updateVoiceParams();
    void installTable (int slot, std::shared_ptr<const dsp::Wavetable> table,
                       juce::String path, juce::String error);
    void timerCallback() override;

    juce::AudioProcessorValueTreeState apvts;

    dsp::VoiceParams voiceParams;   // written on audio thread, read by voices
    juce::Synthesiser synth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> masterGain;

    // --- Wavetable storage -------------------------------------------------
    // Audio thread reads `live` each block; message thread owns `current` and
    // retires old tables, which are freed on a timer well after the audio
    // thread has moved on.
    struct SlotTable
    {
        std::shared_ptr<const dsp::Wavetable> current;       // message thread
        std::atomic<const dsp::Wavetable*> live { nullptr }; // audio thread
        juce::String path;                                    // "" = factory
        juce::String error;
    };
    std::shared_ptr<const dsp::Wavetable> factoryTable;
    std::array<SlotTable, params::maxOscSlots> slotTables;
    std::vector<std::shared_ptr<const dsp::Wavetable>> retiredTables;  // message thread

    // Cached raw parameter pointers (atomic floats owned by the APVTS).
    struct RawSlot
    {
        std::atomic<float>* enable = nullptr;
        std::atomic<float>* position = nullptr;
        std::atomic<float>* coarse = nullptr;
        std::atomic<float>* fine = nullptr;
        std::atomic<float>* level = nullptr;
        std::atomic<float>* pan = nullptr;
        std::atomic<float>* phase = nullptr;
        std::atomic<float>* phaseMode = nullptr;
        std::atomic<float>* unisonCount = nullptr;
        std::atomic<float>* unisonDetune = nullptr;
        std::atomic<float>* unisonBlend = nullptr;
        std::atomic<float>* unisonWidth = nullptr;
    };

    struct Raw
    {
        std::atomic<float>* masterGain = nullptr;
        std::array<RawSlot, params::numOscSlots> slots {};
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
