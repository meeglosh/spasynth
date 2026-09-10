#include "SampleData.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <cmath>

namespace spa::dsp
{

namespace
{
    // Analysis runs at a decimated rate: plenty for follower curves, and it
    // keeps YIN affordable on long files.
    constexpr double analysisRate = 16000.0;
    constexpr int hopSamples = 160;         // 10 ms at 16 kHz
    constexpr int yinWindow = 1024;         // 64 ms
    constexpr int yinMaxLag = 400;          // 40 Hz floor
    constexpr int yinMinLag = 16;           // 1 kHz ceiling
    constexpr float yinThreshold = 0.15f;
    constexpr double maxAnalysisSeconds = 120.0;

    std::vector<float> decimateToMono (const juce::AudioBuffer<float>& audio,
                                       double sourceRate)
    {
        const auto ratio = sourceRate / analysisRate;
        const auto outLen = (int) juce::jmin ((double) audio.getNumSamples() / ratio,
                                              maxAnalysisSeconds * analysisRate);

        std::vector<float> mono ((size_t) outLen, 0.0f);
        const auto channelGain = 1.0f / (float) audio.getNumChannels();

        for (int i = 0; i < outLen; ++i)
        {
            const auto srcIdx = juce::jmin ((int) (i * ratio), audio.getNumSamples() - 1);
            for (int ch = 0; ch < audio.getNumChannels(); ++ch)
                mono[(size_t) i] += audio.getSample (ch, srcIdx) * channelGain;
        }

        return mono;
    }

    std::vector<float> analyzeAmplitude (const std::vector<float>& mono)
    {
        std::vector<float> curve;
        curve.reserve (mono.size() / hopSamples + 1);

        float peak = 1.0e-9f;
        for (size_t start = 0; start + 1 < mono.size(); start += hopSamples)
        {
            const auto end = juce::jmin (start + (size_t) hopSamples * 2, mono.size());
            double sum = 0.0;
            for (auto i = start; i < end; ++i)
                sum += (double) mono[i] * mono[i];

            const auto rms = (float) std::sqrt (sum / (double) (end - start));
            curve.push_back (rms);
            peak = juce::jmax (peak, rms);
        }

        for (auto& v : curve)
            v /= peak;

        return curve;
    }

    // YIN (de Cheveigné & Kawahara) with the cumulative-mean normalization.
    // Returns MIDI note mapped 24..96 -> 0..1, or 0 where unvoiced.
    std::vector<float> analyzePitch (const std::vector<float>& mono)
    {
        std::vector<float> curve;
        curve.reserve (mono.size() / hopSamples + 1);

        std::vector<float> d ((size_t) yinMaxLag + 1);

        for (size_t start = 0; start + 1 < mono.size(); start += hopSamples)
        {
            if (start + yinWindow + yinMaxLag >= mono.size())
            {
                curve.push_back (curve.empty() ? 0.0f : curve.back());
                continue;
            }

            // Difference function.
            for (int lag = 1; lag <= yinMaxLag; ++lag)
            {
                double sum = 0.0;
                for (int i = 0; i < yinWindow; ++i)
                {
                    const auto diff = mono[start + (size_t) i]
                                    - mono[start + (size_t) (i + lag)];
                    sum += (double) diff * diff;
                }
                d[(size_t) lag] = (float) sum;
            }

            // Cumulative mean normalized difference.
            float cumulative = 0.0f;
            int bestLag = 0;
            for (int lag = 1; lag <= yinMaxLag; ++lag)
            {
                cumulative += d[(size_t) lag];
                const auto cmnd = cumulative > 0.0f
                                ? d[(size_t) lag] * (float) lag / cumulative
                                : 1.0f;
                if (lag >= yinMinLag && cmnd < yinThreshold)
                {
                    bestLag = lag;
                    break;
                }
            }

            if (bestLag == 0)
            {
                curve.push_back (0.0f);  // unvoiced
                continue;
            }

            const auto freq = (float) (analysisRate / bestLag);
            const auto midi = 69.0f + 12.0f * std::log2 (freq / 440.0f);
            curve.push_back (juce::jlimit (0.0f, 1.0f, (midi - 24.0f) / 72.0f));
        }

        return curve;
    }

    // Onset detection from the (already-computed) amplitude envelope: a
    // half-wave-rectified flux (frame-to-frame rise only) with an adaptive
    // local-mean threshold, then a local-max peak pick with a 100 ms
    // refractory gap (avoids double-triggering on one transient's ringing).
    std::vector<double> detectOnsetsSeconds (const std::vector<float>& ampCurve, double hopSeconds)
    {
        std::vector<float> flux (ampCurve.size(), 0.0f);
        for (size_t i = 1; i < ampCurve.size(); ++i)
            flux[i] = juce::jmax (0.0f, ampCurve[i] - ampCurve[i - 1]);

        const int windowHops = juce::jmax (1, (int) (0.2 / hopSeconds));
        const int refractoryHops = juce::jmax (1, (int) (0.1 / hopSeconds));

        std::vector<double> onsets;
        int lastOnset = -refractoryHops - 1;

        for (size_t i = 0; i < flux.size(); ++i)
        {
            const auto lo = (size_t) juce::jmax (0, (int) i - windowHops);
            const auto hi = juce::jmin (flux.size(), i + (size_t) windowHops + 1);
            double sum = 0.0;
            for (auto j = lo; j < hi; ++j) sum += flux[j];
            const auto localMean = (float) (sum / (double) (hi - lo));
            const auto threshold = localMean * 1.6f + 0.02f;

            const bool isPeak = flux[i] > threshold
                             && (i == 0 || flux[i] >= flux[i - 1])
                             && (i + 1 >= flux.size() || flux[i] >= flux[i + 1]);

            if (isPeak && (int) i - lastOnset > refractoryHops)
            {
                onsets.push_back ((double) i * hopSeconds);
                lastOnset = (int) i;
            }
        }
        return onsets;
    }

    // Folds a raw BPM estimate into the 60..180 "musical" range by doubling
    // or halving, matching how DAWs report detected tempo.
    double foldBpm (double bpm)
    {
        while (bpm < 60.0 && bpm > 0.0) bpm *= 2.0;
        while (bpm > 180.0) bpm *= 0.5;
        return bpm;
    }

    // Tempo from inter-onset intervals: bucket every IOI (and simple
    // multiples, so half/double-time onsets still vote for the same beat)
    // into a histogram over 250 ms..2 s, take the strongest bucket as the
    // beat period, and report a confidence from how sharply it dominates and
    // how many onsets it actually explains.
    struct TempoEstimate { double bpm; float confidence; };

    TempoEstimate estimateTempoFromOnsets (const std::vector<double>& onsets, double fileLengthSeconds)
    {
        if (onsets.size() < 3)
            return { 120.0, 0.0f };

        constexpr double minPeriod = 0.25, maxPeriod = 2.0;
        constexpr int numBins = 200;
        constexpr double binWidth = (maxPeriod - minPeriod) / (double) numBins;

        // Consecutive-onset IOIs only: each vote is (close to) the true beat
        // period itself rather than some multiple of it, so the histogram
        // peaks cleanly at the real tempo. (An earlier version voted every
        // pairwise gap, folding multiples back into range -- that let odd
        // multiples like 1.5x the true period collect more folded votes
        // than the fundamental itself and biased the estimate.)
        std::vector<double> hist (numBins, 0.0);
        for (size_t i = 1; i < onsets.size(); ++i)
        {
            auto ioi = onsets[i] - onsets[i - 1];
            while (ioi > maxPeriod) ioi *= 0.5;
            while (ioi < minPeriod && ioi > 0.0) ioi *= 2.0;
            if (ioi < minPeriod || ioi > maxPeriod) continue;
            const auto bin = juce::jlimit (0, numBins - 1, (int) ((ioi - minPeriod) / binWidth));
            hist[(size_t) bin] += 1.0;
        }

        int bestBin = 0;
        double bestVal = 0.0, total = 0.0;
        for (int b = 0; b < numBins; ++b)
        {
            total += hist[(size_t) b];
            if (hist[(size_t) b] > bestVal) { bestVal = hist[(size_t) b]; bestBin = b; }
        }
        if (bestVal <= 0.0)
            return { 120.0, 0.0f };

        const auto period = minPeriod + (bestBin + 0.5) * binWidth;
        const auto bpm = foldBpm (60.0 / period);

        const auto sharpness = (float) (bestVal / juce::jmax (1.0, total));
        const auto expectedOnsets = juce::jmax (1.0, fileLengthSeconds / period);
        const auto coverage = juce::jlimit (0.0f, 1.0f, (float) ((double) onsets.size() / expectedOnsets));
        const auto confidence = juce::jlimit (0.0f, 1.0f, sharpness * 2.2f * (0.5f + 0.5f * coverage));

        return { bpm, confidence };
    }

    // Fallback for one-shots/low-confidence material: assume the file spans
    // the nearest whole number of beats (1/2/4/8/16) at some tempo in
    // 60..180, choosing whichever (beats, bpm) pair implies the "most
    // musical" tempo (needs the least octave-folding to land in range).
    double lengthBasedBeats (double lengthSeconds, double& outBpm)
    {
        static constexpr int candidates[] = { 1, 2, 4, 8, 16 };
        double bestErr = 1.0e9, bestBeats = 1.0, bestBpm = 120.0;
        for (auto beats : candidates)
        {
            const auto bpm = 60.0 * beats / juce::jmax (1.0e-6, lengthSeconds);
            const auto folded = foldBpm (bpm);
            const auto err = std::abs (bpm - folded) + std::abs (folded - juce::jlimit (60.0, 180.0, folded));
            if (err < bestErr) { bestErr = err; bestBeats = (double) beats; bestBpm = folded; }
        }
        outBpm = bestBpm;
        return bestBeats;
    }
}

LoadedSample loadSampleFromFile (const juce::File& file)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    // A just-reconnected external drive can briefly fail reads while macOS
    // finishes remounting the volume, which createReaderFor reports as an
    // unrecognized format even though the file is fine moments later. Retry
    // a few times before giving up (runs on a background load thread, never
    // the audio thread, so blocking here is fine).
    std::unique_ptr<juce::AudioFormatReader> reader;
    for (int attempt = 0; attempt < 4 && reader == nullptr; ++attempt)
    {
        if (attempt > 0)
            juce::Thread::sleep (60 * attempt);
        reader.reset (formats.createReaderFor (file));
    }
    if (reader == nullptr)
        return { nullptr, "Unrecognized audio format: " + file.getFileName() };

    if (reader->lengthInSamples < 64)
        return { nullptr, "File too short: " + file.getFileName() };

    auto data = std::make_shared<SampleData>();
    data->sourceSampleRate = reader->sampleRate;
    data->name = file.getFileNameWithoutExtension();

    const auto numChannels = (int) juce::jmin (reader->numChannels, 2u);

    // A corrupt WAV header can declare a sample count that overflows the
    // int64->int cast below (UB, possibly negative); cap at a sane 10 minutes
    // of source-rate audio, same clamp-before-cast pattern as
    // FXChain::loadConvolutionIR. Floor-guarded so a zero/garbage sampleRate
    // can't produce a cap below the 64-sample minimum already checked above.
    const juce::int64 maxSamples = juce::jmax ((juce::int64) 64,
                                               (juce::int64) (reader->sampleRate * 600.0));
    const int n = (int) juce::jmin (maxSamples, reader->lengthInSamples);

    data->audio.setSize (numChannels, n);
    if (! reader->read (&data->audio, 0, n, 0, true, numChannels > 1))
        return { nullptr, "Failed to read audio data: " + file.getFileName() };

    const auto mono = decimateToMono (data->audio, data->sourceSampleRate);
    data->ampCurve = analyzeAmplitude (mono);
    data->pitchCurve = analyzePitch (mono);
    data->hopSeconds = (double) hopSamples / analysisRate;

    // Tempo sync analysis. Files shorter than ~0.3s, or with fewer than 3
    // detected onsets, are treated as one-shots (confidence 0) and fall back
    // to the length-based whole-beat-count guess.
    const auto onsets = detectOnsetsSeconds (data->ampCurve, data->hopSeconds);
    data->onsetCurve.assign (data->ampCurve.size(), 0.0f);
    for (auto t : onsets)
    {
        const auto hop = (int) (t / juce::jmax (1.0e-9, data->hopSeconds));
        if (hop >= 0 && hop < (int) data->onsetCurve.size())
            data->onsetCurve[(size_t) hop] = 1.0f;
    }
    const auto lengthSeconds = data->lengthSeconds();
    if (lengthSeconds < 0.3 || onsets.size() < 3)
    {
        double fallbackBpm = 120.0;
        data->detectedBeats = (float) lengthBasedBeats (lengthSeconds, fallbackBpm);
        data->detectedBpm = fallbackBpm;
        data->bpmConfidence = 0.0f;
    }
    else
    {
        const auto est = estimateTempoFromOnsets (onsets, lengthSeconds);
        data->detectedBpm = est.bpm;
        data->bpmConfidence = est.confidence;
        data->detectedBeats = (float) (lengthSeconds * est.bpm / 60.0);
    }

    return { std::move (data), {} };
}

} // namespace spa::dsp
