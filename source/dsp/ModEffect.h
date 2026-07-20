#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

namespace spa::dsp
{

// Switchable modulation effect: Phaser (cascaded first-order allpass, variable
// stages) or Flanger (modulated short delay with bipolar feedback). Stereo,
// with an L/R LFO phase spread. Fed already-resolved rate in Hz by the FXChain.
class ModEffect
{
public:
    enum class Type { phaser, flanger };

    void prepare (double sr, int /*maxBlock*/)
    {
        sampleRate = sr;
        reset();
    }

    void reset()
    {
        for (auto& ch : channels)
            ch = {};
        lfoPhase = 0.0f;
    }

    struct Params
    {
        Type type = Type::phaser;
        float rateHz = 0.5f;
        float depth = 0.5f;        // 0..1
        float feedback = 0.3f;     // -0.95..0.95 (flanger may be bipolar)
        int stages = 6;            // phaser allpass count
        float centreHz = 800.0f;   // phaser sweep centre
        float manualMs = 3.0f;     // flanger base delay
        float spread = 0.5f;       // 0..1 L/R LFO phase offset
        float mix = 0.5f;          // dry/wet
    };

    void process (juce::AudioBuffer<float>& buffer, const Params& p)
    {
        const int n = buffer.getNumSamples();
        const int numCh = juce::jmin (2, buffer.getNumChannels());
        const float phaseInc = (float) (p.rateHz / sampleRate);
        const float spreadOffset = 0.5f * juce::jlimit (0.0f, 1.0f, p.spread);

        for (int i = 0; i < n; ++i)
        {
            for (int ch = 0; ch < numCh; ++ch)
            {
                auto& st = channels[ch];
                const float lp = lfoPhase + (ch == 1 ? spreadOffset : 0.0f);
                const float lfo = 0.5f + 0.5f * std::sin (lp * juce::MathConstants<float>::twoPi);

                float* d = buffer.getWritePointer (ch);
                const float dry = d[i];
                float wet = dry;

                if (p.type == Type::phaser)
                    wet = processPhaser (st, dry, lfo, p);
                else
                    wet = processFlanger (st, dry, lfo, p);

                d[i] = dry + (wet - dry) * juce::jlimit (0.0f, 1.0f, p.mix);
            }
            lfoPhase += phaseInc;
            if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
        }
    }

private:
    static constexpr int maxStages = 12;
    static constexpr int delayLen = 2048;   // >~40 ms at 48k

    struct ChannelState
    {
        float ap[maxStages] = {};   // allpass state (phaser)
        float fbLast = 0.0f;
        float dl[delayLen] = {};     // flanger delay line
        int dlWrite = 0;
    };

    float processPhaser (ChannelState& st, float in, float lfo, const Params& p)
    {
        // Sweep the allpass break frequency around the centre by +/- depth.
        const float minHz = juce::jmax (40.0f, p.centreHz * (1.0f - 0.9f * p.depth));
        const float maxHz = juce::jmin (0.45f * (float) sampleRate,
                                        p.centreHz * (1.0f + 2.0f * p.depth));
        const float fc = minHz + (maxHz - minHz) * lfo;
        const float t = std::tan (juce::MathConstants<float>::pi * fc / (float) sampleRate);
        const float a = (t - 1.0f) / (t + 1.0f);   // first-order allpass coeff

        const int stages = juce::jlimit (1, maxStages, p.stages);
        float x = in + st.fbLast * juce::jlimit (-0.95f, 0.95f, p.feedback);
        for (int s = 0; s < stages; ++s)
        {
            const float y = a * x + st.ap[s];
            st.ap[s] = x - a * y;
            x = y;
        }
        st.fbLast = x;
        return x;
    }

    float processFlanger (ChannelState& st, float in, float lfo, const Params& p)
    {
        const float baseMs = juce::jlimit (0.1f, 20.0f, p.manualMs);
        const float sweepMs = 0.5f + 9.0f * p.depth;
        const float delayMs = baseMs + sweepMs * lfo;
        const float delaySamps = juce::jlimit (1.0f, (float) (delayLen - 2),
                                               delayMs * 0.001f * (float) sampleRate);

        const float fb = juce::jlimit (-0.95f, 0.95f, p.feedback);
        st.dl[st.dlWrite] = in + st.fbLast * fb;

        // Linear-interpolated read.
        float rp = (float) st.dlWrite - delaySamps;
        while (rp < 0.0f) rp += (float) delayLen;
        const int i0 = (int) rp;
        const float frac = rp - (float) i0;
        const int i1 = (i0 + 1) % delayLen;
        const float out = st.dl[i0] + frac * (st.dl[i1] - st.dl[i0]);

        st.fbLast = out;
        st.dlWrite = (st.dlWrite + 1) % delayLen;
        return out;
    }

    double sampleRate = 48000.0;
    float lfoPhase = 0.0f;
    ChannelState channels[2];
};

} // namespace spa::dsp
