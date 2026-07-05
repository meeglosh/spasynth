#include "ArsenalVoice.h"

namespace arsenal::dsp
{

namespace
{
    // Named handles into the dense mod-dest index space, resolved once.
    struct DestLookup
    {
        struct Slot
        {
            int position, coarse, fine, level, pan, unisonDetune, unisonBlend, unisonWidth;
        };
        std::array<Slot, params::numOscSlots> slots {};
        int cutoff, resonance, drive;
        int ampA, ampD, ampS, ampR;
        int env2A, env2D, env2S, env2R;
        int env3A, env3D, env3S, env3R;
        int chaosDepth, chaosRate, chaosMix;

        static const DestLookup& get()
        {
            static const DestLookup lookup = []
            {
                jassert (params::numModDests() <= params::maxModDests);

                DestLookup l {};
                namespace id = params::id;

                for (int s = 0; s < params::numOscSlots; ++s)
                {
                    auto& slot = l.slots[(size_t) s];
                    slot.position     = params::modDestIndex (id::oscSlot (s, id::osc::position));
                    slot.coarse       = params::modDestIndex (id::oscSlot (s, id::osc::coarse));
                    slot.fine         = params::modDestIndex (id::oscSlot (s, id::osc::fine));
                    slot.level        = params::modDestIndex (id::oscSlot (s, id::osc::level));
                    slot.pan          = params::modDestIndex (id::oscSlot (s, id::osc::pan));
                    slot.unisonDetune = params::modDestIndex (id::oscSlot (s, id::osc::unisonDetune));
                    slot.unisonBlend  = params::modDestIndex (id::oscSlot (s, id::osc::unisonBlend));
                    slot.unisonWidth  = params::modDestIndex (id::oscSlot (s, id::osc::unisonWidth));
                }

                l.cutoff    = params::modDestIndex (id::filter1Cutoff);
                l.resonance = params::modDestIndex (id::filter1Resonance);
                l.drive     = params::modDestIndex (id::filter1Drive);

                l.ampA = params::modDestIndex (id::ampAttack);
                l.ampD = params::modDestIndex (id::ampDecay);
                l.ampS = params::modDestIndex (id::ampSustain);
                l.ampR = params::modDestIndex (id::ampRelease);

                l.env2A = params::modDestIndex (id::envParam (2, "attack"));
                l.env2D = params::modDestIndex (id::envParam (2, "decay"));
                l.env2S = params::modDestIndex (id::envParam (2, "sustain"));
                l.env2R = params::modDestIndex (id::envParam (2, "release"));

                l.env3A = params::modDestIndex (id::envParam (3, "attack"));
                l.env3D = params::modDestIndex (id::envParam (3, "decay"));
                l.env3S = params::modDestIndex (id::envParam (3, "sustain"));
                l.env3R = params::modDestIndex (id::envParam (3, "release"));

                l.chaosDepth = params::modDestIndex (id::chaos::depth);
                l.chaosRate  = params::modDestIndex (id::chaos::rate);
                l.chaosMix   = params::modDestIndex (id::chaos::mix);

                return l;
            }();

            return lookup;
        }
    };

    const juce::NormalisableRange<float>& destRange (int destIndex)
    {
        return params::modDestinations()[(size_t) destIndex].def->range;
    }

    float denorm (const float* values, int destIndex)
    {
        return destRange (destIndex).convertFrom0to1 (values[destIndex]);
    }
}

ArsenalVoice::ArsenalVoice (const SharedState& sharedState)
    : shared (sharedState)
{
    DestLookup::get();  // resolve indices before the audio thread needs them
}

bool ArsenalVoice::canPlaySound (juce::SynthesiserSound* sound)
{
    return dynamic_cast<ArsenalSound*> (sound) != nullptr;
}

void ArsenalVoice::setCurrentPlaybackSampleRate (double newRate)
{
    juce::SynthesiserVoice::setCurrentPlaybackSampleRate (newRate);

    if (newRate > 0.0)
    {
        for (auto& osc : oscs)
            osc.prepare (newRate);
        for (auto& lfo : lfos)
            lfo.prepare (newRate);
        filter.prepare (newRate);
        ampEnv.setSampleRate (newRate);
        env2.setSampleRate (newRate);
        env3.setSampleRate (newRate);
    }
}

float ArsenalVoice::baseValue (int destIndex) const
{
    return denorm (shared.baseNorm.data(), destIndex);
}

void ArsenalVoice::startNote (int midiNoteNumber, float noteVelocity,
                              juce::SynthesiserSound*, int currentPitchWheelPosition)
{
    const auto& lookup = DestLookup::get();

    currentNote = midiNoteNumber;
    velocity = noteVelocity;
    pitchBendSemitones = 2.0f * ((float) currentPitchWheelPosition - 8192.0f) / 8192.0f;

    for (int s = 0; s < params::numOscSlots; ++s)
        oscs[(size_t) s].noteOn (shared.slots[(size_t) s].phaseMode,
                                 shared.slots[(size_t) s].phase, &random);

    for (int i = 0; i < params::numLFOs; ++i)
        lfos[(size_t) i].noteOn (shared.lfo[(size_t) i]);

    chaosGen.prepare (random);
    chaosDepth = baseValue (lookup.chaosDepth);
    chaosRate = baseValue (lookup.chaosRate);
    chaosMix = baseValue (lookup.chaosMix);

    filter.reset();

    // Envelopes start from base (unmodulated) values; the first chunk refresh
    // applies modulation.
    ampEnv.setParameters ({ baseValue (lookup.ampA), baseValue (lookup.ampD),
                            baseValue (lookup.ampS), baseValue (lookup.ampR) });
    env2.setParameters ({ baseValue (lookup.env2A), baseValue (lookup.env2D),
                          baseValue (lookup.env2S), baseValue (lookup.env2R) });
    env3.setParameters ({ baseValue (lookup.env3A), baseValue (lookup.env3D),
                          baseValue (lookup.env3S), baseValue (lookup.env3R) });

    ampEnv.noteOn();
    env2.noteOn();
    env3.noteOn();
    ampEnvLast = 0.0f;
}

void ArsenalVoice::stopNote (float, bool allowTailOff)
{
    if (allowTailOff)
    {
        ampEnv.noteOff();
        env2.noteOff();
        env3.noteOff();
    }
    else
    {
        ampEnv.reset();
        env2.reset();
        env3.reset();
        clearCurrentNote();
    }
}

void ArsenalVoice::pitchWheelMoved (int newPitchWheelValue)
{
    pitchBendSemitones = 2.0f * ((float) newPitchWheelValue - 8192.0f) / 8192.0f;
}

void ArsenalVoice::computeChunk (int blockOffset, int chunkLen)
{
    const auto& lookup = DestLookup::get();
    const auto sampleRate = getSampleRate();

    // --- Organic Chaos walkers ----------------------------------------------
    // Advanced with last chunk's effective depth/rate/mix (chaos feeds the
    // matrix that can modulate those — one-chunk latency breaks the cycle).
    const auto& ch = shared.chaos;
    const auto chaosActive = ch.enabled && chaosMix > 0.0f && chaosDepth > 0.0f;
    const auto chaosScale = chaosActive ? chaosDepth * chaosMix : 0.0f;

    if (chaosActive)
        chaosGen.process (chaosRate, (double) chunkLen / sampleRate, random);

    // --- Modulation sources -------------------------------------------------
    float src[params::numModSources] {};
    src[(int) params::ModSource::env1] = ampEnvLast;
    src[(int) params::ModSource::chaos] = chaosGen.value (ChaosGenerator::matrixSource)
                                        * chaosScale;

    // Advance env2/3 through the chunk; their end value drives this chunk.
    float env2Value = 0.0f, env3Value = 0.0f;
    for (int i = 0; i < chunkLen; ++i)
    {
        env2Value = env2.getNextSample();
        env3Value = env3.getNextSample();
    }
    src[(int) params::ModSource::env2] = env2Value;
    src[(int) params::ModSource::env3] = env3Value;

    for (int i = 0; i < params::numLFOs; ++i)
    {
        const auto& p = shared.lfo[(size_t) i];
        const auto inc = (double) LFO::effectiveRateHz (p, shared.bpm) / sampleRate;
        const auto globalPhase = shared.lfoGlobalPhase[(size_t) i] + inc * blockOffset;
        src[(int) params::ModSource::lfo1 + i] =
            lfos[(size_t) i].processChunk (p, chunkLen, shared.bpm, globalPhase, random);
    }

    for (int m = 0; m < params::numMacros; ++m)
        src[(int) params::ModSource::macro1 + m] = shared.macros[(size_t) m];

    src[(int) params::ModSource::velocity]   = velocity;
    src[(int) params::ModSource::modWheel]   = shared.modWheel;
    src[(int) params::ModSource::aftertouch] = shared.aftertouch;

    // --- Apply routes in normalized space -----------------------------------
    float eff[params::maxModDests];
    std::copy (shared.baseNorm.begin(),
               shared.baseNorm.begin() + params::numModDests(), eff);

    for (int r = 0; r < shared.numActiveRoutes; ++r)
    {
        const auto& route = shared.routes[(size_t) r];
        eff[route.destIndex] += src[route.source] * route.depth;
    }

    for (int d = 0; d < params::numModDests(); ++d)
        eff[d] = juce::jlimit (0.0f, 1.0f, eff[d]);

    // Cache effective chaos controls for next chunk's walker advance.
    chaosDepth = denorm (eff, lookup.chaosDepth);
    chaosRate = denorm (eff, lookup.chaosRate);
    chaosMix = denorm (eff, lookup.chaosMix);

    // Voice-wide chaos drives applied in the render loop.
    chaosAmpGain = chaosActive && ch.ampOn
                 ? 1.0f + chaosGen.value (ChaosGenerator::amp) * ch.ampAmount * 0.5f * chaosScale
                 : 1.0f;
    satDrive = chaosActive && ch.satOn
             ? ch.saturation * chaosScale
               * (0.6f + 0.4f * chaosGen.value (ChaosGenerator::saturation))
             : 0.0f;
    distDrive = chaosActive && ch.distOn
              ? ch.distortion * chaosScale
                * (0.6f + 0.4f * chaosGen.value (ChaosGenerator::distortion))
              : 0.0f;

    // --- Configure DSP from effective values --------------------------------
    for (int s = 0; s < params::numOscSlots; ++s)
    {
        const auto& stat = shared.slots[(size_t) s];
        if (! stat.enabled)
            continue;

        const auto& d = lookup.slots[(size_t) s];

        const auto chaosPitch = chaosActive && ch.pitchOn
                              ? chaosGen.slotPitch (s) * ch.pitchAmountCents * 0.01f * chaosScale
                              : 0.0f;
        const auto chaosPos = chaosActive && ch.positionOn
                            ? chaosGen.slotPosition (s) * ch.positionAmount * chaosScale
                            : 0.0f;
        const auto chaosPhase = chaosActive && ch.phaseOn
                              ? chaosGen.slotPhase (s) * ch.phaseAmount * chaosScale
                              : 0.0f;

        const auto note = (float) currentNote
                        + denorm (eff, d.coarse)
                        + denorm (eff, d.fine) * 0.01f
                        + pitchBendSemitones
                        + chaosPitch;

        oscs[(size_t) s].updateBlock ({
            stat.table,
            440.0f * std::exp2 ((note - 69.0f) / 12.0f),
            juce::jlimit (0.0f, 1.0f, denorm (eff, d.position) + chaosPos),
            stat.unisonCount,
            denorm (eff, d.unisonDetune),
            denorm (eff, d.unisonBlend),
            denorm (eff, d.unisonWidth),
            denorm (eff, d.pan),
            chaosPhase,
        });

        slotGains[(size_t) s] = juce::Decibels::decibelsToGain (denorm (eff, d.level), -60.0f);
    }

    filter.setParams (shared.filterType,
                      denorm (eff, lookup.cutoff),
                      denorm (eff, lookup.resonance),
                      denorm (eff, lookup.drive));

    ampEnv.setParameters ({ denorm (eff, lookup.ampA), denorm (eff, lookup.ampD),
                            denorm (eff, lookup.ampS), denorm (eff, lookup.ampR) });
    env2.setParameters ({ denorm (eff, lookup.env2A), denorm (eff, lookup.env2D),
                          denorm (eff, lookup.env2S), denorm (eff, lookup.env2R) });
    env3.setParameters ({ denorm (eff, lookup.env3A), denorm (eff, lookup.env3D),
                          denorm (eff, lookup.env3S), denorm (eff, lookup.env3R) });
}

void ArsenalVoice::renderNextBlock (juce::AudioBuffer<float>& outputBuffer,
                                    int startSample, int numSamples)
{
    if (! ampEnv.isActive())
        return;

    auto* left = outputBuffer.getWritePointer (0, startSample);
    auto* right = outputBuffer.getNumChannels() > 1
                ? outputBuffer.getWritePointer (1, startSample) : nullptr;

    int rendered = 0;
    while (rendered < numSamples)
    {
        const auto chunkLen = juce::jmin (chunkSize, numSamples - rendered);
        computeChunk (startSample + rendered, chunkLen);

        for (int i = 0; i < chunkLen; ++i)
        {
            float sumL = 0.0f, sumR = 0.0f;

            for (int s = 0; s < params::numOscSlots; ++s)
            {
                if (! shared.slots[(size_t) s].enabled)
                    continue;

                const auto smp = oscs[(size_t) s].getNextSample();
                sumL += smp.left * slotGains[(size_t) s];
                sumR += smp.right * slotGains[(size_t) s];
            }

            const auto envValue = ampEnv.getNextSample();
            ampEnvLast = envValue;
            const auto gain = envValue * (0.2f + 0.8f * velocity) * chaosAmpGain;

            auto outL = filter.processSample (0, sumL);
            auto outR = right != nullptr ? filter.processSample (1, sumR) : 0.0f;

            // Chaos saturation: warm tanh, level-compensated.
            if (satDrive > 0.001f)
            {
                const auto k = 1.0f + 5.0f * satDrive;
                const auto norm = 1.0f / std::tanh (k * 0.7f);
                outL = std::tanh (outL * k) * 0.7f * norm;
                outR = std::tanh (outR * k) * 0.7f * norm;
            }

            // Chaos distortion: harder cubic clip with a touch of asymmetry
            // for edgier, even-harmonic character.
            if (distDrive > 0.001f)
            {
                const auto k = 1.0f + 12.0f * distDrive;
                auto shape = [k] (float x)
                {
                    auto v = juce::jlimit (-1.0f, 1.0f, x * k * 0.5f + 0.08f * x * x);
                    return (1.5f * v - 0.5f * v * v * v) / (0.5f * k + 0.5f);
                };
                outL = shape (outL);
                outR = shape (outR);
            }

            const auto n = rendered + i;
            left[n] += outL * gain;
            if (right != nullptr)
                right[n] += outR * gain;

            if (! ampEnv.isActive())
            {
                clearCurrentNote();
                return;
            }
        }

        rendered += chunkLen;
    }
}

} // namespace arsenal::dsp
