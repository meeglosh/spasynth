#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <vector>
#include <cmath>

namespace spa::dsp
{

// Dattorro plate reverb, re-implemented from the published algorithm
// description (Jon Dattorro, "Effect Design Part 1: Reverberator and Other
// Filters", Journal of the Audio Engineering Society, vol. 45, no. 9,
// September 1997) -- NOT ported from any GPL/LGPL codebase (Zita-Rev1,
// MVerb, Freeverb3 etc. were never opened for this file). The delay-line
// lengths below are the ones published in Dattorro's paper (a set of
// numbers, not source code); the surrounding C++ -- buffer management, the
// fractional-read wrap guard, the LFO rotator, mode voicing, tone/width
// stage -- is original to this codebase.
//
// Topology: mono sum -> input bandwidth filter (one-pole LP) -> four fixed
// allpass diffusers in series -> a two-branch "tank": each branch is a
// modulated allpass (pitch-thickening chirp), a long fixed delay, a
// one-pole damping filter, a decay-gain multiply (sets RT60), a fixed
// decay-diffusion allpass, and a second long fixed delay whose output
// feeds the *other* branch's input next pass (the classic figure-eight
// cross-feed). Stereo output taps are drawn from several points across
// both branches so L/R decorrelate without an explicit cross-mix matrix.
class PlateReverb
{
public:
    enum class Mode { hall, plate, chamber, room, spring };

    struct Params
    {
        int mode = 0;
        float preDelayMs = 20.0f;
        float size = 0.5f;         // 0..1 room size (delay scale)
        float decaySec = 2.0f;     // RT60
        float hfDamp = 0.5f;       // 0..1
        float modDepth = 0.2f;     // 0..1 tank modulation depth
        float lowCutHz = 20.0f;
        float highCutHz = 12000.0f;
        float width = 1.0f;        // 0..1
        float mix = 0.3f;          // linear dry/wet: dry = 1-mix, wet = mix
    };

    void prepare (double sr, int /*maxBlock*/)
    {
        sampleRate = sr;
        // Reference rate the published lengths are stated for; scale by
        // sr/refRate and by the largest possible size factor (mode*user)
        // plus modulation headroom so no runtime resize is ever needed.
        constexpr double refRate = 29761.0;
        const double maxScale = (sr / refRate) * kMaxSizeScale;

        preBuf.assign ((size_t) (0.25 * sr) + 8, 0.0f);
        preW = 0;

        for (int i = 0; i < numInputAP; ++i)
            inputAP[(size_t) i].resize ((size_t) (inputAPLen[i] * maxScale) + kModMargin);

        for (int br = 0; br < 2; ++br)
        {
            auto& b = branch[(size_t) br];
            b.modAP.resize ((size_t) (modAPLen[br] * maxScale) + kModMargin);
            b.delay1.resize ((size_t) (delay1Len[br] * maxScale) + kModMargin);
            b.decayAP.resize ((size_t) (decayAPLen[br] * maxScale) + kModMargin);
            b.delay2.resize ((size_t) (delay2Len[br] * maxScale) + kModMargin);
        }

        reset();
    }

    void reset()
    {
        std::fill (preBuf.begin(), preBuf.end(), 0.0f);
        preW = 0;
        for (auto& ap : inputAP) ap.clear();
        for (auto& b : branch) b.clear();
        // Offset the two branches' modulation phases so they decorrelate
        // from the very first block instead of chirping in lockstep.
        branch[0].lfoPhase = 0.0f;
        branch[1].lfoPhase = 0.37f;
        bwState = 0.0f;
        lowState = {}; highState = {};
    }

    void process (juce::AudioBuffer<float>& buffer, const Params& p)
    {
        const int n = buffer.getNumSamples();
        const int numCh = juce::jmin (2, buffer.getNumChannels());
        const auto mode = (Mode) juce::jlimit (0, 4, p.mode);

        const float sizeScale = 0.3f + 1.7f * juce::jlimit (0.0f, 1.0f, p.size) * modeSizeMul (mode);
        const float rt60 = juce::jmax (0.15f, p.decaySec * modeDecayMul (mode));
        // dampC is a one-pole TRANSPARENCY coefficient (state += dampC*(x -
        // state), same convention as the bandwidth filter below): dampC
        // near 1 = almost no filtering (bright), dampC near 0 = heavy
        // smoothing (dark). hfDamp is the user-facing "more damping = MORE
        // filtering/darker" knob, so it must map INVERSELY -- hfDamp=0 ->
        // dampC near 1 (transparent), hfDamp=1 -> dampC near its floor
        // (dark). An earlier version used dampC = hfDamp*modeDampMul
        // directly, which is backwards (turning damping UP made the tail
        // brighter, not darker) and, compounded every recirculation pass,
        // needlessly darkened and quietened the default-settings tail
        // (measured: spectral centroid of the first arriving tail material
        // was under 200 Hz for a broadband impulse at hfDamp=0.5, and the
        // wet tap needed a ~38x makeup gain to reach a sensible level).
        const float dampAmount = juce::jlimit (0.0f, 1.0f, p.hfDamp * modeDampMul (mode));
        const float dampC = juce::jlimit (0.03f, 0.999f, 1.0f - dampAmount * 0.97f);
        const float modAmt = juce::jlimit (0.0f, 1.0f, p.modDepth) * modeModMul (mode);
        const float diffusionMul = modeDiffusionMul (mode);
        const float bwCoef = modeBandwidth (mode);
        const float extraPreMs = modePreDelayAddMs (mode);

        const double refRate = 29761.0;
        const double rateScale = sampleRate / refRate;
        const double scale = rateScale * (double) sizeScale;

        int preSamps = juce::jlimit (0, (int) preBuf.size() - 2,
                                     (int) ((p.preDelayMs + extraPreMs) * 0.001f * (float) sampleRate));

        int inLen[numInputAP];
        for (int i = 0; i < numInputAP; ++i)
            inLen[i] = juce::jlimit (4, (int) inputAP[(size_t) i].buf.size() - 4,
                                     (int) (inputAPLen[i] * rateScale));
        const float inG[numInputAP] = { 0.75f * diffusionMul, 0.75f * diffusionMul,
                                         0.625f * diffusionMul, 0.625f * diffusionMul };

        BranchGeom geom[2];
        for (int br = 0; br < 2; ++br)
        {
            geom[br].modLen = juce::jlimit (8, (int) branch[(size_t) br].modAP.buf.size() - 8,
                                            (int) (modAPLen[br] * scale));
            geom[br].d1Len  = juce::jlimit (4, (int) branch[(size_t) br].delay1.buf.size() - 4,
                                            (int) (delay1Len[br] * scale));
            geom[br].decayApLen = juce::jlimit (4, (int) branch[(size_t) br].decayAP.buf.size() - 4,
                                                (int) (decayAPLen[br] * scale));
            geom[br].d2Len  = juce::jlimit (4, (int) branch[(size_t) br].delay2.buf.size() - 4,
                                            (int) (delay2Len[br] * scale));
            const int loopLen = geom[br].modLen + geom[br].d1Len + geom[br].decayApLen + geom[br].d2Len;
            geom[br].decayGain = std::pow (10.0f, -3.0f * ((float) loopLen / (float) sampleRate) / rt60);
        }

        // Tail-modulation rotators (see FDNReverb.h -- same technique: seed a
        // unit vector once per block, advance by rotation each sample instead
        // of calling std::sin() per sample per branch). Rate scales with the
        // SAME per-mode character multiplier as the modulation depth
        // (modeModMul) rather than the raw mode index, so Spring's "boing"
        // -- meant to be both deeper AND distinctly faster-chirping than the
        // other modes -- actually reads that way (a plain index-based rate
        // barely separated Spring from Plate).
        const float modAngleInc = (0.5f + 0.6f * modeModMul (mode)) / (float) sampleRate
                                 * juce::MathConstants<float>::twoPi;
        const float cosInc = std::cos (modAngleInc), sinInc = std::sin (modAngleInc);
        std::array<float, 2> lfoRe, lfoIm;
        for (int br = 0; br < 2; ++br)
        {
            const float phase0 = branch[(size_t) br].lfoPhase * juce::MathConstants<float>::twoPi;
            lfoRe[(size_t) br] = std::cos (phase0);
            lfoIm[(size_t) br] = std::sin (phase0);
        }

        const float lowCoef = onePoleCoef (p.lowCutHz);
        const float highCoef = onePoleCoef (juce::jmax (500.0f, p.highCutHz));
        const float width = juce::jlimit (0.0f, 1.0f, p.width);
        // Linear dry/wet: 0 = untouched dry, 1 = pure wet, 0.5 = exact half.
        const float mix = juce::jlimit (0.0f, 1.0f, p.mix);
        const float dryG = 1.0f - mix, wetG = mix;

        float* L = buffer.getWritePointer (0);
        float* R = numCh > 1 ? buffer.getWritePointer (1) : L;

        // Wet-path injection/output scale, tuned so a 0.5-amplitude burst
        // yields wet peaks in the same ~1.5-1.8 range the 1.0.6-normalised
        // FDN produced (see reverbMixTest), keeping factory-preset loudness
        // stable across the engine swap.
        //
        // A single fixed outTapScale is NOT physically sound here: the raw
        // tap's natural level (before this scale) is dominated by
        // geom[].decayGain, the RT60 knob's own per-pass attenuation --
        // short RT60 loses a lot per pass (quiet raw tap), long RT60 loses
        // almost nothing (loud raw tap). A fixed multiplier calibrated
        // against one RT60 either starves short-decay presets or lets
        // long-decay/low-damping ones blow well past the stability bound
        // (measured: a scale tuned for RT60=2s pushed the RT60=10s/heavy-
        // mod stability probe to 5-8x its <3.0 bound). Compensate by
        // dividing by the branches' own average decayGain so the SCALED
        // tap's level stays roughly RT60-independent, matching how a real
        // room's early reflections don't get louder just because the room
        // also happens to be more reverberant/absorptive.
        constexpr float injectScale = 0.62f;
        const float avgDecayGain = 0.5f * (geom[0].decayGain + geom[1].decayGain);
        constexpr float baseTapScale = 8.5f;
        const float outTapScale = baseTapScale / juce::jmax (0.12f, avgDecayGain);

        for (int s = 0; s < n; ++s)
        {
            const float dryL = L[s], dryR = R[s];
            float x = 0.5f * (dryL + dryR);

            // Pre-delay.
            preBuf[(size_t) preW] = x;
            int pr = preW - preSamps; if (pr < 0) pr += (int) preBuf.size();
            x = preBuf[(size_t) pr];
            preW = (preW + 1) % (int) preBuf.size();

            // Input bandwidth filter (darkens/brightens what enters the tank).
            bwState += bwCoef * (x - bwState);
            x = bwState;

            // Four series input diffusers.
            for (int i = 0; i < numInputAP; ++i)
                x = allpass (inputAP[(size_t) i], inLen[i], inG[i], x);

            const float diffOut = x * injectScale;

            // Cross-feed taps from last sample's tank output (read at the
            // SAME geom[].d2Len tap branchOut[] itself reads, before this
            // sample's write -- i.e. exactly the previous pass's output,
            // not the full (much larger) allocated buffer capacity, which
            // is sized for the largest possible size/mode and would silence
            // the feedback loop entirely at smaller size settings).
            const float crossFromB = branch[1].delay2.readInt (geom[1].d2Len);
            const float crossFromA = branch[0].delay2.readInt (geom[0].d2Len);

            float branchOut[2];
            for (int br = 0; br < 2; ++br)
            {
                auto& b = branch[(size_t) br];
                // NOTE: crossFromA/crossFromB already carry their own
                // branch's decayGain (applied once, below, where each
                // branch's own damped signal is produced) -- do NOT
                // multiply by decayGain again here, or the coupled A<->B
                // loop attenuates twice per lap and the tail decays far
                // faster than the requested RT60 (measured ~0.68x target
                // before this fix).
                const float in = diffOut + (br == 0 ? crossFromB : crossFromA);

                // Modulated allpass: fractional read offset by the rotator's
                // imaginary part, scaled to a small sample excursion.
                const float modExcursion = 1.0f + 16.0f * modAmt * lfoIm[(size_t) br];
                float y1 = modulatedAllpass (b.modAP, geom[br].modLen, modExcursion, -0.7f, in);

                b.delay1.write (y1);
                const float y2 = b.delay1.readInt (geom[br].d1Len);

                b.damp += dampC * (y2 - b.damp);
                const float damped = b.damp * geom[br].decayGain;

                const float y3 = allpass (b.decayAP, geom[br].decayApLen, 0.6f, damped);
                b.delay2.write (y3);
                branchOut[br] = b.delay2.readInt (geom[br].d2Len);

                // Advance this branch's rotator.
                const float newRe = lfoRe[(size_t) br] * cosInc - lfoIm[(size_t) br] * sinInc;
                const float newIm = lfoRe[(size_t) br] * sinInc + lfoIm[(size_t) br] * cosInc;
                lfoRe[(size_t) br] = newRe; lfoIm[(size_t) br] = newIm;
            }

            if ((s & 4095) == 4095)
                for (int br = 0; br < 2; ++br)
                {
                    const float invMag = 1.0f / std::sqrt (lfoRe[(size_t) br] * lfoRe[(size_t) br]
                                                           + lfoIm[(size_t) br] * lfoIm[(size_t) br]);
                    lfoRe[(size_t) br] *= invMag; lfoIm[(size_t) br] *= invMag;
                }

            // Stereo taps: mix each branch's own delay2 output with a tap
            // from the *other* branch's decay-diffuser stage, so L and R
            // draw from decorrelated points in the tank.
            const float tapA = branch[0].decayAP.readInt (juce::jmax (1, geom[0].decayApLen / 3));
            const float tapB = branch[1].decayAP.readInt (juce::jmax (1, geom[1].decayApLen / 3));
            // Additive taps, not subtractive: branchOut and the OTHER
            // branch's decayAP tap are both derived from the same original
            // diffuser output through overlapping paths and correlate
            // heavily at these short lags, so subtracting them (an earlier
            // version of this code did) mostly cancelled rather than
            // decorrelated, leaving the wet path far too quiet. A small
            // additive cross-blend still pulls L and R apart (the width
            // test covers it) without gutting the level.
            float wetL = outTapScale * (branchOut[0] + 0.35f * tapB);
            float wetR = outTapScale * (branchOut[1] + 0.35f * tapA);

            lowState[0] += lowCoef * (wetL - lowState[0]); wetL -= lowState[0];
            lowState[1] += lowCoef * (wetR - lowState[1]); wetR -= lowState[1];
            highState[0] += highCoef * (wetL - highState[0]); wetL = highState[0];
            highState[1] += highCoef * (wetR - highState[1]); wetR = highState[1];

            const float mid = 0.5f * (wetL + wetR);
            const float side = 0.5f * (wetL - wetR) * width;
            wetL = mid + side; wetR = mid - side;

            L[s] = dryL * dryG + wetL * wetG;
            if (numCh > 1) R[s] = dryR * dryG + wetR * wetG;
        }

        // Persist each branch's rotator phase for the next block's reseed.
        for (int br = 0; br < 2; ++br)
            branch[(size_t) br].lfoPhase = std::atan2 (lfoIm[(size_t) br], lfoRe[(size_t) br])
                                          / juce::MathConstants<float>::twoPi;
    }

private:
    static constexpr int numInputAP = 4;
    static constexpr int kModMargin = 96;
    // Largest possible sizeScale = 0.3 + 1.7*1.0*modeSizeMul(hall=1.3).
    static constexpr double kMaxSizeScale = 0.3 + 1.7 * 1.3;

    // Published Dattorro plate constants, samples at the paper's 29761 Hz
    // reference rate.
    static constexpr float inputAPLen[numInputAP] = { 142.0f, 107.0f, 379.0f, 277.0f };
    static constexpr float modAPLen[2]   = { 672.0f, 908.0f };
    static constexpr float delay1Len[2]  = { 4453.0f, 4217.0f };
    static constexpr float decayAPLen[2] = { 1800.0f, 2656.0f };
    static constexpr float delay2Len[2]  = { 3720.0f, 3163.0f };

    // A simple modulo ring buffer with the float-wrap-guard read pattern
    // established (and hard-learned) in FDNReverb.h: after computing a
    // fractional read index, `while (i0 >= sz) i0 -= sz;` catches the case
    // where a position a hair below zero rounds, at float precision, up to
    // exactly `sz` -- one element past the buffer, reading whatever the
    // allocator put next on the heap. Always applied here, on every read.
    struct Ring
    {
        std::vector<float> buf;
        int w = 0;

        void resize (size_t n) { buf.assign (juce::jmax ((size_t) 8, n), 0.0f); w = 0; }
        void clear() { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }

        void write (float v) { buf[(size_t) w] = v; w = (w + 1) % (int) buf.size(); }

        // Value about to be overwritten by the next write() -- the oldest
        // sample in the ring, i.e. exactly `buf.size()` samples old.
        float peekWrite() const { return buf[(size_t) w]; }

        float readInt (int back) const
        {
            const int sz = (int) buf.size();
            int rp = w - back;
            while (rp < 0) rp += sz;
            while (rp >= sz) rp -= sz;
            return buf[(size_t) rp];
        }

        float readFrac (float back) const
        {
            const int sz = (int) buf.size();
            float rp = (float) w - back;
            while (rp < 0.0f) rp += (float) sz;
            int i0 = (int) rp;
            const float fr = rp - (float) i0;
            while (i0 >= sz) i0 -= sz;           // float-wrap guard (see class comment)
            const int i1 = (i0 + 1) % sz;
            return buf[(size_t) i0] + fr * (buf[(size_t) i1] - buf[(size_t) i0]);
        }
    };

    struct Branch
    {
        Ring modAP, delay1, decayAP, delay2;
        float damp = 0.0f;
        float lfoPhase = 0.0f;

        void resize (size_t modN, size_t d1N, size_t decN, size_t d2N)
        {
            modAP.resize (modN); delay1.resize (d1N); decayAP.resize (decN); delay2.resize (d2N);
        }
        void clear() { modAP.clear(); delay1.clear(); decayAP.clear(); delay2.clear(); damp = 0.0f; }
    };

    struct BranchGeom { int modLen = 0, d1Len = 0, decayApLen = 0, d2Len = 0; float decayGain = 0.0f; };

    static float allpass (Ring& d, int len, float g, float in)
    {
        const float vN = d.readInt (len);
        const float y = -g * in + vN;
        d.write (in + g * y);
        return y;
    }

    static float modulatedAllpass (Ring& d, int baseLen, float excursion, float g, float in)
    {
        // Read the feedback path at a fractionally-modulated length (the
        // "chirp"/pitch-thickening character); write at the fixed length so
        // the ring never has to grow.
        const float readBack = juce::jlimit (1.0f, (float) baseLen + 80.0f, (float) baseLen - (excursion - 1.0f));
        const float vN = d.readFrac (readBack);
        const float y = -g * in + vN;
        d.write (in + g * y);
        return y;
    }

    static float onePoleCoef (float hz)
    {
        return juce::jlimit (0.0001f, 0.999f,
                             1.0f - std::exp (-juce::MathConstants<float>::twoPi * hz / 48000.0f));
    }

    static float modeSizeMul (Mode m)
    {
        switch (m)
        {
            case Mode::hall:    return 1.3f;
            case Mode::plate:   return 1.0f;
            case Mode::chamber: return 0.85f;
            case Mode::room:    return 0.6f;
            case Mode::spring:  return 0.45f;
        }
        return 1.0f;
    }
    static float modeDecayMul (Mode m)
    {
        switch (m)
        {
            case Mode::hall:    return 1.2f;
            case Mode::plate:   return 1.0f;
            case Mode::chamber: return 0.85f;
            case Mode::room:    return 0.5f;
            case Mode::spring:  return 0.55f;
        }
        return 1.0f;
    }
    static float modeDampMul (Mode m)
    {
        switch (m)
        {
            case Mode::hall:    return 1.0f;
            case Mode::plate:   return 0.55f;
            case Mode::chamber: return 0.85f;
            case Mode::room:    return 1.2f;
            case Mode::spring:  return 0.35f;
        }
        return 1.0f;
    }
    static float modeModMul (Mode m)
    {
        switch (m)
        {
            case Mode::hall:    return 1.0f;
            case Mode::plate:   return 0.35f;
            case Mode::chamber: return 0.8f;
            case Mode::room:    return 0.5f;
            case Mode::spring:  return 4.0f;
        }
        return 1.0f;
    }
    static float modeDiffusionMul (Mode m)
    {
        switch (m)
        {
            case Mode::hall:    return 1.0f;
            case Mode::plate:   return 1.0f;
            case Mode::chamber: return 1.0f;
            case Mode::room:    return 0.55f;
            case Mode::spring:  return 0.7f;
        }
        return 1.0f;
    }
    static float modeBandwidth (Mode m)
    {
        switch (m)
        {
            case Mode::hall:    return 0.999f;
            case Mode::plate:   return 0.9995f;
            case Mode::chamber: return 0.9993f;
            case Mode::room:    return 0.9997f;
            case Mode::spring:  return 0.9999f;
        }
        return 0.9995f;
    }
    static float modePreDelayAddMs (Mode m)
    {
        switch (m)
        {
            case Mode::hall:    return 15.0f;
            case Mode::plate:   return 0.0f;
            case Mode::chamber: return 3.0f;
            case Mode::room:    return 0.0f;
            case Mode::spring:  return 0.0f;
        }
        return 0.0f;
    }

    double sampleRate = 48000.0;
    std::array<Ring, numInputAP> inputAP;
    std::vector<float> preBuf;
    int preW = 0;
    float bwState = 0.0f;
    std::array<Branch, 2> branch;
    std::array<float, 2> lowState {}, highState {};
};

} // namespace spa::dsp
