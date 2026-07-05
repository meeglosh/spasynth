# Arsenal

Hybrid wavetable + SFX library soft synth by Silverplatter Audio.

Wavetable synthesis with an organic-chaos modulation layer, plus a sample/SFX
engine where the Silverplatter Audio library acts as both oscillator and
modulation source. See `arsenal-claude-code-brief.md` for the full spec.

## Building

Requirements: CMake ≥ 3.24, Ninja (or Xcode/VS), a C++20 compiler.
JUCE 8.0.14 is vendored as a submodule.

```sh
git clone --recurse-submodules <repo-url>
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Formats: Standalone, VST3 (macOS + Windows), AU (macOS). AAX is deliberately
out for v1. Release/universal builds: add `-DARSENAL_UNIVERSAL_BINARY=ON`.

Built plugins are copied to the user plugin folders automatically
(disable with `-DARSENAL_COPY_PLUGIN=OFF`).

## Tests

```sh
cmake --build build --target ArsenalTests && ctest --test-dir build
```

## Layout

- `source/params/` — the parameter registry: single source of truth for every
  parameter (range, default, section, mod-destination flag, randomization
  metadata). The mod matrix, RANDOMIZE ALL, UI, and host automation all read
  from this table. Add parameters here, never ad hoc.
- `source/dsp/` — real-time DSP. No allocation, locks, or I/O on the audio thread.
- `source/ui/` — editor; all colors/fonts/spacing come from `ui/Theme.h` tokens.
- `assets/branding/` — Silverplatter Audio logo SVGs (embedded via BinaryData).
- `Silverplatter Audio packs/` — SFX library zips (gitignored, 63 GB; content
  never goes in the code repo).

## Licensing notes

Commercial closed-source product: requires a paid JUCE license tier
(Indie/Pro) and the Steinberg VST3 license agreement before distribution.
`JUCE_DISPLAY_SPLASH_SCREEN=0` assumes the paid tier.
