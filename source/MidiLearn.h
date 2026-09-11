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
// Applying a mapped CC to its parameter used to call
// juce::AudioProcessorParameter::setValueNotifyingHost() directly from
// processMidi() on the audio thread. That call takes a CriticalSection
// internally (listenerLock, in sendValueChangedMessageToListeners) to walk
// its listener list, a lock on the audio thread, which violates this
// codebase's RT-safety invariant and can silently delay/starve the host
// notification if that lock is ever contended by the message thread. Fixed:
// processMidi() still writes the parameter's own value synchronously
// (RangedAudioParameter::setValue() is a plain non-locking write, so the DSP,
// which reads raw APVTS pointers directly, reacts immediately); only the
// host/UI notification is deferred to the message thread via AsyncUpdater
// (event-driven, posted once per captured/mapped CC, not a periodic poll).
class MidiLearnManager : public juce::ChangeBroadcaster,
                          private juce::AsyncUpdater
{
public:
    explicit MidiLearnManager (juce::AudioProcessorValueTreeState&);
    ~MidiLearnManager() override;

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
    void handleAsyncUpdate() override;   // message thread: setValueNotifyingHost

    juce::AudioProcessorValueTreeState& apvts;
    std::vector<juce::RangedAudioParameter*> parametersByIndex;

    std::array<std::atomic<int>, 128> ccToParam;   // param index or -1
    std::atomic<int> armedParamIndex { -1 };

    // Deferred host-notify hop for processMidi(); see the class comment
    // above. Single slot: a burst of CCs for the same learned target simply
    // coalesces to the latest value, which is what triggerAsyncUpdate()
    // already does for repeated calls before the message loop turns.
    std::atomic<int> pendingApplyIndex { -1 };
    std::atomic<float> pendingApplyValue { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiLearnManager)
};

} // namespace spa
