// Headless test suite: exercises the real processor and the wavetable
// pipeline without a host.

#include "ArsenalProcessor.h"
#include "dsp/WavetableLoader.h"
#include "params/ParameterRegistry.h"

#include <iostream>

namespace
{
    int failures = 0;

    void expect (bool condition, const juce::String& description)
    {
        std::cout << (condition ? "  ok    " : "  FAIL  ") << description << "\n";
        if (! condition)
            ++failures;
    }

    float renderBlocks (arsenal::ArsenalProcessor& proc,
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

    void setParam (arsenal::ArsenalProcessor& proc, const juce::String& id, float realValue)
    {
        auto* param = proc.getAPVTS().getParameter (id);
        jassert (param != nullptr);
        param->setValueNotifyingHost (param->convertTo0to1 (realValue));
    }

    void renderSmokeTest()
    {
        std::cout << "renderSmokeTest\n";

        constexpr double sampleRate = 48000.0;
        constexpr int blockSize = 512;

        arsenal::ArsenalProcessor proc;
        proc.prepareToPlay (sampleRate, blockSize);

        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;

        const auto silentPeak = renderBlocks (proc, buffer, midi, 8);
        expect (silentPeak < 1.0e-6f, "silent before any note");

        midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        const auto notePeak = renderBlocks (proc, buffer, midi, 32);
        expect (notePeak > 0.05f, "meaningful output during note");
        expect (notePeak < 2.0f, "output not clipping hot");

        midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
        renderBlocks (proc, buffer, midi, (int) (4.0 * sampleRate / blockSize));
        const auto releasedPeak = renderBlocks (proc, buffer, midi, 8);
        expect (releasedPeak < 1.0e-4f, "decays to silence after release");
    }

    void multiSlotUnisonTest()
    {
        std::cout << "multiSlotUnisonTest\n";

        constexpr double sampleRate = 48000.0;
        constexpr int blockSize = 512;

        arsenal::ArsenalProcessor proc;
        proc.prepareToPlay (sampleRate, blockSize);

        namespace id = arsenal::params::id;
        setParam (proc, id::oscSlot (1, id::osc::enable), 1.0f);
        setParam (proc, id::oscSlot (1, id::osc::coarse), 12.0f);
        setParam (proc, id::oscSlot (0, id::osc::unisonCount), 5.0f);
        setParam (proc, id::oscSlot (0, id::osc::unisonDetune), 30.0f);
        setParam (proc, id::oscSlot (0, id::osc::unisonWidth), 1.0f);
        setParam (proc, id::oscSlot (0, id::osc::position), 0.66f);

        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;

        midi.addEvent (juce::MidiMessage::noteOn (1, 48, (juce::uint8) 100), 0);
        const auto notePeak = renderBlocks (proc, buffer, midi, 32);
        expect (notePeak > 0.05f, "two slots + unison produce output");
        expect (notePeak < 2.0f, "unison stack level-compensated");

        // Stereo width: with full width and detune the channels should differ.
        proc.processBlock (buffer, midi);
        float diff = 0.0f;
        for (int i = 0; i < blockSize; ++i)
            diff = juce::jmax (diff, std::abs (buffer.getSample (0, i) - buffer.getSample (1, i)));
        expect (diff > 1.0e-3f, "unison width produces stereo image");
    }

    void wavetableLoaderTest()
    {
        std::cout << "wavetableLoaderTest\n";

        // Build a 4-frame Serum-convention WAV (frames of 2048 samples).
        constexpr int frameSize = arsenal::dsp::Wavetable::tableSize;
        constexpr int numFrames = 4;

        juce::AudioBuffer<float> buffer (1, frameSize * numFrames);
        for (int f = 0; f < numFrames; ++f)
            for (int i = 0; i < frameSize; ++i)
            {
                // Frame f = f+1'th harmonic sine, so band-limiting is testable.
                const auto phase = juce::MathConstants<double>::twoPi * (f + 1) * i / frameSize;
                buffer.setSample (0, f * frameSize + i, (float) std::sin (phase));
            }

        const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                              .getNonexistentChildFile ("arsenal-wt-test", ".wav");

        {
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
            auto writer = wav.createWriterFor (stream,
                                               juce::AudioFormatWriterOptions()
                                                   .withSampleRate (48000.0)
                                                   .withNumChannels (1)
                                                   .withBitsPerSample (24));
            expect (writer != nullptr, "test WAV writer created");
            if (writer == nullptr)
                return;
            writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
        }

        auto result = arsenal::dsp::loadWavetableFromFile (file);
        expect (result.table != nullptr, "wavetable loads: " + result.error);

        if (result.table != nullptr)
        {
            expect (result.table->getNumFrames() == numFrames, "detects 4 frames");

            // Frame 0 (pure fundamental) must survive even the last mip level.
            const auto* frame0 = result.table->getFrame (
                arsenal::dsp::Wavetable::numMipLevels - 1, 0);
            float peak = 0.0f;
            for (int i = 0; i < frameSize; ++i)
                peak = juce::jmax (peak, std::abs (frame0[i]));
            expect (peak > 0.5f, "fundamental survives deepest mip level");
        }

        file.deleteFile();
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    renderSmokeTest();
    multiSlotUnisonTest();
    wavetableLoaderTest();

    std::cout << (failures == 0 ? "ALL PASS" : juce::String (failures) + " FAILURES") << "\n";
    return failures == 0 ? 0 : 1;
}
