#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "params/ParameterRegistry.h"
#include "dsp/ArsenalVoice.h"
#include "dsp/FXChain.h"
#include "library/PresetManager.h"

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
    double getTailLengthSeconds() const override { return fxChain.tailSeconds (fxParams); }

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

    // Sample/SFX slot management — same threading contract as wavetables.
    void loadSampleFromFile (int slot, const juce::File& file);
    juce::String getSampleName (int slot) const;
    juce::String getSampleError (int slot) const;

    // RANDOMIZE ALL (message thread). Wildness and lock state live as state
    // properties so they persist with the session but stay non-automatable.
    void randomizeAll();
    float getRandomWildness() const;
    void setRandomWildness (float wildness);
    bool isLockGroupLocked (int group) const;
    void setLockGroupLocked (int group, bool locked);

    // Presets + library (message thread).
    library::PresetManager& getPresetManager() { return *presetManager; }

    // Full plugin state as a tree (params + portable wavetable/sample paths).
    // Shared by host chunks and the preset system.
    juce::ValueTree buildStateTree();
    void restoreStateTree (const juce::ValueTree&);

    // Rescans the configured library and (re)generates factory presets for
    // any packs that don't have them yet.
    void refreshLibrary();

private:
    void updateSharedState (int blockLength);
    void scanMidiControllers (const juce::MidiBuffer& midi);
    void installTable (int slot, std::shared_ptr<const dsp::Wavetable> table,
                       juce::String path, juce::String error);
    void installSample (int slot, std::shared_ptr<const dsp::SampleData> sample,
                        juce::String path, juce::String error);
    void timerCallback() override;

    juce::AudioProcessorValueTreeState apvts;

    dsp::SharedState shared;   // written on audio thread, read by voices
    juce::Synthesiser synth;
    dsp::FXChain fxChain;
    dsp::FXChain::Params fxParams;
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

    // Sample/SFX storage, same ownership scheme as wavetables.
    struct SlotSample
    {
        std::shared_ptr<const dsp::SampleData> current;       // message thread
        std::atomic<const dsp::SampleData*> live { nullptr }; // audio thread
        juce::String path;
        juce::String error;
    };
    std::array<SlotSample, params::maxOscSlots> slotSamples;
    std::vector<std::shared_ptr<const dsp::SampleData>> retiredSamples;  // message thread

    // Constructed last (captures the pristine default state).
    std::unique_ptr<library::PresetManager> presetManager;

    // Cached raw parameter pointers (atomic floats owned by the APVTS).
    struct RawSlot
    {
        std::atomic<float>* enable = nullptr;
        std::atomic<float>* mode = nullptr;
        std::atomic<float>* phase = nullptr;
        std::atomic<float>* phaseMode = nullptr;
        std::atomic<float>* unisonCount = nullptr;
        std::atomic<float>* sampleStart = nullptr;
        std::atomic<float>* loop = nullptr;
        std::atomic<float>* loopStart = nullptr;
        std::atomic<float>* loopEnd = nullptr;
        std::atomic<float>* keytrack = nullptr;
        std::atomic<float>* rootNote = nullptr;
        std::atomic<float>* grainPitch = nullptr;
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

    struct RawChaos
    {
        std::atomic<float>* enable = nullptr;
        std::atomic<float>* pitchOn = nullptr;
        std::atomic<float>* pitchAmount = nullptr;
        std::atomic<float>* phaseOn = nullptr;
        std::atomic<float>* phaseAmount = nullptr;
        std::atomic<float>* positionOn = nullptr;
        std::atomic<float>* positionAmount = nullptr;
        std::atomic<float>* ampOn = nullptr;
        std::atomic<float>* ampAmount = nullptr;
        std::atomic<float>* satOn = nullptr;
        std::atomic<float>* saturation = nullptr;
        std::atomic<float>* distOn = nullptr;
        std::atomic<float>* distortion = nullptr;
    };

    struct Raw
    {
        std::atomic<float>* masterGain = nullptr;
        std::atomic<float>* filterType = nullptr;
        RawChaos chaos {};
        std::array<RawSlot, params::numOscSlots> slots {};
        std::array<RawLFO, params::numLFOs> lfos {};
        std::array<std::atomic<float>*, params::numMacros> macros {};
        std::array<RawRoute, params::numModRoutes> routes {};
        // One pointer per mod destination, in dense mod-dest index order.
        std::vector<std::atomic<float>*> dests;

        struct RawFX
        {
            std::atomic<float>* distEnable = nullptr;
            std::atomic<float>* distType = nullptr;
            std::atomic<float>* distDrive = nullptr;
            std::atomic<float>* distTone = nullptr;
            std::atomic<float>* distMix = nullptr;
            std::atomic<float>* chorusEnable = nullptr;
            std::atomic<float>* chorusRate = nullptr;
            std::atomic<float>* chorusDepth = nullptr;
            std::atomic<float>* chorusFeedback = nullptr;
            std::atomic<float>* chorusMix = nullptr;
            std::atomic<float>* delayEnable = nullptr;
            std::atomic<float>* delaySync = nullptr;
            std::atomic<float>* delayTime = nullptr;
            std::atomic<float>* delayDivision = nullptr;
            std::atomic<float>* delayFeedback = nullptr;
            std::atomic<float>* delayPingPong = nullptr;
            std::atomic<float>* delayMix = nullptr;
            std::atomic<float>* reverbEnable = nullptr;
            std::atomic<float>* reverbSize = nullptr;
            std::atomic<float>* reverbDamping = nullptr;
            std::atomic<float>* reverbWidth = nullptr;
            std::atomic<float>* reverbMix = nullptr;
            std::atomic<float>* eqEnable = nullptr;
            std::atomic<float>* eqLowGain = nullptr;
            std::atomic<float>* eqMidFreq = nullptr;
            std::atomic<float>* eqMidGain = nullptr;
            std::atomic<float>* eqHighGain = nullptr;
        } fx {};
    } raw;

    void updateFXParams();

    static constexpr int numVoices = 16;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArsenalProcessor)
};

} // namespace arsenal
