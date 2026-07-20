# Next release plan (post-1.0.2)

Running spec for the next feature release. Mike is gathering all feature
requests first; do NOT build until the list is complete and he gives the go.
(Version number TBD — these are features, so 1.1.0 may fit better than 1.0.3,
but that is Mike's call.)

## 1. FX: new "Mod" tab (Phaser / Flanger) — AGREED

- A new **Mod** module/tab in the FX section, with a **type selector: Phaser or
  Flanger** (switchable, one at a time; one slot in the chain). Not both at once.
- Maximum granular controls per type:
  - **Phaser:** Rate + Tempo Sync (note divisions), Depth, Feedback/Resonance,
    Stages/Poles (2/4/6/8/12 allpass = notch count), Center Frequency, Stereo
    Spread, Mix, Enable.
  - **Flanger:** Rate + Tempo Sync, Depth (sweep), Manual delay/offset (base comb
    time), Feedback (bipolar +/-), Stereo Width, Mix, Enable.
- Continuous controls become **mod-matrix destinations** (append after existing
  dests; dense dest order is serialized and append-only).
- DSP: `juce::dsp::Phaser` for the phaser; flanger is a modulated short delay line
  with bipolar feedback (custom, or `juce::dsp::Chorus` short-delay config).
- Keep the existing **Chorus** tab separate (decided against folding it in).
- Params go in `ParameterRegistry` (Mod type is an append-only choice param;
  sync divisions reuse the existing division choices).

## 2. FX: drag-to-reorder tabs -> chain order — AGREED

- Reorder the FX tabs by **drag and drop** (browser-tab feel), and the order
  drives the actual FX chain processing order.
- **All six** modules reorderable: Dist, Chorus, Mod, Delay, Reverb, EQ.
- Order is **saved per preset** (part of the sound). Existing presets without an
  order default to today's fixed order (`distortion, chorus, delay, reverb, eq`,
  plus Mod slotted in) — backward-compatible.
- UI: JUCE `TabbedComponent` has no native tab drag-reorder, so build a custom
  draggable tab bar.
- **Drag affordance:** every FX tab shows a small **grip-dots handle** (the
  standard drag glyph) so users know the tabs are draggable — discoverability
  matters (same lesson as the keyboard toggle being missed). Subtle by default,
  brighter on hover.
- Real-time safety: `FXChain::processOrder` becomes runtime state (small array of
  stable module IDs). Publish the new order to the audio thread via the existing
  once-per-block SharedState / atomic-pointer pattern; no locks/alloc on the
  audio thread. Give each module a **stable serialized ID** so reordering saves
  correctly and future modules can be added without breaking old presets.

## 3. FX: Limiter / Maximizer — AGREED

- A **maximizer**: brickwall limiter plus an input **drive** that pushes loudness
  up to a ceiling.
- Controls: Enable, **Drive/Input Gain** (dB), **Ceiling** (dBFS), **Release**
  (ms) + **Auto-release** toggle, **Character/Style** modes (Clean / Punchy /
  Aggressive — release curve + saturation), **Stereo Link** (%), **True-peak**
  toggle (inter-sample), and a **gain-reduction meter** display.
- Placement: a normal **reorderable** FX tab (part of the drag-reorder chain),
  but it **defaults to the last position** where a limiter belongs.
- **Lookahead: optional toggle, OFF by default** (zero added latency for live
  play). Off = fast zero-latency clipper/soft-limit style. On = true lookahead
  peak detection; report the lookahead latency to the host via
  `setLatencySamples` (update it when the toggle changes; hosts re-sync).
- DSP: custom lookahead limiter (JUCE `dsp::Limiter` is too basic — no
  lookahead, no drive). Continuous controls (drive, ceiling, release) as
  mod-matrix destinations where sensible.
- This makes 7 modules in the reorderable chain: Dist, Chorus, Mod, Delay,
  Reverb, EQ, Limiter.

## 4. Global oversampling / quality — AGREED

- Global control: **Off / 2x / 4x / 8x**.
- Scope: **whole synth** — the entire audio-generation path (oscillators,
  filters, per-voice DSP) plus FX runs at Nx, then downsample the final output to
  the host rate. (A synth generates audio, so there is no input to upsample: run
  generation at Nx, downsample out.)
- **Near-zero-latency** by default (IIR/minimum-phase polyphase via
  `juce::dsp::Oversampling`) so live play stays tight; FIR would be more linear
  but adds latency.
- CPU scales ~xN; **off by default**.
- Implementation notes:
  - Prepare the voice audio DSP at `sampleRate * N`. Decide whether the per-voice
    64-sample mod chunks run at Nx or stay host-rate control timing (lean:
    control/mod stays host-rate, audio runs at Nx).
  - Changing the factor re-prepares the DSP at the new rate (message-thread
    reconfigure + RT-safe swap; Off = full bypass, zero cost).
  - First verify oscillator anti-aliasing (mip-mapped wavetables / PolyBLEP?) to
    confirm where OS earns its CPU (oscillators vs only the nonlinear FX).
  - **Open sub-decision:** is the factor a global/machine setting (lean this —
    it is a CPU/quality choice, lives well in the settings menu) or a per-preset
    param? Confirm with Mike before building.

## 5. Reverb: algorithmic modes (FDN) — AGREED

- Rework the Reverb module from the single `juce::Reverb` to an **FDN-based
  engine with modes**: Hall, Plate, Chamber, Room, Spring (dispersive-allpass
  model). Type is an append-only choice param.
- Granular controls: Pre-delay, Size/Decay (RT60), HF damping, LF damping,
  Diffusion, Tail modulation (rate + depth), Low-cut, High-cut, Early/Late
  balance, Width, Mix (reuse the 1.0.2 equal-power dry/wet crossfade), Enable.
- Self-contained DSP, no bundled IR assets. Continuous controls become
  mod-matrix destinations where sensible. Supersedes the current juce::Reverb.

## 6. FX: new "Convolve" module (SFX as impulse) — AGREED

- New reorderable FX tab. Convolves the signal with an impulse response chosen
  from the **library** (reuse the osc sample-picker UI) or **any user WAV**.
  Framed as a creative texture/space effect, NOT a realistic-reverb replacement
  (the algorithmic Reverb covers realistic spaces).
- DSP: `juce::dsp::Convolution` (zero-latency partitioned). Cap IR length
  (truncate/resample long SFX to ~a few seconds), auto-normalize, and condition
  (fade in/out, optional DC/low-cut) so arbitrary SFX stay usable.
- Controls: IR source (library picker / user WAV), IR Trim/Start, Reverse,
  Length cap, Pre-delay, Low-cut + High-cut on the wet, Width, Mix, Enable.
- Global FX (on the summed output), not per-voice. IR refs stored portably
  (`$LIB$/...`) per preset, like other sample refs. No bundled space IRs.

## 7. Voice modes — AGREED

- Per-preset **voice mode** (append-only choice param): **Poly** (default),
  **Mono**, **Duo**, **Paraphonic**, **Unison**.
- Mono family (Mono/Duo/Para/Unison) sub-controls: **Note Priority**
  (Last / Low / High) and **Legato vs Retrigger**. Mono keeps a held-note stack
  (release falls back to a still-held note). Ties into the existing Glide modes
  (Off/Always/Legato).
- **Unison mode:** whole-voice mono-unison — voice count, detune, stereo spread
  (distinct from the existing per-oscillator UNISON, which thickens one osc).
- **Paraphonic = the heavy one.** All held notes trigger oscillators but share
  ONE filter + ONE amp envelope. The current engine is fully per-voice (filter +
  envelopes baked into each voice), so true paraphonic is a real architecture
  refactor (multi-pitch oscillator bank into a shared filter/VCA). Biggest DSP
  lift in the release alongside the FDN reverb.
- Implemented in the note-assignment layer (`GlideSynthesiser` / voice
  allocation). Interacts with the arp (mono + arp = voice stolen per step, normal).

## 8. Standalone tempo: internal BPM + external MIDI clock — AGREED

- **Plugin is unchanged** — it already syncs to the host playhead (arp, delay
  sync, LFO sync all follow the DAW). No global sync toggle in the plugin.
- **Standalone only:** today it runs synced features at a fixed 120 BPM (no
  playhead). Add:
  - an **internal BPM control** (tempo field + tap tempo) feeding `shared.bpm`,
  - a sync-source toggle **Internal vs External (MIDI clock)**, and
  - **MIDI Beat Clock reception**: parse `0xF8` (24 PPQN) for tempo (smoothed)
    and Start/Stop/Continue (`0xFA/0xFC/0xFB`) for transport, so the standalone
    can slave to a DAW or hardware sequencer.
- Implementation: give the processor an internal tempo/transport source used
  when there is no host playhead; the standalone UI exposes BPM + sync source.
  MIDI clock bytes arrive in the processBlock MIDI buffer; parse + smooth there.
  Plugin path untouched.

## 9. Parametric EQ (FabFilter Pro-Q style) — AGREED

- Replace the fixed 3-band EQ with a **parametric EQ, up to 8 bands**.
- Per band: Enable, **Frequency**, **Gain**, **Q**, **Type** (Bell, Low Shelf,
  High Shelf, Low Cut / High Cut with selectable slope, Notch).
- **Fixed pool of 8 bands** (params pre-declared + appended to the registry;
  automation/preset-safe). "Add node" enables the next free band; "remove"
  disables it — feels dynamic without dynamic params.
- **Interactive node display** over the EQ curve: drag **x = freq, y = gain**;
  **scroll / Alt-drag = Q**; **double-click empty = add band**; **double-click a
  node = remove**; **right-click node = type/options**. Live curve redraw. This
  is the biggest custom-UI build in the release.
- **Real-time spectrum analyzer** (FFT) drawn behind the curve (Pro-Q look).
- **EQ character dropdown** (append-only choice): Clean / Modern (transparent),
  Vintage / Tube (analog-style harmonic saturation + proportional-Q coloration).
- DSP: chain of biquads (`juce::dsp::IIR` or the existing filter code), RT-safe
  coefficient updates; character adds a saturation/analog-modeling layer.
- **Backward compat:** migrate the old `eqLowGain/eqMidFreq/eqMidGain/eqHighGain`
  into the new bands on preset load; keep the old params readable for legacy
  presets. Selected band freq/gain/Q can be mod-matrix destinations (appended).

## 10. FX: Tremolo / Vibrato tab — AGREED

- New reorderable FX tab with **both a Tremolo and a Vibrato section, each
  independent** (own enable + controls) so they can run alone or stacked.
- **Tremolo** (amplitude mod): Rate + Tempo Sync, Depth, Shape (sine/tri/
  square/saw + smoothing), Stereo/Phase (auto-pan or L-R phase offset), Mix,
  Enable.
- **Vibrato** (pitch mod): Rate + Tempo Sync, Depth (cents/semitones), Shape,
  Mix (dry blend = chorus-like doubling), Enable.
- Whole-mix global FX, tempo-synced (distinct from the per-voice LFO mod
  matrix, which can also do trem/vib). Continuous controls become mod-matrix
  destinations where sensible.

## Reorderable FX chain (running list)

Modules now in the drag-reorder chain: **Dist, Chorus, Mod (Phaser/Flanger),
Trem/Vib, Delay, Reverb (FDN modes), Convolve, EQ (parametric), Limiter**
(Limiter defaults last). ~9 modules; the custom tab bar + per-preset order need
to scale to this.

## 11. Panic button — AGREED

- Always-visible momentary **PANIC** button in the **header, top-right by the
  output meter** (subtle by default, warning tint on hover).
- Instantly: kill all active voices (all-notes-off + all-sound-off), clear the
  **arp latched/held-note stack** (the likely cause of Phil's stuck sound — arp
  latch keeps notes going after release), reset the on-screen keyboard state.
- Also respond to incoming MIDI **All Notes Off / All Sound Off** (CC 123/120)
  so a hardware controller's panic works too.
- Implementation: a processor `panic()` that flushes the synth voices + arp
  state + keyboard state (message-thread safe / RT-safe flag); UI button and the
  CC handler both call it.

## 12+. (more features to come — Mike is still listing them)

- TBD
