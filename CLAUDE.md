# SPASynth — project state & working guide

Commercial hybrid soft synth by **Silverplatter Audio** (Mike Jerugim), sold
via Shopify. JUCE 8.0.14 (submodule `libs/JUCE`), C++20, CMake + Ninja.
Formats: AU/VST3/Standalone (macOS universal), VST3/Standalone (Windows x64).
AAX deliberately out for v1. Original spec: `spasynth-claude-code-brief.md`
(the project was renamed Arsenal → SPASynth; the repo folder is still
`arsenal`, plugin code `SpSy`, manufacturer `SpAu`).

## Where the project stands (2026-07-11)

**v1.0.0 — feature-complete, packaged, in macOS smoke testing.** All 10 brief
checkpoints are done, plus post-brief features: dual filters
(series/parallel), UAD-style preset browser drawer, glide (Off/Always/Legato),
arp probability controls (chance/stutter/jump/humanize), user-tintable accent
colors (light mode removed), license.txt ownership stamp in the footer,
2-decimal value readouts, and the full packaging pipeline.

**Smoke testing found + fixed three shipping bugs (all committed/pushed):**
1. `33abff0` — installer only laid down the Standalone; AU/VST3 silently
   didn't install. Cause: all three component pkgs shared one identifier
   (`com.silverplatteraudio.spasynth`, derived from the common
   CFBundleIdentifier). Fix: explicit unique `--identifier` + `--version` per
   `pkgbuild --component` (`installers/macos/build_installer.sh`).
2. `33abff0` — standalone had no app icon. Fix: `assets/branding/app_icon.svg`
   (+ `make_app_icon.sh` → `app_icon.png`) wired as `ICON_BIG` in
   `juce_add_plugin`; JUCE emits the `.icns`/`.ico`.
3. `9b15cd2` — standalone hard-crashed (SIGABRT via macOS TCC) on the
   Bluetooth MIDI menu. Cause: no `NSBluetoothAlwaysUsageDescription` in
   Info.plist. Fix: `BLUETOOTH_PERMISSION_ENABLED/_TEXT` +
   `MICROPHONE_PERMISSION_ENABLED/_TEXT` in `juce_add_plugin` (mic added
   pre-emptively — audio input would TCC-crash the same way).

Release artefacts + `dist/installers/SPASynth-1.0.0-macOS.pkg` were rebuilt
with all three fixes and verified (unique ids, icon present, permission
strings present). **Immediate next step: Mike reinstalls the fresh pkg and
confirms** — `sudo installer -pkg dist/installers/SPASynth-1.0.0-macOS.pkg
-target /` (needs admin; can't be run from the agent shell), then check AU +
VST3 appear, the app icon shows, and Bluetooth MIDI prompts instead of
crashing. The pkg is still UNSIGNED, so first launch needs right-click → Open.

**Remaining for launch (Mike's manual steps, nothing to code):**
1. Set signing env vars (`SPASYNTH_CODESIGN_IDENTITY`,
   `SPASYNTH_INSTALLER_IDENTITY`, optional `SPASYNTH_NOTARIZE_PROFILE`) and
   re-run `./scripts/build_release.sh` for a signed/notarized pkg. (The
   Bluetooth/mic usage strings are also a notarization requirement — now in
   place.)
2. Download the `spasynth-installer-Windows` CI artifact into both
   `dist/shopify/` folders. (Windows standalone gets the icon automatically
   from the same `ICON_BIG`.)
3. Real-DAW smoke test on both platforms (ideally a clean user account).
   macOS pass is underway (the three fixes above came out of it); Windows
   not yet exercised.
4. Upload per the attachment list `build_release.sh` prints: Standard (pkg,
   exe, `SPASynth Starter Library.zip`, 3 docs), Pro (pkg, exe, 11 × 
   `SPASynth Pro Library (Part N).zip`, docs), Upgrade (the 11 parts only).
   Add-on pack products later from `dist/library/packs/`. Files stay under
   Shopify's 5 GB cap; upload as individual attachments, not one giant zip.
5. Pricing, product copy (see `docs/marketing-brief.md` — self-contained
   source for the landing page).

Business decisions of record: two SKUs differentiated by **content only**
(one binary, no gating). Standard = full synth + 440-sound starter library;
Pro = all 88 packs / 11,401 WAVs / 37 GB at 24/48 (24/96 originals archived
by Mike). **No DRM ever** — no serials, no activation (Mike re-confirmed
after considering a serial system; the license.txt footer stamp is the
agreed alternative). Upgrade path is handled entirely in Shopify.

## The verification ritual (do this for every change)

1. `cmake --build build --target SPASynthTests` then run
   `build/SPASynthTests_artefacts/Debug/SPASynthTests` → expect `ALL PASS`
   (140+ assertions). Fix every new compiler warning — one caught a real
   Filter-2 lock bug.
2. UI changes: render snapshots and **actually look at them**:
   `SPASynthTests --snapshot <dir>` writes `spasynth-dark.png` (preset
   browser open, FILTER 2 + DELAY fronted) and `spasynth-accent.png`
   (violet/lime re-tint). Note: renders pick up the *user's saved accent
   colors* from machine settings; the committed `docs/*.png` are the visual
   regression record — refresh them from a defaults run.
3. `cmake --build build --target SPASynth_AU SPASynth_VST3` then
   `auval -v aumu SpSy SpAu` and
   `pluginval --strictness-level 8 --validate ~/Library/Audio/Plug-Ins/VST3/SPASynth.vst3`.
4. Commit + push at every completed feature (Mike expects this cadence).

Dev build dir is `build/` (Ninja, Debug, plugins copied to user plugin
folders). Release: `./scripts/build_release.sh` (uses `build-release/`,
universal, runs tests, builds pkg, packages library, assembles
`dist/shopify/`; pass `-` to skip the slow library packaging). Ignore stale
clangd/IDE diagnostics ("juce not found" etc.) — the build is the arbiter.

## Load-bearing invariants (break these = break users' sessions)

- **`source/params/ParameterRegistry.*` is the single source of truth** for
  every parameter (range/default/section/mod-dest flag/RandomSpec/choices).
  Add params there, never ad hoc. New params are keyed by ID so placement is
  free, EXCEPT:
  - **ModSource enum is append-only** (serialized in matrix route choices).
  - **Mod-destination order is append-only**: dense dest indices are
    serialized; new destination params must be appended AFTER all existing
    dests in registry order (capacity `maxModDests = 96`).
  - Choice-parameter orders (filter types, osc modes, arp modes, LFO shapes,
    divisions, glide modes) are load-bearing and append-only.
- **Real-time safety**: no allocation/locks/IO on the audio thread.
  Content loads on background threads → atomic live pointers → timer-deferred
  retirement (processor). Fixed-capacity everything in voices/arp
  (`Arpeggiator` pending-ratchet queue, preallocated scratch MidiBuffer).
- **Per-voice modulation at 64-sample chunks** in normalized space; chaos and
  granular followers use one-chunk-latency feedback. `SharedState` is written
  once per block by the processor; voices only read (glide origin/key-count
  is the exception — written by `GlideSynthesiser` note hooks in event order).
- **UI reads all colors/fonts/metrics from `source/ui/Theme.h` tokens at
  paint time.** No light theme; accents are user preferences (see picker).
  Value formatting is set at parameter construction (adaptive ≤2 decimals).
- **Machine settings** (library root, favorites, accent colors + link,
  MIDI-learn map is session-state not settings) go through the single
  `PropertiesFile` singleton in `source/library/Library.cpp` — never create
  another PropertiesFile (that pattern caused the theme-reset bug).
- **Portable paths**: sample/wavetable refs inside presets/sessions are
  `$LIB$/...` relative to the library root.
- Presets: `.spasynth` XML; factory presets are generated per pack
  (Keys/Texture/Pulse from smallest/middle/largest WAV — the starter library
  intentionally includes exactly those files so Standard presets all load).
- MIDI-Learn maps ride host sessions but are **excluded from presets**.

## Map

- `source/SPASynthProcessor.*` — APVTS, raw-pointer caches (`Raw` struct),
  `updateSharedState`, `buildStateTree/restoreStateTree`, randomizeAll,
  library refresh, content storage.
- `source/dsp/` — `SPASynthVoice` (7 engines/slot, dual filters, glide,
  chaos), `Arpeggiator` (modes + probability), `FXChain`, `Telemetry`
  (lock-free audio→UI), loaders.
- `source/params/` — registry + `Randomizer` (lock groups, wildness).
- `source/library/` — settings singleton, auto-discovery
  (`/Users/Shared/Silverplatter Audio/SPASynth Library` etc.), PresetManager,
  license stamp (`getLicenseLine`).
- `source/ui/` — `Theme.h` tokens, LookAndFeel, `SPASynthEditor`
  (ContentComponent + fixed-aspect scaling shell + accent picker),
  `PresetBrowser` drawer, `ModulePanels`, `Displays` (telemetry scopes),
  `Controls.h` (Knob/Choice/Toggle with paramID props for MIDI Learn).
- `tests/SPASynthTests.cpp` — the whole suite + `--snapshot` renderer.
- `scripts/` — `build_library.sh` (zips→24/48 packs), `package_library.sh`
  (pack zips + starter + Pro volumes; APFS-clone staging, idempotent),
  `build_release.sh` (one-shot release + Shopify folders).
- `installers/` — macOS pkg builder (unique per-component ids + signing
  hooks), Windows Inno `.iss`.
- `packaging/` — customer docs (README/QUICKSTART/EULA),
  `license-template.txt` (per-order ownership stamp).
- `assets/branding/` — logo SVGs + `app_icon.svg`/`app_icon.png` (standalone
  icon, wired via `ICON_BIG`); regen the PNG with `make_app_icon.sh`.
- Local-only (gitignored): `library/` (88 built packs, symlinked to
  `/Users/Shared/...`), `Silverplatter Audio packs/` (raw zips),
  `dist/`, `build*/`. A starter-library copy for Standard-experience testing
  lives at `/Users/Shared/Silverplatter Audio/SPASynth Starter Library`.

## Conventions & gotchas

- Comment style: explain constraints/why, sparingly; match existing density.
- CI (`.github/workflows/build.yml`): macOS universal + Windows x64 + both
  unsigned installers as artifacts. macOS job is slow (~1 h JUCE build).
- GitHub remote: `https://github.com/meeglosh/SPASynth.git`. Mike's PAT has
  repo+workflow scopes but not admin.
- Snapshot tests front tabs/drawer via `dynamic_cast` walks; keep component
  types discoverable if refactoring.
- `juce_add_binary_data` asset list: the white square logo filename really is
  `SPAudio_logo_sqaure_white.svg` (upstream typo, kept).
- Old "Arsenal"-named DAW sessions predate the rename and won't reconnect.
- Release rebuilds: never hand-`rm` subdirs inside
  `build-release/SPASynth_artefacts/` — Ninja relinks the binary but skips the
  bundle-assembly steps (Info.plist, VST3 manifest), leaving half-built
  bundles that `pkgbuild` rejects. To force a clean rebuild, delete the whole
  `SPASynth_artefacts/` dir (the compiled objects live in
  `build-release/CMakeFiles/`, so this re-archives + relinks + reassembles in
  ~40s with no source recompile).
- Installing the pkg needs admin (`sudo installer -pkg … -target /`); the
  agent shell can't sudo, so the actual install is always Mike's step.
