# SPASynth — project state & working guide

Commercial hybrid soft synth by **Silverplatter Audio** (Paul, Phil, and
Mike; Mike is the primary dev contact here), sold
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

**Smoke testing found + fixed four shipping bugs (all committed/pushed):**
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
4. The AU *installed then vanished* on upgrade installs (receipt written,
   bundle gone from `/Library/...`; Logic never saw it). Root cause: all
   three bundles shared one CFBundleIdentifier, and PackageKit keys its
   payload "atomic shove" bookkeeping on the bundle id — three same-id
   payloads in one install collide and the AU gets trashed right after
   landing (`/var/log/install.log` showed "Parent bundle … will be
   atomically shoved" ×3 with one id; the fixed pkg logs three distinct
   ids). Fix: per-format CFBundleIdentifier patched into the JUCE-generated
   plists at configure time (CMakeLists.txt — AU = `…spasynth.au`, VST3 =
   `…spasynth.vst3`, standalone keeps the base id since it owns the TCC
   grants), plus `BundleIsRelocatable=false` component plists in
   `build_installer.sh`. Same scheme Arturia/Soundtoys/Softube ship. Host
   compat unaffected (AU identity = aumu/SpSy/SpAu; VST3 = class UUID).

**macOS smoke test now PASSES end-to-end**: the reinstalled pkg laid down
all three formats and they stayed put; icon + Bluetooth prompt OK; auval +
pluginval pass on the installed copies; SPASynth loads and plays in Logic.
The pkg is still UNSIGNED, so first launch needs right-click → Open.

Follow-up polish from the same smoke-test day (all committed/pushed; Logic
behaviour confirmed by Mike where noted):
- `2251488` — library discovery falls back to any WAV-holding folder inside
  a "Silverplatter Audio" dir when no folder named "SPASynth Library"
  exists. (Mike's "samples not loading" was the dev symlink still pointing
  at ~/arsenal after the repo folder rename — repointed.)
- `91bd70d` — macOS Tahoe + Logic's AUHostingService opens the editor with
  a stale hit-test region (top strip dead until a knob moves; Apple bug,
  hits non-JUCE plugins too, REAPER/standalone immune). Workaround: 1px
  resize nudge + repaint after the editor first shows (AU/macOS only,
  `parentHierarchyChanged`). Confirmed fixed in Logic.
- `f1d18c6` — loading state while slot content loads: sweep bar + dimmed
  stale waveform + "loading..." header label; per-slot atomic pendingLoads
  counters on the processor; third snapshot `spasynth-loading.png`.
- `f7aee02` — both accents default to Silverplatter teal #51D0BF (the old
  orange/cyan pair read too close to MiniFreak); LINK defaults on and the
  picker's RESET re-links.
- `ff537aa` — brand wordmark centred on true glyph ink via path bounds
  (GlyphArrangement's box is advance-based; tracked text sat 5.5px left).
- `826cf82` — coarse tune excluded from RANDOMIZE ALL (semitone jumps break
  the song key; fine detune still rolls; same pattern as rootNote).

Dev-machine notes from the bug-4 session:
- Both build trees had stale CMake caches from the `~/arsenal` →
  `~/spasynth` folder rename; both were reconfigured from scratch.
- installd (as root) had earlier "relocated" an app payload INTO the build
  tree — a root-owned `build-release/SPASynth_artefacts/Release/Standalone/
  SPASynth.app.root-junk` remains; Mike can `sudo rm -rf` it whenever.
- Logic caches per-version validation verdicts: after replacing a
  same-version AU, use Plug-in Manager → Reset & Rescan Selection, then
  RESTART Logic (the plugin menu is built at launch).
- Dev builds copy Debug plugins into ~/Library, which shadow the installed
  /Library release copies in Logic — clear them (`rm -rf ~/Library/Audio/
  Plug-Ins/{Components/SPASynth.component,VST3/SPASynth.vst3}`) whenever
  Mike is smoke-testing the installed release.
- GitHub repo renamed to `meeglosh/spasynth` (remote updated).

**macOS signing/notarization: DONE (2026-07-17).** Kenzora Games Developer
ID (team `7K9WY5T49S`); certs imported to login keychain (had to use
`security import` via CLI — double-click threw -25294 on this macOS), keys
authorized for the signing tools with `security set-key-partition-list`
(else codesign stalls on a GUI prompt), notary creds stored as keychain
profile `SPASYNTH_NOTARY` (app-specific password). The shipping pkg
`dist/installers/SPASynth-1.0.0-macOS.pkg` is signed + notarized + stapled
(spctl: "Notarized Developer ID / accepted") and copied into both
`dist/shopify/` folders. To re-sign a future build:
`export SPASYNTH_CODESIGN_IDENTITY="Developer ID Application: Kenzora Games (7K9WY5T49S)"`,
`SPASYNTH_INSTALLER_IDENTITY="Developer ID Installer: Kenzora Games (7K9WY5T49S)"`,
`SPASYNTH_NOTARIZE_PROFILE="SPASYNTH_NOTARY"`, then `./scripts/build_release.sh`.

**Windows installer: BUILT + STAGED (2026-07-17).** CI had a latent bug —
the Inno `/O` output path used `..\..\dist\installers` (correct base for
Source paths, which are .iss-relative, but `/O` is CWD-relative), so the
`.exe` landed two levels above the workspace and the artifact upload found
nothing on every prior run. Fixed in `e719088` (absolute
`%GITHUB_WORKSPACE%` path). `SPASynth-1.0.0-Windows.exe` (unsigned by
decision) is now in `dist/installers/` and both `dist/shopify/` folders.
Both SKU folders are complete: signed pkg + exe + library zips + 3 docs
(Standard 3.0 GB, Pro 32 GB; every Pro part < 5 GB Shopify cap).

**Post-prep UI polish (all committed, verified in Logic, in the shipping
build 7524a4c):** flat modern knobs (thin ring + accent arc + position dot,
replacing the skeuomorphic disc; `664e2be`), a padlock glyph on locked
section buttons, and animated granular playback — the waveform display now
shows the live grain cloud (per-slot `Telemetry::GrainViz`, published each
mod chunk; `7524a4c`). The signed/notarized pkg and the fresh Windows exe
in both `dist/shopify/` folders include all three.

**Product listings written** to `docs/shopify-listings.md` (descriptions,
SEO fields, card blurbs, FAQ, pricing, per-SKU attachment lists; no em
dashes). Sound count corrected to 11,474 everywhere (verified WAV count;
spasynth.com still says 11,401 and needs updating outside the repo).

**Official pricing (from spasynth.com):** Standard $99 intro / $149 reg;
Pro $702 sale / $1264 reg; Upgrade = the difference, $603 intro / $1115
reg (set Shopify Price = intro, Compare-at = reg). Intro-period length TBD.

**Remaining for launch (Mike's manual steps, nothing to code):**
1. Real-DAW smoke test on Windows (macOS is DONE). Load the VST3 in
   Reaper/Live/Cubase, confirm library auto-discovers. Windows unsigned →
   "More info → Run anyway" past SmartScreen.
2. Shopify: create the 3 products, paste descriptions, set prices +
   compare-at, attach files per SKU (Upgrade = the 11 Pro parts only),
   test-purchase, go live.
4. Upload per the attachment list `build_release.sh` prints: Standard (pkg,
   exe, `SPASynth Starter Library.zip`, 3 docs), Pro (pkg, exe, 11 × 
   `SPASynth Pro Library (Part N).zip`, docs), Upgrade (the 11 parts only).
   Add-on pack products later from `dist/library/packs/`. Files stay under
   Shopify's 5 GB cap; upload as individual attachments, not one giant zip.
5. Pricing, product copy (see `docs/marketing-brief.md` — self-contained
   source for the landing page).

Business decisions of record: two SKUs differentiated by **content only**
(one binary, no gating). Standard = full synth + 440-sound starter library;
Pro = all 88 packs / 11,474 WAVs / 37 GB at 24/48 (24/96 originals archived
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
