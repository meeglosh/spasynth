# SPASynth

Hybrid wavetable + SFX library soft synth by Silverplatter Audio.

Wavetable synthesis with an organic-chaos modulation layer, plus a sample/SFX
engine where the Silverplatter Audio library acts as both oscillator and
modulation source. See `spasynth-claude-code-brief.md` for the full spec.

## Building

Requirements: CMake ≥ 3.24, Ninja (or Xcode/VS), a C++20 compiler.
JUCE 8.0.14 is vendored as a submodule.

```sh
git clone --recurse-submodules <repo-url>
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Formats: Standalone, VST3 (macOS + Windows), AU (macOS). AAX is deliberately
out for v1. Release/universal builds: add `-DSPASYNTH_UNIVERSAL_BINARY=ON`.

Built plugins are copied to the user plugin folders automatically
(disable with `-DSPASYNTH_COPY_PLUGIN=OFF`).

## Tests

```sh
cmake --build build --target SPASynthTests && ctest --test-dir build
```

## Release / packaging

One command builds the whole Shopify deliverable (universal Release build,
tests, macOS installer, library zips, download folders for both SKUs):

```sh
./scripts/build_release.sh              # full run (library packaging included)
./scripts/build_release.sh -            # skip library packaging (faster)
```

Output lands in `dist/shopify/SPASynth-{Standard,Pro}-<version>/`. The
Windows installer comes from CI (`spasynth-installer-Windows` artifact,
built with Inno Setup from `installers/windows/SPASynth.iss`) — drop it
into both folders, zip them, and upload to Shopify.

Pipeline pieces, all runnable standalone:

- `scripts/build_library.sh` — raw pack zips → 24/48 pack folders.
- `scripts/package_library.sh` — pack folders → per-pack customer zips +
  the Standard edition's starter selection (5 sounds per pack). Zips embed
  the full `Silverplatter Audio/SPASynth Library/<Pack>/` path so extracting
  into `/Users/Shared` (macOS) or `C:\Users\Public\Documents` (Windows)
  lands on SPASynth's auto-discovery paths.
- `installers/macos/build_installer.sh` — pkgbuild/productbuild with AU /
  VST3 / Standalone choices. Unsigned by default; set
  `SPASYNTH_CODESIGN_IDENTITY`, `SPASYNTH_INSTALLER_IDENTITY` and
  (optionally) `SPASYNTH_NOTARIZE_PROFILE` to sign + notarize.
- `installers/windows/SPASynth.iss` — Inno Setup; define `SignToolCmd` to sign.

SKU model: one binary, content-differentiated. Standard ships the starter
library; every full pack is an add-on zip that installs the same way. Pro
ships all 88 packs. No DRM anywhere, by design.

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
