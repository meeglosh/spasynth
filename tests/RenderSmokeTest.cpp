// Headless smoke test: instantiate the real processor, play a note, and
// assert that audio actually comes out (and stops after release).

#include "ArsenalProcessor.h"

#include <iostream>

namespace
{
    float blockMagnitude (arsenal::ArsenalProcessor& proc,
                          juce::AudioBuffer<float>& buffer,
                          juce::MidiBuffer& midi,
                          int numBlocks)
    {
        float peak = 0.0f;
        for (int i = 0; i < numBlocks; ++i)
        {
            proc.processBlock (buffer, midi);
            midi.clear();
            peak = juce::jmax (peak, buffer.getMagnitude (0, buffer.getNumSamples()));
        }
        return peak;
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    arsenal::ArsenalProcessor proc;
    proc.prepareToPlay (sampleRate, blockSize);

    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;

    // Silence before any note.
    const auto silentPeak = blockMagnitude (proc, buffer, midi, 8);

    // Note on -> expect signal.
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
    const auto notePeak = blockMagnitude (proc, buffer, midi, 32);

    // Note off -> expect decay back to silence within a few seconds.
    midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
    const auto tailBlocks = (int) (4.0 * sampleRate / blockSize);
    blockMagnitude (proc, buffer, midi, tailBlocks);
    const auto releasedPeak = blockMagnitude (proc, buffer, midi, 8);

    std::cout << "silent peak:   " << silentPeak << "\n"
              << "note peak:     " << notePeak << "\n"
              << "released peak: " << releasedPeak << "\n";

    bool pass = true;

    if (silentPeak > 1.0e-6f) { std::cout << "FAIL: output before any note\n"; pass = false; }
    if (notePeak < 0.05f)     { std::cout << "FAIL: no meaningful output during note\n"; pass = false; }
    if (notePeak > 2.0f)      { std::cout << "FAIL: output suspiciously hot\n"; pass = false; }
    if (releasedPeak > 1.0e-4f) { std::cout << "FAIL: note did not decay after release\n"; pass = false; }

    std::cout << (pass ? "PASS" : "FAIL") << "\n";
    return pass ? 0 : 1;
}
