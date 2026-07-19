# SPASynth — Marketing Brief

Source document for building the product landing page. Everything below is
factually accurate to the shipping v1.0.1 build — do not invent features or
numbers beyond what is listed here.

---

## Product in one sentence

SPASynth is a hybrid synthesizer built around the entire Silverplatter Audio
sound-effects library — 88 packs and 11,474 production-ready sounds that act
as oscillators, granular fuel, and modulation sources inside a full wavetable
synth with an organic-chaos engine and a one-button patch randomizer.

## Positioning & audience

- **Who it's for:** sound designers for film/TV/games, media composers,
  producers who want cinematic and organic textures fast, and anyone bored of
  static presets.
- **The hook:** other synths give you waveforms. SPASynth gives you a
  Foley-grade SFX library *as the synthesis material* — a typewriter, a rain
  storm, or a billiard break becomes a playable keyboard instrument, a
  granular cloud, or the modulation source that pumps the filter.
- **Key differentiators (in priority order):**
  1. The Silverplatter library IS the oscillator — and the modulator. Each
     loaded sound's own amplitude and pitch contours become mod sources.
  2. RANDOMIZE ALL with per-section locks and a WILD amount knob — a genuine
     patch generator, not a gimmick (every roll is guaranteed to make sound).
  3. Organic Chaos: analog-style instability (pitch/phase/position/amp drift
     plus saturation & distortion drift) that makes digital patches breathe.
  4. **No DRM. Ever.** No serial numbers, no activation, no iLok, no
     phone-home. Buy it, download it, own it. (This is a headline-worthy
     brand position — lean into it.)
- **Brand voice:** warm, confident, minimal. Silverplatter Audio's aesthetic
  is mid-century-modern minimalism; the plugin UI is a deep blue-teal dark
  theme accented in Silverplatter teal (`#51D0BF`) — which the user can
  re-tint to any colors they like (audio and modulation accents can differ).
- **Who makes it:** SPASynth is made by Silverplatter Audio. Always speak in
  the company's own first-person voice ("we"/"our"/"Silverplatter Audio"),
  never in the third person ("they"/"their") and never naming individuals.
  We are primarily a boutique sound-effects library company, and SPASynth is
  the instrument we built to play our libraries.
- **About Silverplatter Audio (reusable copy):** Silverplatter Audio is a
  boutique sound-effects library company. Drawing on years of recording and
  mastering game audio, sound design for film, and sound effects for video
  and music production, we create professional, high-quality sound-effects
  packs for sound designers around the world, with a focus on usability,
  convenience, and affordability so audio drops cleanly into games, film, TV,
  web, and more. SPASynth is the instrument we built to play those libraries.

## Complete feature set (v1.0.1)

### Sound engines — 7 per oscillator slot, 3 slots
- **Wavetable** — mip-mapped, alias-free, with position morphing, up to
  8-voice unison (detune/blend/width), and user wavetable import.
- **Sample/SFX** — any WAV becomes a playable instrument: keytracking with
  root note, start offset, loop points. Ships pointed at the Silverplatter
  library.
- **In-pack quick-swap** — audition every variant in a pack straight from the
  oscillator: click the sample name for a dropdown of the whole pack. No file
  dialogs, no browser trip. Turns 11,474 sounds into something you can
  actually explore while you play.
- **Granular** — grain size, density, position, spray, and pitch over any
  sample; scrub the playhead with an LFO for evolving textures.
- **Virtual Analog** — classic PolyBLEP shapes with pulse-width control.
- **FM** — 2-operator FM with ratio and modulation index.
- **Noise** — white / pink / brown.
- **Pluck** — Karplus-Strong plucked string with damping.

### The SFX-as-modulator system
- Every loaded sound is analyzed on import (amplitude follower + pitch
  tracking); each slot contributes **SFX Amp** and **SFX Pitch** modulation
  sources to the matrix. Route a thunderstorm's loudness to filter cutoff and
  the storm plays the synth.

### Filters — two of them
- Dual multimode filters, 8 types each (LP/HP/BP/Notch, 12 & 24 dB), with
  drive, keytracking, dedicated envelope depth (±4 octaves), and dry/wet mix.
- **Series or parallel routing** — band-carve with LP→HP or run split-band
  textures in parallel.

### Organic Chaos
- A bank of slow random walkers adds analog instability per voice: pitch,
  phase, wavetable-position, and amplitude drift, plus drifting saturation
  and distortion for harmonic movement. Global depth, rate, and mix — all
  modulatable, with an animated chaos scope.

### Modulation
- 16-slot mod matrix; 20 sources (3 envelopes, 3 LFOs, macros, velocity, mod
  wheel, aftertouch, chaos, 6 SFX followers) into 90+ destinations.
- 3 LFOs: 6 shapes, tempo sync (host-locked), retrigger, unipolar mode,
  phase.
- 3 ADSR envelopes with live-animated displays.
- Per-voice modulation evaluated at 64-sample resolution — smooth, no zipper.

### RANDOMIZE ALL
- One button rolls an entire playable patch. **WILD** knob sets how polite or
  unhinged. **Per-section locks** (OSC, FILTER, ENV, LFO, CHAOS, ARP, FX,
  MATRIX) keep what you love and re-roll the rest. Musically weighted — every
  parameter carries hand-tuned randomization ranges and biases.

### Arpeggiator
- 12 modes: up, down, up/down, down/up, inclusive, converge, diverge,
  as-played, chord, random, random-walk, and melodic **phrases** (12 built-in
  interval patterns).
- Tempo-synced to the host, swing, gate, up to 4 octaves, latch, velocity
  modes (as played / fixed / accent).
- **Probability controls:** CHANCE (steps fire or rest), STUTTER (ratchet
  rolls of 2–4 repeats), JUMP (random octave leaps), HUMAN (per-hit velocity
  spread). Patterns become performances.

### Playability
- Global glide/portamento: Off / Always / Legato modes, 1 ms–2 s, applied
  across every engine and both filters' keytracking (works on arp steps too).
- 16-voice polyphony. Pitch bend, mod wheel, aftertouch.
- **MIDI Learn on everything** — right-click any control, move a hardware
  knob, done. Mappings persist with your session, never hijacked by presets.

### Effects
- Distortion (multiple curve types, tone, mix), chorus, tempo-syncable delay
  with ping-pong, reverb, 3-band EQ — each with a live signal-fed scope.

### The library & presets
- **88 Silverplatter Audio packs, 11,474 sounds, 24-bit/48 kHz** —
  production-ready for film and TV work (Pro edition; Standard ships a
  440-sound starter selection spanning every pack).
- Library auto-discovery: extract to one folder and SPASynth finds it. No
  scanning dialogs, no locate-your-content prompts.
- **264 factory presets** auto-generated from the library — every pack gets a
  playable Keys patch, a granular Texture, and an SFX-driven Pulse.
- Slide-out preset browser: live search, type filters, pack filter,
  persistent favorites.
- Presets use portable paths — sessions and presets survive the library
  living anywhere.

### The look
- Resizable, GPU-friendly vector UI that scales from laptop to studio
  display. Live telemetry animates every scope, envelope, LFO, and filter
  curve from the actual audio engine.
- **User-tintable accent colors** — a built-in picker re-tints the whole UI;
  link both accents for a single-color look.

### Formats & requirements
- **macOS 11+** (universal: Apple silicon + Intel): AU, VST3, Standalone.
- **Windows 10+ (64-bit)**: VST3, Standalone.
- No AAX in v1.

### Editions & upgrade path
- **SPASynth Standard** — the full synth, all features, all 264 presets, with
  a 440-sound starter library (5 representative sounds from all 88 packs).
- **SPASynth Pro** — the same synth with the complete 11,474-sound,
  37 GB library.
- **Standard → Pro upgrade** — pay the difference, download the full library,
  drop it in the same folder. No reinstall, nothing to activate.
- **Add-on packs** — individual full packs installable the same way.
- Every copy can carry a personal ownership stamp ("Licensed to you@…") shown
  in the footer — a signature, not a lock.

## The no-DRM statement (use prominently)

> SPASynth has no copy protection, no serial numbers, no activation, no
> dongles, and it never phones home. We sell to people who pay for the tools
> they love, and we price it accordingly. The license is simple: use the
> sounds in anything you make, royalty-free, forever — just don't
> redistribute the raw files.

## Changelog

### v1.0.1 — Initial release
- 7 synthesis engines per slot: wavetable, sample/SFX, granular, virtual
  analog, 2-op FM, noise (white/pink/brown), Karplus-Strong pluck
- SFX-as-modulator: automatic amplitude + pitch follower analysis on every
  loaded sound, exposed as 6 mod-matrix sources
- Dual 8-type multimode filters with series/parallel routing, drive,
  keytracking, envelope depth, and mix
- Organic Chaos engine: per-voice pitch/phase/position/amp drift +
  saturation/distortion drift, fully modulatable
- 16-slot modulation matrix, 3 LFOs (host-sync), 3 envelopes, 64-sample
  modulation resolution
- RANDOMIZE ALL patch generator with WILD amount and 8 per-section locks
- Arpeggiator: 12 modes incl. melodic phrases, swing, latch, plus CHANCE /
  STUTTER / JUMP / HUMANIZE probability controls
- Global portamento: Off / Always / Legato, engine-wide including filter
  keytracking
- FX chain with live scopes: distortion, chorus, sync delay (ping-pong),
  reverb, 3-band EQ
- 264 factory presets generated from the Silverplatter library (Keys /
  Texture / Pulse per pack)
- Slide-out preset browser: search, type & pack filters, favorites
- Library auto-discovery with portable preset/session paths
- MIDI Learn via right-click on every control
- Resizable telemetry-animated UI with user-tintable accent colors
- macOS universal (AU/VST3/Standalone) + Windows x64 (VST3/Standalone)
- No DRM of any kind

## Available assets

- `docs/spasynth-marketing.png` — THE hero shot for the website: full UI at
  2x/retina (2760×1800), default teal accents, no drawer, a sample loaded
  in oscillator A. Regenerate any time with
  `SPASynthTests --snapshot <dir>` (renders pick up the machine's saved
  accent colors — use a defaults machine state).
- `docs/spasynth-dark.png` — full UI screenshot (default colors, preset
  browser open, FILTER 2 and DELAY panels visible)
- `docs/spasynth-accent.png` — same UI re-tinted violet/lime, demonstrating
  the accent color picker
- `assets/branding/` — Silverplatter Audio logo SVGs (black + white, square
  and banner variants)
- UI palette (default theme): background `#0c1114`, panels `#151c21`, text
  `#e7ecef` / `#7f8d97`, accents (audio + modulation) `#51d0bf`
  (Silverplatter teal)

## Landing page guidance

- Tone: confident and warm, not hype-y. Short sentences. The product is
  premium but the brand is human — the no-DRM stance is part of the voice.
- Suggested section order: hero (screenshot + one-liner) → "your library is
  the synth" → randomize/chaos → deep-dive feature grid → editions/pricing
  cards with upgrade path → no-DRM statement → specs → FAQ.
- FAQ fodder: Will my DAW support it? (AU/VST3 list) · Do I need the library
  on my system drive? (no — point SPASynth anywhere) · What happens when I
  upgrade to Pro? (download, extract, done) · Is there copy protection? (no,
  and here's why) · AAX/Pro Tools? (not in v1).
- Do not fabricate: user counts, awards, testimonials, or demo videos. Prices
  are set (see `docs/shopify-listings.md`).
