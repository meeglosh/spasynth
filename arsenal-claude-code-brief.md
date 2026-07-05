# Arsenal — Build Brief for Claude Code

You are building **Arsenal**, a commercial software synthesizer for macOS and Windows, sold by Silverplatter Audio. Build it in C++ using the **JUCE** framework. This document is the full spec. Read it end to end before writing code, then propose a build order and confirm the JUCE version and toolchain with me before scaffolding.

## What Arsenal is

A hybrid soft synth that fuses two ideas into one instrument:

1. A **wavetable synth** in the spirit of Serum, but with an added **organic chaos** layer that introduces controlled, analog-style randomness and instability so patches feel alive rather than sterile.
2. A **sample / SFX engine** in the spirit of Krotos Studio, where audio files can act as oscillators AND as modulation sources. The sound library is the Silverplatter Audio SFX collection.

The instrument targets music production, sound design, and experimental soundscapes. Its signature feature is a top-bar **RANDOMIZE ALL** button that generates unique, unpredictable, still-musical patches, with per-section locks so the user can freeze what they like and re-roll the rest.

## Formats and platforms (v1)

- Standalone (Mac + Windows)
- VST3 (Mac + Windows)
- AU (Mac)
- **AAX is explicitly OUT for v1.** Structure the project so an AAX target can be added later without rearchitecting, but do not attempt to build it now. Do not add PACE/iLok code.
- Cross-platform: universal binary on macOS (Apple Silicon + Intel), x64 on Windows.

## Core architecture

Design the DSP so the two engines share one voice structure rather than living as two separate synths. Per voice:

**Oscillator slots (build 3, architect for 4):** each slot has a switchable engine:
- **Wavetable mode:** wavetable playback with position modulation, unison (voice count, detune, blend, stereo width), phase control, and per-slot pitch/level/pan.
- **Sample/SFX mode:** loads a WAV. Two playback sub-modes: classic sample playback (with loop points, start offset, keytracking on/off) and granular (grain size, density, position, spray, pitch). An SFX slot can be tonal or textural.

**Organic Chaos section (the Serum-plus differentiator):** a per-voice modulation source that injects constrained randomness. Parameters: Depth, Rate, and independent enable/amount for drift on pitch, phase, wavetable position, and amplitude. Model it as smoothed random-walk / filtered noise rather than white noise, so it reads as analog instability and organic movement, not glitching. It must be assignable in the mod matrix too.

**Filters:** two multimode filters (LP/HP/BP/notch, 12/24 dB), routable series or parallel, with drive.

**Envelopes:** at least 3 ADSR envelopes (one hardwired to amp), assignable.

**LFOs:** at least 3, tempo-syncable, with custom shapes if feasible.

**Modulation matrix:** fully assignable. Sources must include: envelopes, LFOs, macros (at least 4), velocity, mod wheel, aftertouch, the Chaos source, AND **SFX followers** — the amplitude envelope and pitch of any loaded sample/SFX slot usable as a mod source routed to any destination. This SFX-as-modulator routing is the Krotos-side signature; make it first-class.

**FX chain:** distortion/waveshaper, chorus, delay (tempo-sync), reverb, EQ. Orderable if practical.

**Voice management:** polyphonic with configurable voice count, mono/legato/glide modes.

## RANDOMIZE ALL

Top-bar button. Randomizes every parameter, but through **weighted, constrained** ranges so results are musical, not noise. Implementation requirements:
- Per-section **lock** toggles (oscillators, filters, chaos, mod matrix, FX, etc.). Locked sections are preserved; everything else re-rolls.
- Randomization respects sane bounds per parameter (define a min/max/curve per parameter in the parameter definition itself).
- A global "chaos amount" that biases how wild the randomization is.
- Re-rolling should feel generative and inspiring. Test it by ear conceptually: pure uniform random across all params sounds bad; bias toward musically useful zones.

## Preset system and the Silverplatter library

- Preset browser with categories.
- **The SFX library is already organized as WAV files, one folder per pack.** Map **each pack folder to one preset category.** On load, scan the library root and build categories from folder names automatically.
- Ship factory presets per category that showcase the SFX-as-oscillator and SFX-as-modulator capabilities.
- Preset format: human-readable (JSON or XML via JUCE ValueTree) plus binary state for host save/restore.
- Assume the library path is configurable and provide a first-run setup to point Arsenal at it. Do not hardcode my machine's paths.

## UI

- Single-window resizable UI. Clear top bar with: preset name/browser, RANDOMIZE ALL, master volume, chaos amount.
- Silverplatter Audio branding placeholder (I'll supply final assets). Leave a clean theming layer so I can restyle. My aesthetic leans mid-century modern, minimalist, warm palette, but treat colors/typography as tokens I can swap, don't bake them in.
- Prioritize a working, legible UI over visual polish in early stages. Polish is a late phase.

## Build order (build top-down but keep it running at every checkpoint)

Even though the goal is aggressive, keep Arsenal **sound-producing and compiling at every checkpoint**. Suggested order, adjust and propose your own:

1. JUCE project scaffold. All three v1 formats plus standalone building on both platforms. MIDI in, one wavetable osc, one filter, amp envelope. Confirm it makes sound in a host.
2. Full wavetable engine: wavetable loading, position mod, unison, phase, multiple osc slots.
3. Organic Chaos section, wired to pitch/phase/position/amp and exposed to the mod matrix.
4. Sample/SFX engine: classic playback + granular. Slot engine switching.
5. Mod matrix, LFOs, extra envelopes, macros, and the SFX-follower sources.
6. FX chain.
7. RANDOMIZE ALL with per-section locks and constrained ranges.
8. Preset system + Silverplatter library folder-to-category scanning + factory presets.
9. UI theming pass and layout polish.
10. Packaging: installers for Mac (.pkg) and Windows, code-signing hooks left as configurable steps (I'll handle certs/notarization).

## Constraints and notes

- **JUCE licensing:** this is a commercial product. Flag anything that requires a specific JUCE license tier. Do not assume GPL is acceptable.
- **No third-party code with incompatible licenses** without flagging it to me first.
- Keep DSP real-time-safe: no allocation, locking, or file I/O on the audio thread.
- Parameter definitions should be centralized (one source of truth) so the mod matrix, randomizer, UI, and host automation all read from the same list.
- Write it to be maintainable: I will iterate on this across many sessions.
- If something is genuinely ambiguous or a decision has real tradeoffs, ask me rather than guessing.

## First response I want from you

Do NOT start scaffolding yet. First:
1. Confirm JUCE version, C++ standard, and build system (CMake preferred) you'll use.
2. Flag the JUCE license tier this needs for commercial sale.
3. Give me your proposed build order (yours may differ from mine) and what the first running checkpoint will contain.
4. List anything in this spec that's technically risky or that you'd descope for a v1.

Then wait for my go before writing code.
