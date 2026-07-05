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

    static void setRouteParams (arsenal::ArsenalProcessor& proc, int route,
                         arsenal::params::ModSource source,
                         const juce::String& destParamID, float depth)
    {
        namespace params = arsenal::params;
        namespace id = arsenal::params::id;

        // Destination choice index = dense mod-dest index + 1 ("None" is 0).
        const auto destChoice = (float) (params::modDestIndex (destParamID) + 1);

        setParam (proc, id::routeParam (route, id::route::source), (float) (int) source);
        setParam (proc, id::routeParam (route, id::route::dest), destChoice);
        setParam (proc, id::routeParam (route, id::route::depth), depth);
    }

    static void modMatrixMacroTest()
    {
        std::cout << "modMatrixMacroTest\n";

        namespace params = arsenal::params;
        namespace id = arsenal::params::id;

        constexpr double sampleRate = 48000.0;
        constexpr int blockSize = 512;

        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;

        // Baseline: no modulation.
        float basePeak = 0.0f;
        {
            arsenal::ArsenalProcessor proc;
            proc.prepareToPlay (sampleRate, blockSize);
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            basePeak = renderBlocks (proc, buffer, midi, 32);
        }

        // Macro 1 at full, routed to Osc A level with depth -1 -> much quieter.
        {
            arsenal::ArsenalProcessor proc;
            proc.prepareToPlay (sampleRate, blockSize);
            setRouteParams (proc, 0, params::ModSource::macro1,
                            id::oscSlot (0, id::osc::level), -1.0f);
            setParam (proc, id::macro (0), 1.0f);

            midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            const auto moddedPeak = renderBlocks (proc, buffer, midi, 32);

            expect (basePeak > 0.05f, "baseline note is audible");
            expect (moddedPeak < basePeak * 0.1f,
                    "macro->level route attenuates (base " + juce::String (basePeak)
                    + " vs modded " + juce::String (moddedPeak) + ")");
        }
    }

    static void lfoModulationTest()
    {
        std::cout << "lfoModulationTest\n";

        namespace params = arsenal::params;
        namespace id = arsenal::params::id;

        constexpr double sampleRate = 48000.0;
        constexpr int blockSize = 512;

        arsenal::ArsenalProcessor proc;
        proc.prepareToPlay (sampleRate, blockSize);

        // LFO 1: 4 Hz square, routed hard to Osc A level -> output pulses.
        setParam (proc, id::lfoParam (0, id::lfo::shape),
                  (float) (int) params::LFOShape::square);
        setParam (proc, id::lfoParam (0, id::lfo::rate), 4.0f);
        setRouteParams (proc, 0, params::ModSource::lfo1,
                        id::oscSlot (0, id::osc::level), -1.0f);

        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);

        // Collect per-block peaks over ~1 second; square LFO should produce
        // loud and near-silent blocks.
        float minPeak = 1.0e9f, maxPeak = 0.0f;
        for (int b = 0; b < (int) (sampleRate / blockSize); ++b)
        {
            proc.processBlock (buffer, midi);
            midi.clear();
            if (b < 8)
                continue;  // let the attack settle
            const auto peak = buffer.getMagnitude (0, blockSize);
            minPeak = juce::jmin (minPeak, peak);
            maxPeak = juce::jmax (maxPeak, peak);
        }

        expect (maxPeak > 0.05f, "LFO-modulated note is audible at peaks");
        expect (minPeak < maxPeak * 0.2f,
                "square LFO->level pulses output (min " + juce::String (minPeak)
                + " vs max " + juce::String (maxPeak) + ")");
    }

    static void velocityRouteTest()
    {
        std::cout << "velocityRouteTest\n";

        namespace params = arsenal::params;
        namespace id = arsenal::params::id;

        constexpr double sampleRate = 48000.0;
        constexpr int blockSize = 512;

        arsenal::ArsenalProcessor proc;
        proc.prepareToPlay (sampleRate, blockSize);

        // Velocity opens the filter: base cutoff low, velocity routed up.
        setParam (proc, id::filter1Cutoff, 200.0f);
        setRouteParams (proc, 0, params::ModSource::velocity, id::filter1Cutoff, 1.0f);

        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;

        auto brightness = [&] (juce::uint8 vel)
        {
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, vel), 0);
            renderBlocks (proc, buffer, midi, 16);
            // Rough high-frequency content estimate: mean absolute sample-to-
            // sample difference.
            float hf = 0.0f;
            for (int i = 1; i < blockSize; ++i)
                hf += std::abs (buffer.getSample (0, i) - buffer.getSample (0, i - 1));
            midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
            renderBlocks (proc, buffer, midi, (int) (2.0 * sampleRate / blockSize));
            return hf / (float) blockSize;
        };

        const auto soft = brightness (10);
        const auto hard = brightness (127);

        expect (hard > soft * 1.5f,
                "velocity->cutoff makes loud notes brighter (soft " + juce::String (soft)
                + " vs hard " + juce::String (hard) + ")");
    }

    static void chaosMixBypassTest()
    {
        std::cout << "chaosMixBypassTest\n";

        namespace id = arsenal::params::id;

        constexpr double sampleRate = 48000.0;
        constexpr int blockSize = 512;

        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;

        // Everything cranked but mix at 0 must be bit-identical to a clean
        // render (phase mode Reset is deterministic).
        auto render = [&] (float mix)
        {
            arsenal::ArsenalProcessor proc;
            proc.prepareToPlay (sampleRate, blockSize);
            setParam (proc, id::chaos::depth, 1.0f);
            setParam (proc, id::chaos::rate, 10.0f);
            setParam (proc, id::chaos::satOn, 1.0f);
            setParam (proc, id::chaos::distOn, 1.0f);
            setParam (proc, id::chaos::saturation, 1.0f);
            setParam (proc, id::chaos::distortion, 1.0f);
            setParam (proc, id::chaos::mix, mix);

            midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            juce::AudioBuffer<float> capture (2, blockSize);
            for (int b = 0; b < 16; ++b)
            {
                proc.processBlock (buffer, midi);
                midi.clear();
            }
            capture.makeCopyOf (buffer);
            return capture;
        };

        arsenal::ArsenalProcessor clean;
        clean.prepareToPlay (sampleRate, blockSize);
        setParam (clean, id::chaos::enable, 0.0f);
        juce::AudioBuffer<float> cleanBuf (2, blockSize);
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        for (int b = 0; b < 16; ++b)
        {
            clean.processBlock (cleanBuf, midi);
            midi.clear();
        }

        const auto mixed0 = render (0.0f);
        float maxDiff = 0.0f;
        for (int i = 0; i < blockSize; ++i)
            maxDiff = juce::jmax (maxDiff, std::abs (mixed0.getSample (0, i)
                                                     - cleanBuf.getSample (0, i)));
        expect (maxDiff < 1.0e-6f, "chaos mix=0 is bit-transparent (diff "
                                   + juce::String (maxDiff) + ")");

        // Full mix with heavy sat/dist must differ audibly from clean.
        const auto mixed1 = render (1.0f);
        float diff1 = 0.0f;
        for (int i = 0; i < blockSize; ++i)
            diff1 = juce::jmax (diff1, std::abs (mixed1.getSample (0, i)
                                                 - cleanBuf.getSample (0, i)));
        expect (diff1 > 1.0e-3f, "chaos mix=1 audibly changes output (diff "
                                 + juce::String (diff1) + ")");
    }

    static void chaosMatrixSourceTest()
    {
        std::cout << "chaosMatrixSourceTest\n";

        namespace params = arsenal::params;
        namespace id = arsenal::params::id;

        constexpr double sampleRate = 48000.0;
        constexpr int blockSize = 512;

        arsenal::ArsenalProcessor proc;
        proc.prepareToPlay (sampleRate, blockSize);

        // Chaos as a matrix source hammering oscillator level. Fast rate and
        // full depth: per-block peaks must fluctuate.
        setParam (proc, id::chaos::depth, 1.0f);
        setParam (proc, id::chaos::rate, 15.0f);
        setRouteParams (proc, 0, params::ModSource::chaos,
                        id::oscSlot (0, id::osc::level), -1.0f);

        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);

        float minPeak = 1.0e9f, maxPeak = 0.0f;
        for (int b = 0; b < (int) (2.0 * sampleRate / blockSize); ++b)
        {
            proc.processBlock (buffer, midi);
            midi.clear();
            if (b < 8)
                continue;
            const auto peak = buffer.getMagnitude (0, blockSize);
            minPeak = juce::jmin (minPeak, peak);
            maxPeak = juce::jmax (maxPeak, peak);
        }

        expect (maxPeak > 0.02f, "chaos-modulated note is audible");
        expect (minPeak < maxPeak * 0.7f,
                "chaos source varies level over time (min " + juce::String (minPeak)
                + " vs max " + juce::String (maxPeak) + ")");
    }

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    renderSmokeTest();
    multiSlotUnisonTest();
    wavetableLoaderTest();
    modMatrixMacroTest();
    lfoModulationTest();
    velocityRouteTest();
    chaosMixBypassTest();
    chaosMatrixSourceTest();

    std::cout << (failures == 0 ? "ALL PASS" : juce::String (failures) + " FAILURES") << "\n";
    return failures == 0 ? 0 : 1;
}
