#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>

namespace spa
{

// MIDI Learn: binds hardware CCs to plugin parameters.
//
// Threading: the audio thread calls processMidi() every block (lock-free —
// the CC map is an array of atomics); everything else is message-thread.
// Broadcasts a change whenever an assignment is made, cleared, or restored,
// and when a pending learn captures its CC.
class MidiLearnManager : public juce::ChangeBroadcaster
{
public:
    explicit MidiLearnManager (juce::AudioProcessorValueTreeState&);

    // --- Message thread ------------------------------------------------------
    void armLearn (const juce::String& paramID);
    void cancelLearn();
    bool isArmed() const { return armedParamIndex.load() >= 0; }
    juce::String getArmedParamID() const;

    int getAssignedCC (const juce::String& paramID) const;   // -1 = none
    void clearAssignment (const juce::String& paramID);
    void clearAll();

    juce::ValueTree toValueTree() const;                     // type "MIDIMAP"
    void restoreFromValueTree (const juce::ValueTree&);

    static constexpr const char* mapTreeType = "MIDIMAP";

    // --- Audio thread --------------------------------------------------------
    // Captures a pending learn and applies mapped CCs to their parameters.
    void processMidi (const juce::MidiBuffer&);

private:
    int indexOfParam (const juce::String& paramID) const;

    juce::AudioProcessorValueTreeState& apvts;
    std::vector<juce::RangedAudioParameter*> parametersByIndex;

    std::array<std::atomic<int>, 128> ccToParam;   // param index or -1
    std::atomic<int> armedParamIndex { -1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiLearnManager)
};

} // namespace spa
