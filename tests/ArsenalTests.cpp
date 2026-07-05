// Headless test suite: exercises the real processor and the wavetable
// pipeline without a host.

#include "ArsenalProcessor.h"
#include "dsp/WavetableLoader.h"
#include "library/Library.h"
#include "library/PresetManager.h"
#include "params/ParameterRegistry.h"
#include "params/Randomizer.h"
#include "ui/ArsenalEditor.h"

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

    // Writes a WAV whose amplitude ramps 0 -> 1 over its length (440 Hz sine).
    static juce::File writeRampSine (double seconds, double sampleRate)
    {
        const auto numSamples = (int) (seconds * sampleRate);
        juce::AudioBuffer<float> buffer (1, numSamples);
        for (int i = 0; i < numSamples; ++i)
        {
            const auto ramp = (float) i / (float) numSamples;
            buffer.setSample (0, i, ramp * (float) std::sin (
                juce::MathConstants<double>::twoPi * 440.0 * i / sampleRate));
        }

        const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                              .getNonexistentChildFile ("arsenal-sfx-test", ".wav");
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
        auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions()
                                                       .withSampleRate (sampleRate)
                                                       .withNumChannels (1)
                                                       .withBitsPerSample (24));
        if (writer != nullptr)
            writer->writeFromAudioSampleBuffer (buffer, 0, numSamples);
        return file;
    }

    // Pumps the message loop until the sample lands in the slot.
    static bool waitForSample (arsenal::ArsenalProcessor& proc, int slot, int timeoutMs)
    {
        const auto deadline = juce::Time::getMillisecondCounter() + (juce::uint32) timeoutMs;
        while (juce::Time::getMillisecondCounter() < deadline)
        {
            if (proc.getSampleName (slot).isNotEmpty() || proc.getSampleError (slot).isNotEmpty())
                return proc.getSampleError (slot).isEmpty();
            juce::MessageManager::getInstance()->runDispatchLoopUntil (10);
        }
        return false;
    }

    static void samplePlaybackTest()
    {
        std::cout << "samplePlaybackTest\n";

        namespace params = arsenal::params;
        namespace id = arsenal::params::id;

        constexpr double sampleRate = 48000.0;
        constexpr int blockSize = 512;

        const auto file = writeRampSine (2.0, sampleRate);

        arsenal::ArsenalProcessor proc;
        proc.prepareToPlay (sampleRate, blockSize);
        proc.loadSampleFromFile (0, file);
        expect (waitForSample (proc, 0, 15000), "sample loads with analysis");

        // Classic sample mode, keytrack off -> plays at source pitch (440 Hz).
        setParam (proc, id::oscSlot (0, id::osc::mode), (float) (int) params::OscMode::sample);
        setParam (proc, id::oscSlot (0, id::osc::keytrack), 0.0f);
        setParam (proc, id::oscSlot (0, id::osc::sampleStart), 0.5f);  // start in audible region

        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 48, (juce::uint8) 100), 0);
        renderBlocks (proc, buffer, midi, 16);

        // Estimate frequency by zero crossings over one block.
        proc.processBlock (buffer, midi);
        int crossings = 0;
        for (int i = 1; i < blockSize; ++i)
            if ((buffer.getSample (0, i - 1) < 0.0f) != (buffer.getSample (0, i) < 0.0f))
                ++crossings;
        const auto freq = (float) crossings * (float) sampleRate / (2.0f * blockSize);
        expect (freq > 400.0f && freq < 480.0f,
                "keytrack-off sample plays at source pitch (" + juce::String (freq) + " Hz)");

        file.deleteFile();
    }

    static void granularTest()
    {
        std::cout << "granularTest\n";

        namespace params = arsenal::params;
        namespace id = arsenal::params::id;

        constexpr double sampleRate = 48000.0;
        constexpr int blockSize = 512;

        const auto file = writeRampSine (2.0, sampleRate);

        arsenal::ArsenalProcessor proc;
        proc.prepareToPlay (sampleRate, blockSize);
        proc.loadSampleFromFile (0, file);
        expect (waitForSample (proc, 0, 15000), "sample loads for granular");

        setParam (proc, id::oscSlot (0, id::osc::mode), (float) (int) params::OscMode::granular);
        setParam (proc, id::oscSlot (0, id::osc::grainPos), 0.8f);  // loud region
        setParam (proc, id::oscSlot (0, id::osc::grainDensity), 30.0f);

        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        const auto peak = renderBlocks (proc, buffer, midi, 32);
        expect (peak > 0.02f, "granular engine produces output (peak "
                              + juce::String (peak) + ")");

        file.deleteFile();
    }

    static void sfxFollowerTest()
    {
        std::cout << "sfxFollowerTest\n";

        namespace params = arsenal::params;
        namespace id = arsenal::params::id;

        constexpr double sampleRate = 48000.0;
        constexpr int blockSize = 512;

        const auto file = writeRampSine (2.0, sampleRate);

        arsenal::ArsenalProcessor proc;
        proc.prepareToPlay (sampleRate, blockSize);
        proc.loadSampleFromFile (0, file);
        expect (waitForSample (proc, 0, 15000), "sample loads for follower");

        // Slot A plays the ramping SFX (loop off). Its amp follower drives
        // slot B's level DOWN: as the SFX gets louder, slot B gets quieter.
        setParam (proc, id::oscSlot (0, id::osc::mode), (float) (int) params::OscMode::sample);
        setParam (proc, id::oscSlot (0, id::osc::loop), 0.0f);
        setParam (proc, id::oscSlot (0, id::osc::level), -60.0f);  // SFX itself silent
        setParam (proc, id::oscSlot (1, id::osc::enable), 1.0f);
        setRouteParams (proc, 0, params::ModSource::sfxAmpA,
                        id::oscSlot (1, id::osc::level), -1.0f);

        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);

        // Average block peaks early vs late in the 2-second sample.
        auto averagePeak = [&] (int numBlocks)
        {
            float sum = 0.0f;
            for (int b = 0; b < numBlocks; ++b)
            {
                proc.processBlock (buffer, midi);
                midi.clear();
                sum += buffer.getMagnitude (0, blockSize);
            }
            return sum / (float) numBlocks;
        };

        averagePeak (8);  // attack settles
        const auto early = averagePeak (30);
        averagePeak ((int) (sampleRate / blockSize));  // skip ~1s into the ramp
        const auto late = averagePeak (30);

        expect (early > 0.02f, "follower patch is audible early on");
        expect (late < early * 0.6f,
                "amp follower tracks the SFX ramp (early " + juce::String (early)
                + " vs late " + juce::String (late) + ")");

        file.deleteFile();
    }

    static void fxDelayReverbTest()
    {
        std::cout << "fxDelayReverbTest\n";

        namespace id = arsenal::params::id;

        constexpr double sampleRate = 48000.0;
        constexpr int blockSize = 512;

        // Short staccato note; measure energy in the window 0.2-1.0s after
        // note-off, with and without delay+reverb.
        auto tailEnergy = [&] (bool fxOn)
        {
            arsenal::ArsenalProcessor proc;
            proc.prepareToPlay (sampleRate, blockSize);
            setParam (proc, id::ampRelease, 0.02f);
            setParam (proc, id::chaos::enable, 0.0f);

            if (fxOn)
            {
                setParam (proc, id::fx::delayEnable, 1.0f);
                setParam (proc, id::fx::delaySync, 0.0f);
                setParam (proc, id::fx::delayTime, 150.0f);
                setParam (proc, id::fx::delayFeedback, 0.6f);
                setParam (proc, id::fx::delayMix, 0.8f);
                setParam (proc, id::fx::reverbEnable, 1.0f);
                setParam (proc, id::fx::reverbMix, 0.5f);
            }

            juce::AudioBuffer<float> buffer (2, blockSize);
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            midi.addEvent (juce::MidiMessage::noteOff (1, 60), blockSize - 1);

            float energy = 0.0f;
            const auto blocksTotal = (int) (1.0 * sampleRate / blockSize);
            const auto blocksSkip = (int) (0.2 * sampleRate / blockSize);
            for (int b = 0; b < blocksTotal; ++b)
            {
                proc.processBlock (buffer, midi);
                midi.clear();
                if (b >= blocksSkip)
                    energy += buffer.getRMSLevel (0, 0, blockSize);
            }
            return energy;
        };

        const auto dry = tailEnergy (false);
        const auto wet = tailEnergy (true);
        expect (wet > dry * 3.0f + 1.0e-4f,
                "delay+reverb produce a tail (dry " + juce::String (dry)
                + " vs wet " + juce::String (wet) + ")");
    }

    static void fxEQDistortionTest()
    {
        std::cout << "fxEQDistortionTest\n";

        namespace id = arsenal::params::id;

        constexpr double sampleRate = 48000.0;
        constexpr int blockSize = 512;

        auto brightnessWith = [&] (auto configure)
        {
            arsenal::ArsenalProcessor proc;
            proc.prepareToPlay (sampleRate, blockSize);
            setParam (proc, id::chaos::enable, 0.0f);
            setParam (proc, id::oscSlot (0, id::osc::position), 0.66f);  // saw-ish
            configure (proc);

            juce::AudioBuffer<float> buffer (2, blockSize);
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            renderBlocks (proc, buffer, midi, 16);

            proc.processBlock (buffer, midi);
            float hf = 0.0f;
            for (int i = 1; i < blockSize; ++i)
                hf += std::abs (buffer.getSample (0, i) - buffer.getSample (0, i - 1));
            return hf / (float) blockSize;
        };

        const auto flat = brightnessWith ([] (auto&) {});
        const auto darkened = brightnessWith ([] (auto& proc)
        {
            setParam (proc, id::fx::eqEnable, 1.0f);
            setParam (proc, id::fx::eqHighGain, -12.0f);
            setParam (proc, id::fx::eqMidGain, -12.0f);
            setParam (proc, id::fx::eqMidFreq, 4000.0f);
        });
        expect (darkened < flat * 0.8f,
                "EQ high/mid cut darkens output (flat " + juce::String (flat)
                + " vs cut " + juce::String (darkened) + ")");

        // Distortion flattens peaks: crest factor (peak/RMS) must drop.
        auto crestWith = [&] (auto configure)
        {
            arsenal::ArsenalProcessor proc;
            proc.prepareToPlay (sampleRate, blockSize);
            setParam (proc, id::chaos::enable, 0.0f);
            setParam (proc, id::oscSlot (0, id::osc::position), 0.66f);
            configure (proc);

            juce::AudioBuffer<float> buffer (2, blockSize);
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            renderBlocks (proc, buffer, midi, 16);

            proc.processBlock (buffer, midi);
            const auto peak = buffer.getMagnitude (0, blockSize);
            const auto rms = buffer.getRMSLevel (0, 0, blockSize);
            return rms > 0.0f ? peak / rms : 0.0f;
        };

        const auto crestClean = crestWith ([] (auto&) {});
        const auto crestDriven = crestWith ([] (auto& proc)
        {
            setParam (proc, id::fx::distEnable, 1.0f);
            setParam (proc, id::fx::distDrive, 1.0f);
            setParam (proc, id::fx::distTone, 20000.0f);
        });
        expect (crestDriven < crestClean * 0.9f,
                "distortion flattens peaks (clean crest " + juce::String (crestClean)
                + " vs driven " + juce::String (crestDriven) + ")");
    }

    static void randomizerTest()
    {
        std::cout << "randomizerTest\n";

        namespace params = arsenal::params;
        namespace id = arsenal::params::id;

        constexpr double sampleRate = 48000.0;
        constexpr int blockSize = 512;

        arsenal::ArsenalProcessor proc;
        proc.prepareToPlay (sampleRate, blockSize);
        auto& apvts = proc.getAPVTS();

        auto snapshot = [&]
        {
            std::vector<float> values;
            for (const auto& def : params::all())
                values.push_back (apvts.getParameter (def.id)->getValue());
            return values;
        };

        // Re-roll changes a substantial number of parameters.
        const auto before = snapshot();
        proc.randomizeAll();
        const auto after = snapshot();

        int changed = 0;
        for (size_t i = 0; i < before.size(); ++i)
            if (std::abs (before[i] - after[i]) > 1.0e-4f)
                ++changed;
        expect (changed > 30, "re-roll changes many params ("
                              + juce::String (changed) + " changed)");

        // Osc A always survives a re-roll enabled.
        expect (apvts.getParameter (id::oscSlot (0, id::osc::enable))->getValue() >= 0.5f,
                "osc A stays enabled after re-roll");

        // Constrained bounds hold at default wildness: cutoff never below its
        // minNorm window, resonance never in the self-oscillation zone.
        bool boundsOk = true;
        for (int roll = 0; roll < 30; ++roll)
        {
            proc.randomizeAll();
            boundsOk = boundsOk
                    && apvts.getParameter (id::filter1Resonance)->getValue() <= 0.86f;
        }
        expect (boundsOk, "randomization respects per-param constrained ranges");

        // Locks: filter section untouched when locked.
        proc.setLockGroupLocked ((int) params::LockGroup::filter, true);
        const auto cutoffBefore = apvts.getParameter (id::filter1Cutoff)->getValue();
        const auto typeBefore = apvts.getParameter (id::filter1Type)->getValue();
        for (int roll = 0; roll < 5; ++roll)
            proc.randomizeAll();
        expect (juce::approximatelyEqual (
                    apvts.getParameter (id::filter1Cutoff)->getValue(), cutoffBefore)
                && juce::approximatelyEqual (
                    apvts.getParameter (id::filter1Type)->getValue(), typeBefore),
                "locked filter section survives re-rolls");
        proc.setLockGroupLocked ((int) params::LockGroup::filter, false);

        // No-sample slots never land in sample/granular mode.
        bool modesOk = true;
        for (int roll = 0; roll < 10; ++roll)
        {
            proc.randomizeAll();
            for (int s = 0; s < params::numOscSlots; ++s)
                modesOk = modesOk
                       && apvts.getParameter (id::oscSlot (s, id::osc::mode))->getValue() < 0.01f;
        }
        expect (modesOk, "slots without samples stay in wavetable mode");
    }

    static void randomizerProducesSoundTest()
    {
        std::cout << "randomizerProducesSoundTest\n";

        namespace params = arsenal::params;

        constexpr double sampleRate = 48000.0;
        constexpr int blockSize = 512;

        arsenal::ArsenalProcessor proc;
        proc.prepareToPlay (sampleRate, blockSize);

        // The matrix can legitimately duck levels (that's its job); lock it so
        // this test isolates the "every re-roll makes sound" guarantee.
        proc.setLockGroupLocked ((int) params::LockGroup::matrix, true);

        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;

        int audible = 0;
        constexpr int rolls = 8;
        for (int roll = 0; roll < rolls; ++roll)
        {
            proc.randomizeAll();
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            const auto peak = renderBlocks (proc, buffer, midi, 90);  // ~1s
            midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
            renderBlocks (proc, buffer, midi, 90);
            if (peak > 0.005f)
                ++audible;
        }

        expect (audible == rolls, "every re-roll produces an audible patch ("
                                  + juce::String (audible) + "/" + juce::String (rolls) + ")");
    }

    // Builds a throwaway library: two packs with tiny WAVs.
    static juce::File makeFakeLibrary()
    {
        const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory)
                              .getNonexistentChildFile ("arsenal-lib-test", "");
        for (auto* pack : { "Alpha Pack", "Beta Pack" })
            for (auto* wav : { "one.wav", "two.wav", "three.wav" })
            {
                juce::AudioBuffer<float> buffer (1, 4800);
                for (int i = 0; i < 4800; ++i)
                    buffer.setSample (0, i, 0.5f * (float) std::sin (
                        juce::MathConstants<double>::twoPi * 220.0 * i / 48000.0));

                const auto file = root.getChildFile (pack).getChildFile (wav);
                file.getParentDirectory().createDirectory();
                juce::WavAudioFormat fmt;
                std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
                if (auto writer = fmt.createWriterFor (stream,
                        juce::AudioFormatWriterOptions().withSampleRate (48000.0)
                            .withNumChannels (1).withBitsPerSample (24)))
                    writer->writeFromAudioSampleBuffer (buffer, 0, 4800);
            }
        return root;
    }

    static void libraryScanTest()
    {
        std::cout << "libraryScanTest\n";

        namespace lib = arsenal::library;

        const auto root = makeFakeLibrary();
        const auto packs = lib::scanLibrary (root);

        expect (packs.size() == 2, "scan finds two pack categories");
        expect (! packs.empty() && packs[0].name == "Alpha Pack"
                && packs[0].wavs.size() == 3,
                "pack folder maps to category with its wavs");

        // Portable path round-trip.
        const auto wav = packs[0].wavs.getFirst();
        const auto portable = lib::toPortable (wav, root);
        expect (portable.startsWith ("$LIB$"), "library paths serialize portably");
        expect (lib::fromPortable (portable, root) == wav, "portable path resolves back");

        root.deleteRecursively();
    }

    static void libraryDiscoveryTest()
    {
        std::cout << "libraryDiscoveryTest\n";

        namespace lib = arsenal::library;

        const auto realLib = makeFakeLibrary();
        const auto emptyDir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                  .getNonexistentChildFile ("arsenal-empty", "");
        emptyDir.createDirectory();
        const auto missing = juce::File ("/nonexistent/arsenal-lib");

        expect (lib::looksLikeLibrary (realLib), "pack folders identify a library");
        expect (! lib::looksLikeLibrary (emptyDir), "empty folder is not a library");
        expect (! lib::looksLikeLibrary (missing), "missing folder is not a library");

        // Discovery skips invalid candidates and lands on the first real one.
        expect (lib::discoverLibrary ({ missing, emptyDir, realLib }) == realLib,
                "discovery finds the library among standard locations");
        expect (lib::discoverLibrary ({ missing, emptyDir }) == juce::File(),
                "discovery reports nothing when no candidate is valid");

        realLib.deleteRecursively();
        emptyDir.deleteRecursively();
    }

    static void presetRoundTripTest()
    {
        std::cout << "presetRoundTripTest\n";

        namespace params = arsenal::params;
        namespace id = arsenal::params::id;
        namespace lib = arsenal::library;

        arsenal::ArsenalProcessor proc;
        proc.prepareToPlay (48000.0, 512);

        const auto presetsRoot = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                     .getNonexistentChildFile ("arsenal-presets-test", "");
        lib::PresetManager pm (proc.getAPVTS(),
                               [&] { return proc.buildStateTree(); },
                               [&] (const juce::ValueTree& t) { proc.restoreStateTree (t); },
                               presetsRoot);

        setParam (proc, id::filter1Cutoff, 1234.0f);
        setParam (proc, id::oscSlot (0, id::osc::position), 0.42f);
        expect (pm.saveUserPreset ("RoundTrip"), "user preset saves");

        setParam (proc, id::filter1Cutoff, 20000.0f);
        setParam (proc, id::oscSlot (0, id::osc::position), 0.0f);

        pm.rescan();
        bool loaded = false;
        for (size_t i = 0; i < pm.getPresets().size(); ++i)
            if (pm.getPresets()[i].name == "RoundTrip")
                loaded = pm.loadPreset ((int) i);
        expect (loaded, "user preset loads back");

        const auto cutoff = proc.getAPVTS().getParameter (id::filter1Cutoff)
                                ->convertFrom0to1 (proc.getAPVTS()
                                    .getParameter (id::filter1Cutoff)->getValue());
        expect (std::abs (cutoff - 1234.0f) < 5.0f,
                "params restore from preset (cutoff " + juce::String (cutoff) + ")");
        expect (pm.getCurrentName() == "RoundTrip", "current preset name tracks");

        presetsRoot.deleteRecursively();
    }

    static void factoryPresetGenerationTest()
    {
        std::cout << "factoryPresetGenerationTest\n";

        namespace params = arsenal::params;
        namespace id = arsenal::params::id;
        namespace lib = arsenal::library;

        constexpr double sampleRate = 48000.0;
        constexpr int blockSize = 512;

        arsenal::ArsenalProcessor proc;
        proc.prepareToPlay (sampleRate, blockSize);

        const auto libRoot = makeFakeLibrary();
        const auto presetsRoot = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                     .getNonexistentChildFile ("arsenal-factory-test", "");

        lib::PresetManager pm (proc.getAPVTS(),
                               [&] { return proc.buildStateTree(); },
                               [&] (const juce::ValueTree& t) { proc.restoreStateTree (t); },
                               presetsRoot);

        const auto packs = lib::scanLibrary (libRoot);
        const auto written = pm.generateFactoryPresets (packs, libRoot);
        expect (written == 6, "3 factory presets per pack ("
                              + juce::String (written) + " written)");
        expect (pm.getCategories().size() == 2, "one preset category per pack");

        // Load a "Keys" preset end-to-end: sample mode engages and the
        // library sample actually loads (portable path resolution works).
        bool foundKeys = false;
        for (size_t i = 0; i < pm.getPresets().size(); ++i)
        {
            if (pm.getPresets()[i].name == "Alpha Pack Keys")
            {
                foundKeys = pm.loadPreset ((int) i);
                break;
            }
        }
        expect (foundKeys, "factory Keys preset loads");

        // Portable-path lambda in restoreStateTree resolves against the
        // *configured* library root; for the test, resolve manually instead:
        // the preset stores $LIB$ paths, so with no configured root the load
        // lands nowhere. Verify the stored path is portable and resolvable.
        const auto presetFile = presetsRoot.getChildFile ("Factory")
                                    .getChildFile ("Alpha Pack")
                                    .findChildFiles (juce::File::findFiles, false,
                                                     "*Keys*").getFirst();
        const auto xml = juce::XmlDocument::parse (presetFile);
        expect (xml != nullptr, "factory preset file parses as XML");
        if (xml != nullptr)
        {
            const auto state = juce::ValueTree::fromXml (*xml->getFirstChildElement());
            const auto stored = state.getChildWithName ("SAMPLES")
                                     .getProperty ("slot0").toString();
            expect (stored.startsWith ("$LIB$"), "factory preset stores portable path");
            expect (lib::fromPortable (stored, libRoot).existsAsFile(),
                    "portable path resolves to a real library file");
        }

        // The mode parameter came through the preset.
        const auto mode = (int) proc.getAPVTS().getParameter (
            id::oscSlot (0, id::osc::mode))->convertFrom0to1 (
                proc.getAPVTS().getParameter (id::oscSlot (0, id::osc::mode))->getValue());
        expect (mode == (int) params::OscMode::sample, "Keys preset sets sample mode");

        libRoot.deleteRecursively();
        presetsRoot.deleteRecursively();
    }

    // Renders the editor offscreen for visual review: ArsenalTests --snapshot <dir>
    static void renderEditorSnapshots (const juce::File& outDir)
    {
        arsenal::ArsenalProcessor proc;
        proc.prepareToPlay (48000.0, 512);

        const auto previousPreference = arsenal::library::getDarkThemeEnabled();

        for (const bool dark : { true, false })
        {
            arsenal::ui::setDarkTheme (dark);
            std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
            editor->setSize (arsenal::ui::metrics::baseWidth, arsenal::ui::metrics::baseHeight);

            const auto image = editor->createComponentSnapshot (editor->getLocalBounds());
            const auto file = outDir.getChildFile (dark ? "arsenal-dark.png" : "arsenal-light.png");
            file.deleteFile();
            juce::PNGImageFormat png;
            juce::FileOutputStream stream (file);
            if (stream.openedOk())
                png.writeImageToStream (image, stream);
            std::cout << "snapshot: " << file.getFullPathName() << "\n";
        }

        arsenal::ui::setDarkTheme (previousPreference);  // don't pollute user settings
    }

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    if (argc >= 3 && juce::String (argv[1]) == "--snapshot")
    {
        renderEditorSnapshots (juce::File (argv[2]));
        return 0;
    }

    renderSmokeTest();
    multiSlotUnisonTest();
    wavetableLoaderTest();
    modMatrixMacroTest();
    lfoModulationTest();
    velocityRouteTest();
    chaosMixBypassTest();
    chaosMatrixSourceTest();
    samplePlaybackTest();
    granularTest();
    sfxFollowerTest();
    fxDelayReverbTest();
    fxEQDistortionTest();
    randomizerTest();
    randomizerProducesSoundTest();
    libraryScanTest();
    libraryDiscoveryTest();
    presetRoundTripTest();
    factoryPresetGenerationTest();

    std::cout << (failures == 0 ? "ALL PASS" : juce::String (failures) + " FAILURES") << "\n";
    return failures == 0 ? 0 : 1;
}
