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
    void updateSharedState (int blockLength);
    void scanMidiControllers (const juce::MidiBuffer& midi);
    void installTable (int slot, std::shared_ptr<const dsp::Wavetable> table,
                       juce::String path, juce::String error);
    void timerCallback() override;

    juce::AudioProcessorValueTreeState apvts;

    dsp::SharedState shared;   // written on audio thread, read by voices
    juce::Synthesiser synth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> masterGain;

    double currentSampleRate = 44100.0;
    float lastModWheel = 0.0f;
    float lastAftertouch = 0.0f;
    std::array<double, params::numLFOs> lfoPhaseAccum {};  // free-running LFO phases

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
        std::atomic<float>* phase = nullptr;
        std::atomic<float>* phaseMode = nullptr;
        std::atomic<float>* unisonCount = nullptr;
    };

    struct RawLFO
    {
        std::atomic<float>* shape = nullptr;
        std::atomic<float>* rate = nullptr;
        std::atomic<float>* sync = nullptr;
        std::atomic<float>* division = nullptr;
        std::atomic<float>* phase = nullptr;
        std::atomic<float>* retrig = nullptr;
        std::atomic<float>* unipolar = nullptr;
    };

    struct RawRoute
    {
        std::atomic<float>* source = nullptr;
        std::atomic<float>* dest = nullptr;
        std::atomic<float>* depth = nullptr;
    };

    struct Raw
    {
        std::atomic<float>* masterGain = nullptr;
        std::atomic<float>* filterType = nullptr;
        std::array<RawSlot, params::numOscSlots> slots {};
        std::array<RawLFO, params::numLFOs> lfos {};
        std::array<std::atomic<float>*, params::numMacros> macros {};
        std::array<RawRoute, params::numModRoutes> routes {};
        // One pointer per mod destination, in dense mod-dest index order.
        std::vector<std::atomic<float>*> dests;
    } raw;

    static constexpr int numVoices = 16;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArsenalProcessor)
};

} // namespace arsenal
