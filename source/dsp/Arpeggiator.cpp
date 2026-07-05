#include "Arpeggiator.h"

namespace arsenal::dsp
{

void Arpeggiator::prepare (double sampleRate)
{
    currentSampleRate = sampleRate;
    scratch.ensureSize (8192);
    reset();
}

void Arpeggiator::reset()
{
    numHeld = 0;
    numActive = 0;
    arrivalCounter = 0;
    stepCounter = 0;
    walkIndex = 0;
    beatClock = 0.0;
    lastHostPpq = -1.0e9;
    latchedChordDown = false;
}

void Arpeggiator::addHeld (juce::uint8 note, juce::uint8 velocity)
{
    for (int i = 0; i < numHeld; ++i)
        if (held[i].note == note)
        {
            held[i].velocity = velocity;
            return;
        }

    if (numHeld < maxHeld)
        held[numHeld++] = { note, velocity, arrivalCounter++ };
}

void Arpeggiator::removeHeld (juce::uint8 note)
{
    for (int i = 0; i < numHeld; ++i)
    {
        if (held[i].note == note)
        {
            for (int j = i; j < numHeld - 1; ++j)
                held[j] = held[j + 1];
            --numHeld;
            return;
        }
    }
}

void Arpeggiator::releaseAllActive (juce::MidiBuffer& out, int samplePos)
{
    for (int i = 0; i < numActive; ++i)
        out.addEvent (juce::MidiMessage::noteOff (1, active[i].note), samplePos);
    numActive = 0;
}

int Arpeggiator::pickNoteIndex (const Params& p, int sequenceLength)
{
    switch (p.mode)
    {
        case params::ArpMode::random:
            return random.nextInt (sequenceLength);

        case params::ArpMode::randomWalk:
        {
            walkIndex += random.nextBool() ? 1 : -1;
            walkIndex = (walkIndex % sequenceLength + sequenceLength) % sequenceLength;
            return walkIndex;
        }

        case params::ArpMode::up: case params::ArpMode::down:
        case params::ArpMode::upDown: case params::ArpMode::downUp:
        case params::ArpMode::upDownInclusive: case params::ArpMode::converge:
        case params::ArpMode::diverge: case params::ArpMode::asPlayed:
        case params::ArpMode::chord: case params::ArpMode::phrase:
        default:
            return stepCounter % sequenceLength;
    }
}

void Arpeggiator::triggerStep (juce::MidiBuffer& out, int samplePos, const Params& p,
                               double stepBeat, double beatsPerStep)
{
    if (numHeld == 0)
        return;

    // Sorted-by-pitch view of the held notes (insertion sort into a small
    // local array — numHeld <= 32).
    int sorted[maxHeld];
    for (int i = 0; i < numHeld; ++i)
        sorted[i] = i;
    for (int i = 1; i < numHeld; ++i)
        for (int j = i; j > 0 && held[sorted[j]].note < held[sorted[j - 1]].note; --j)
            std::swap (sorted[j], sorted[j - 1]);

    const auto offBeat = stepBeat + (double) p.gate * beatsPerStep * 0.98;

    const auto velocityFor = [&] (juce::uint8 played, int step, int patternLen) -> juce::uint8
    {
        if (p.velocityMode == 1)
            return 100;
        if (p.velocityMode == 2)
            return step % juce::jmax (1, patternLen) == 0 ? 127 : 88;
        return played;
    };

    const auto emit = [&] (int midiNote, juce::uint8 velocity)
    {
        const auto note = (juce::uint8) juce::jlimit (0, 127, midiNote);
        if (numActive < maxActive)
        {
            out.addEvent (juce::MidiMessage::noteOn (1, note, velocity), samplePos);
            active[numActive++] = { note, offBeat };
        }
    };

    if (p.mode == params::ArpMode::chord)
    {
        for (int i = 0; i < numHeld; ++i)
            emit (held[i].note, velocityFor (held[i].velocity, stepCounter, 2));
        ++stepCounter;
        return;
    }

    if (p.mode == params::ArpMode::phrase)
    {
        const auto& phrase = params::arpPhrases()[(size_t) juce::jlimit (
            0, (int) params::arpPhrases().size() - 1, p.phrase)];
        const auto step = stepCounter % (phrase.length * juce::jmax (1, p.octaves));
        const auto interval = phrase.intervals[step % phrase.length];
        const auto octave = 12 * (step / phrase.length);
        const auto base = held[sorted[0]];
        emit (base.note + interval + octave,
              velocityFor (base.velocity, step, phrase.length));
        ++stepCounter;
        return;
    }

    // Build the pattern order over held notes x octaves.
    const auto n = numHeld;
    const auto octaves = juce::jmax (1, p.octaves);
    const auto spanLength = n * octaves;

    const auto noteAt = [&] (int position) -> const Held&
    {
        return held[sorted[position % n]];
    };
    const auto pitchAt = [&] (int position)
    {
        return (int) noteAt (position).note + 12 * (position / n);
    };

    int sequencePosition = 0;
    int sequenceLength = spanLength;

    switch (p.mode)
    {
        case params::ArpMode::up:
            sequencePosition = pickNoteIndex (p, spanLength);
            break;

        case params::ArpMode::down:
            sequencePosition = spanLength - 1 - (stepCounter % spanLength);
            break;

        case params::ArpMode::upDown:
        {
            sequenceLength = juce::jmax (1, 2 * spanLength - 2);
            const auto step = pickNoteIndex (p, sequenceLength);
            sequencePosition = step < spanLength ? step : 2 * spanLength - 2 - step;
            break;
        }

        case params::ArpMode::downUp:
        {
            sequenceLength = juce::jmax (1, 2 * spanLength - 2);
            const auto step = pickNoteIndex (p, sequenceLength);
            const auto up = step < spanLength ? step : 2 * spanLength - 2 - step;
            sequencePosition = spanLength - 1 - up;
            break;
        }

        case params::ArpMode::upDownInclusive:
        {
            sequenceLength = 2 * spanLength;
            const auto step = pickNoteIndex (p, sequenceLength);
            sequencePosition = step < spanLength ? step : 2 * spanLength - 1 - step;
            break;
        }

        case params::ArpMode::converge:
        {
            const auto step = pickNoteIndex (p, spanLength);
            sequencePosition = (step % 2 == 0) ? step / 2
                                               : spanLength - 1 - step / 2;
            break;
        }

        case params::ArpMode::diverge:
        {
            const auto step = pickNoteIndex (p, spanLength);
            const auto centre = spanLength / 2;
            sequencePosition = (step % 2 == 0) ? centre + step / 2
                                               : centre - 1 - step / 2;
            sequencePosition = ((sequencePosition % spanLength) + spanLength) % spanLength;
            break;
        }

        case params::ArpMode::asPlayed:
        {
            // Arrival order instead of pitch order.
            int byArrival[maxHeld];
            for (int i = 0; i < n; ++i)
                byArrival[i] = i;
            for (int i = 1; i < n; ++i)
                for (int j = i; j > 0 && held[byArrival[j]].arrivalOrder
                                         < held[byArrival[j - 1]].arrivalOrder; --j)
                    std::swap (byArrival[j], byArrival[j - 1]);

            const auto step = pickNoteIndex (p, spanLength);
            const auto& h = held[byArrival[step % n]];
            emit ((int) h.note + 12 * (step / n),
                  velocityFor (h.velocity, step, spanLength));
            ++stepCounter;
            return;
        }

        case params::ArpMode::random:
        case params::ArpMode::randomWalk:
            sequencePosition = pickNoteIndex (p, spanLength);
            break;

        case params::ArpMode::chord:
        case params::ArpMode::phrase:
        default:
            sequencePosition = stepCounter % spanLength;
            break;
    }

    const auto& source = noteAt (sequencePosition);
    emit (pitchAt (sequencePosition),
          velocityFor (source.velocity, stepCounter, sequenceLength));
    ++stepCounter;
}

void Arpeggiator::process (juce::MidiBuffer& midi, int numSamples, const Params& p)
{
    if (! p.enable)
    {
        if (wasEnabled)
        {
            // Arp turned off: release anything still sounding, keep the
            // original buffer flowing.
            scratch.clear();
            releaseAllActive (scratch, 0);
            scratch.addEvents (midi, 0, numSamples, 0);
            midi.swapWith (scratch);
            reset();
        }
        wasEnabled = false;
        return;
    }
    wasEnabled = true;

    scratch.clear();
    auto& out = scratch;

    // --- Absorb incoming note events, pass everything else through ----------
    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();

        if (message.isNoteOn())
        {
            if (p.latch && ! latchedChordDown)
                numHeld = 0;   // new chord replaces the latched one
            latchedChordDown = true;
            addHeld ((juce::uint8) message.getNoteNumber(),
                     (juce::uint8) juce::jmax (1, (int) (message.getVelocity())));
        }
        else if (message.isNoteOff())
        {
            if (! p.latch)
                removeHeld ((juce::uint8) message.getNoteNumber());
            // With latch: keep the notes; just track physical key state.
        }
        else
        {
            out.addEvent (message, metadata.samplePosition);
        }
    }

    // Latch bookkeeping: when no physical keys remain down, the next chord
    // starts fresh. (Approximation: any noteOff this block may have lifted
    // the last key; precise per-key tracking below.)
    if (p.latch)
    {
        bool anyOffThisBlock = false;
        for (const auto metadata : midi)
            if (metadata.getMessage().isNoteOff())
                anyOffThisBlock = true;
        if (anyOffThisBlock)
            latchedChordDown = false;
    }

    // --- Beat clock ----------------------------------------------------------
    const auto beatsPerStep = (double) params::lfoDivisionBeats (p.division);
    const auto samplesPerBeat = currentSampleRate * 60.0 / juce::jmax (1.0, p.bpm);
    const auto blockBeats = (double) numSamples / samplesPerBeat;

    double blockStartBeat;
    if (p.hostPlaying)
    {
        // Follow the host; resync on jumps (loop points, relocations).
        if (std::abs (p.ppqAtBlockStart - lastHostPpq) > blockBeats * 4.0 + 0.25)
            stepCounter = (int) std::floor (p.ppqAtBlockStart / beatsPerStep + 1.0e-6);
        blockStartBeat = p.ppqAtBlockStart;
        lastHostPpq = p.ppqAtBlockStart + blockBeats;
        beatClock = blockStartBeat + blockBeats;
    }
    else
    {
        blockStartBeat = beatClock;
        beatClock += blockBeats;
    }
    const auto blockEndBeat = blockStartBeat + blockBeats;

    // --- Note-offs due inside this block --------------------------------------
    for (int i = numActive - 1; i >= 0; --i)
    {
        if (active[i].offBeat < blockEndBeat)
        {
            const auto beatOffset = juce::jmax (0.0, active[i].offBeat - blockStartBeat);
            const auto pos = juce::jlimit (0, numSamples - 1,
                                           (int) (beatOffset * samplesPerBeat));
            out.addEvent (juce::MidiMessage::noteOff (1, active[i].note), pos);
            active[i] = active[--numActive];
        }
    }

    if (numHeld == 0 && numActive == 0)
    {
        midi.swapWith (out);
        return;
    }

    // --- Step triggers inside this block (with swing on odd steps) ------------
    const auto firstStep = (int) std::ceil (blockStartBeat / beatsPerStep - 1.0e-9);
    for (int k = firstStep; ; ++k)
    {
        auto trigBeat = (double) k * beatsPerStep;
        if (k % 2 != 0)
            trigBeat += (double) p.swing * beatsPerStep;

        if (trigBeat >= blockEndBeat)
            break;
        if (trigBeat < blockStartBeat)
            continue;

        const auto pos = juce::jlimit (0, numSamples - 1,
                                       (int) ((trigBeat - blockStartBeat) * samplesPerBeat));
        triggerStep (out, pos, p, trigBeat, beatsPerStep);
    }

    midi.swapWith (out);
}

} // namespace arsenal::dsp
