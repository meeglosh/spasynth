#include "FXChain.h"

namespace spa::dsp
{

void FXChain::prepare (double newSampleRate, int maxBlockSize)
{
    sampleRate = newSampleRate;

    const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlockSize, 2 };

    for (auto& f : toneFilters)
    {
        f.prepare ({ sampleRate, (juce::uint32) maxBlockSize, 1 });
        f.setType (juce::dsp::FirstOrderTPTFilterType::lowpass);
    }

    chorus.prepare (spec);

    delayBuffer.setSize (2, (int) (sampleRate * 4.0) + 8);
    delayBuffer.clear();
    delayWritePos = 0;
    delaySamplesSmoothed.reset (sampleRate, 0.1);

    reverb.setSampleRate (sampleRate);

    for (auto& f : eqLow)  f.prepare ({ sampleRate, (juce::uint32) maxBlockSize, 1 });
    for (auto& f : eqMid)  f.prepare ({ sampleRate, (juce::uint32) maxBlockSize, 1 });
    for (auto& f : eqHigh) f.prepare ({ sampleRate, (juce::uint32) maxBlockSize, 1 });
    lastLowGain = lastMidGain = lastHighGain = 1.0e9f;
    lastMidFreq = 0.0f;

    reset();
}

void FXChain::reset()
{
    for (auto& f : toneFilters)
        f.reset();
    chorus.reset();
    delayBuffer.clear();
    reverb.reset();
    for (auto& f : eqLow)  f.reset();
    for (auto& f : eqMid)  f.reset();
    for (auto& f : eqHigh) f.reset();
}

double FXChain::tailSeconds (const Params& p) const
{
    double tail = 0.0;

    if (p.delayEnable)
    {
        const auto time = p.delaySync
                        ? params::lfoDivisionBeats (p.delayDivision) * 60.0 / p.bpm
                        : (double) p.delayTimeMs * 0.001;
        // Feedback ring-out to roughly -60 dB.
        const auto repeats = p.delayFeedback > 0.01f
                           ? std::log (0.001) / std::log ((double) p.delayFeedback)
                           : 1.0;
        tail = juce::jlimit (0.0, 12.0, time * repeats);
    }

    if (p.reverbEnable)
        tail = juce::jmax (tail, 2.0 + 6.0 * (double) p.reverbSize);

    return tail;
}

void FXChain::process (juce::AudioBuffer<float>& buffer, const Params& params)
{
    for (const auto module : params.order)
    {
        switch (module)
        {
            case Module::distortion: if (params.distEnable)   processDistortion (buffer, params); break;
            case Module::chorus:     if (params.chorusEnable) processChorus (buffer, params); break;
            case Module::delay:      if (params.delayEnable)  processDelay (buffer, params); break;
            case Module::reverb:     if (params.reverbEnable) processReverb (buffer, params); break;
            case Module::eq:         if (params.eqEnable)     processEQ (buffer, params); break;
        }
    }
}

void FXChain::processDistortion (juce::AudioBuffer<float>& buffer, const Params& p)
{
    const auto driveGain = 1.0f + 15.0f * p.distDrive;

    for (auto& f : toneFilters)
        f.setCutoffFrequency (p.distToneHz);

    for (int ch = 0; ch < juce::jmin (2, buffer.getNumChannels()); ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        auto& tone = toneFilters[(size_t) ch];

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const auto dry = data[i];
            const auto x = dry * driveGain;

            float wet;
            switch (p.distType)
            {
                case 1:  wet = juce::jlimit (-1.0f, 1.0f, x); break;              // Hard
                case 2:  wet = std::sin (x * 1.2f); break;                        // Fold
                default: wet = std::tanh (x); break;                              // Soft
            }

            wet = tone.processSample (0, wet / std::sqrt (driveGain));
            data[i] = dry + (wet - dry) * p.distMix;
        }
    }
}

void FXChain::processChorus (juce::AudioBuffer<float>& buffer, const Params& p)
{
    chorus.setRate (p.chorusRate);
    chorus.setDepth (p.chorusDepth);
    chorus.setCentreDelay (7.0f);
    chorus.setFeedback (p.chorusFeedback);
    chorus.setMix (p.chorusMix);

    juce::dsp::AudioBlock<float> block (buffer);
    chorus.process (juce::dsp::ProcessContextReplacing<float> (block));
}

void FXChain::processDelay (juce::AudioBuffer<float>& buffer, const Params& p)
{
    const auto timeSeconds = p.delaySync
                           ? params::lfoDivisionBeats (p.delayDivision) * 60.0 / p.bpm
                           : (double) p.delayTimeMs * 0.001;
    const auto targetSamples = (float) juce::jlimit (
        32.0, (double) delayBuffer.getNumSamples() - 8.0, timeSeconds * sampleRate);
    delaySamplesSmoothed.setTargetValue (targetSamples);

    const auto bufLen = delayBuffer.getNumSamples();
    auto* bufL = delayBuffer.getWritePointer (0);
    auto* bufR = delayBuffer.getWritePointer (1);
    auto* left = buffer.getWritePointer (0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : left;

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const auto delaySamples = delaySamplesSmoothed.getNextValue();

        auto readPos = (double) delayWritePos - (double) delaySamples;
        while (readPos < 0.0)
            readPos += (double) bufLen;

        const auto r0 = (int) readPos;
        const auto r1 = (r0 + 1) % bufLen;
        const auto frac = (float) (readPos - (double) r0);

        const auto outL = bufL[r0] + frac * (bufL[r1] - bufL[r0]);
        const auto outR = bufR[r0] + frac * (bufR[r1] - bufR[r0]);

        // Ping-pong crosses the feedback paths.
        bufL[delayWritePos] = left[i] + (p.delayPingPong ? outR : outL) * p.delayFeedback;
        bufR[delayWritePos] = right[i] + (p.delayPingPong ? outL : outR) * p.delayFeedback;

        left[i] += outL * p.delayMix;
        right[i] += outR * p.delayMix;

        delayWritePos = (delayWritePos + 1) % bufLen;
    }
}

void FXChain::processReverb (juce::AudioBuffer<float>& buffer, const Params& p)
{
    juce::Reverb::Parameters rp;
    rp.roomSize = p.reverbSize;
    rp.damping = p.reverbDamping;
    rp.width = p.reverbWidth;
    // MIX is a true dry/wet dial: unity dry at 0, full wet (pure reverb) at 1,
    // equal-power (sin/cos) so the perceived level stays roughly constant since
    // the reverb tail and the dry signal are decorrelated. juce::Reverb scales
    // dryLevel by 2x internally, so 0.5*cos yields exactly unity dry at mix 0
    // (the old mapping left the dry pinned near +6 dB and never fully wet).
    const auto theta = p.reverbMix * juce::MathConstants<float>::halfPi;
    rp.wetLevel = std::sin (theta);
    rp.dryLevel = 0.5f * std::cos (theta);
    reverb.setParameters (rp);

    if (buffer.getNumChannels() > 1)
        reverb.processStereo (buffer.getWritePointer (0), buffer.getWritePointer (1),
                              buffer.getNumSamples());
    else
        reverb.processMono (buffer.getWritePointer (0), buffer.getNumSamples());
}

void FXChain::processEQ (juce::AudioBuffer<float>& buffer, const Params& p)
{
    if (! juce::approximatelyEqual (p.eqLowGainDb, lastLowGain)
        || ! juce::approximatelyEqual (p.eqMidFreq, lastMidFreq)
        || ! juce::approximatelyEqual (p.eqMidGainDb, lastMidGain)
        || ! juce::approximatelyEqual (p.eqHighGainDb, lastHighGain))
    {
        lastLowGain = p.eqLowGainDb;
        lastMidFreq = p.eqMidFreq;
        lastMidGain = p.eqMidGainDb;
        lastHighGain = p.eqHighGainDb;

        const auto low = juce::dsp::IIR::Coefficients<float>::makeLowShelf (
            sampleRate, 120.0f, 0.707f, juce::Decibels::decibelsToGain (p.eqLowGainDb));
        const auto mid = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
            sampleRate, p.eqMidFreq, 0.7f, juce::Decibels::decibelsToGain (p.eqMidGainDb));
        const auto high = juce::dsp::IIR::Coefficients<float>::makeHighShelf (
            sampleRate, 6000.0f, 0.707f, juce::Decibels::decibelsToGain (p.eqHighGainDb));

        for (auto& f : eqLow)  *f.coefficients = *low;
        for (auto& f : eqMid)  *f.coefficients = *mid;
        for (auto& f : eqHigh) *f.coefficients = *high;
    }

    for (int ch = 0; ch < juce::jmin (2, buffer.getNumChannels()); ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            auto v = eqLow[(size_t) ch].processSample (data[i]);
            v = eqMid[(size_t) ch].processSample (v);
            data[i] = eqHigh[(size_t) ch].processSample (v);
        }
    }
}

} // namespace spa::dsp
