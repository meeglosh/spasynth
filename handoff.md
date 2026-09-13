# SPASynth handoff (2026-09-11)

Quick "start here" for the next session. Full detail lives in `CLAUDE.md`; this
is the short version.

## Where we are

- **2026-09-11: v1.0.15 built + staged (main `656a8bb`), awaiting Mike's
  install + gauntlet, then it goes to Paul and Phil.** The largest round
  since launch prep: 31 commits of tester feedback (Mike + Paul + Phil),
  all in one customer-voice `## 1.0.15` changelog section. Plate reverb
  replacing the FDN, % MIX knobs, Crush distortion, SUB osc, wavetable
  TABLE menu, sample SYNC (beat/transport-locked loops + time signatures),
  waveform zoom/pan, octave shift, EQ band types/slopes, mod matrix ASSIGN
  mode, live modulation display on knobs, scrolling chaos trace, bold FX
  tabs, preset browser beside the synth, fit-to-screen window, opaque
  editor (Logic flicker), MIDI Learn fixed (menu anchoring + message-thread
  apply + a diagnostic badge), never-silent RANDOMIZE ALL, factory preset
  recipes v7. macOS pkg md5 `31ae82e03ac4f84de4acbdf1f2ab3f0e` (from
  `517557a`; `656a8bb` is a Windows-only test compile fix), Windows exe md5
  `93dc0fa8b67e49f61a464e4e15f9f1f6` (`ci-windows-656a8bb`), both in
  `dist/installers/` and `dist/shopify/SPASynth-{Standard,Pro}-1.0.15/`.
  **Standing rule from Mike: the agent orchestrates Sonnet subagents and
  only verifies/corrects; it does not code.** Read CLAUDE.md's 2026-09-11
  section for the agent-management and test-hygiene lessons (tests must
  never touch Mike's real settings or presets; every popup needs a focus
  anchor; run agents foreground-only). MiniFreak encoders not reaching
  Logic at all = controller setting, closed. Repo PUBLIC.

- **2026-09-07: v1.0.14 built, staged, INSTALLED, and CONFIRMED by Mike.**
  1.0.13 got installed and confirmed (the VOICE close-window crash was gone),
  but before it went to testers Mike hit a NEW Logic crash recording a second
  MIDI track: arpeggiator + count-in, negative host ppq -> negative `%` ->
  out-of-bounds stack read. Fixed in `2585783`, version bumped to 1.0.14.
  Then an AddressSanitizer build of the test suite (now ritual step 1b, see
  below) turned up three more real bugs, fixed in `81236ad`: (a) FDNReverb
  read one float past a delay line on float-wrap rounding — almost certainly
  the cause of the reverb noise bursts that got `audit-hardening` abandoned
  on 2026-09-05; same guard added to delay/ModEffect/TremVib; (b) the VOICE
  call-out panel could outlive the processor on project close, now detached
  synchronously in `~ContentComponent`; (c) the intermittent
  `voicePanelEditorCloseTest` flake was a test bug (raw `CallOutBox*` across
  message pumps), now a SafePointer — this flake also aborts
  `build_release.sh` if it fires. 1.0.14 built + staged 2026-09-07: macOS pkg
  md5 `68edd892e562370c765c005627dfb376` (signed/notarized/stapled), Windows
  exe md5 `89003ae52f0c2905eba27f696de34df4` (draft release
  `ci-windows-81236ad`), byte-identical across `dist/installers/` and
  `dist/shopify/SPASynth-{Standard,Pro}-1.0.14/`. Docs commit `878bfc5`. Mike
  installed it (first attempt failed silently because he was given a
  relative pkg path from the wrong directory — **always give him an
  absolute pkg path**) and confirmed the recording crash is fixed. Repo was
  flipped PUBLIC by Mike for the Windows CI run and is still public.
  **Next: v1.0.15 is in progress** — tester-feedback improvements from
  Mike's own playtest sessions with Paul and Phil, starting with "enabled FX
  tabs show their label in bold." 1.0.14 has NOT been sent to Paul and Phil
  yet; the rest of the gauntlet + the 1.0.15 round come first. The
  `hardening-safe` branch (`d7f38c3`) still sits on 1.0.13's main — needs a
  rebase onto current main and Mike's Logic verification before any of it
  ships.

- **2026-09-06: 1.0.13 fixed the Logic crash on closing the window with the
  VOICE call-out open** (call-outs were parented to the AU wrapper's holder,
  which deletes its children; see CLAUDE.md history). Installed + confirmed
  by Mike, but superseded by 1.0.14 above before it reached testers.

- **2026-09-05: 1.0.12 is installed in /Library and confirmed working by
  Mike in Logic (his own session, playback clean).** The `audit-hardening`
  branch (old 1.0.13) was ABANDONED — its build produced reverb-triggered
  pulsing noise bursts that 1.0.12 does not. Cause was unknown at the time;
  **now believed found** — see the FDNReverb off-by-one fix above. Do not
  merge that branch as-is regardless; re-verify anything pulled from it.
  Full story + the Logic-loading lessons in CLAUDE.md's 2026-09-05 section.
  Everything below this bullet is older history.

- **v1.0.8, 1.0.9, and the RANDOMIZE-ALL focus fix are all long since
  confirmed and shipped** (QWERTY focus-steal fixes: the right flag is
  `setMouseClickGrabsKeyboardFocus`, not `setWantsKeyboardFocus` — see
  Gotchas below; the 1.0.9 hardening batch; the ~25-button focus fix that
  followed it). Full history in CLAUDE.md's 2026-08-21 through 2026-08-28
  sections if needed.
- GitHub Actions storage alerts are a known non-issue: the quota is
  account-wide across all of Mike's repos, not per-repo, and the alert
  reflects a cycle-peak measurement, not live usage.

## Library distribution: decisions of record (2026-09-13)

- **SPASynth Pro = SPASynth + the Everything Bundle (EB) entitlement.** No
  separate SPASynth Pro library exists any more. Standard, Pro and Upgrade
  are the same binary; the difference is library access. Pro buyers become
  EB owners and vice versa (same entitlements in Shopify + SPAStation);
  existing EB owners get SPASynth Pro free; future EB includes it.
- **Pro library = the original 24/96 pack zips**, delivered by SPAStation
  (`/Users/mikejerugim/SPAStation`, Electron + Cloudflare Worker, streams
  Shopify Digital Downloads; "Download all" exists). The 24/48 conversion,
  the 11 Pro volumes, R2 and the links file are retired for Pro. SPASynth
  plays 96k as-is (loader keeps source rate, voice resamples); cost is 2x
  disk and 2x RAM (whole-file float in memory). **Accepted for now; add a
  "load samples at 48k" preference only if testers hit memory trouble.**
- **Standard's starter library stays 24/48** (5 sounds from every pack),
  delivered via Shopify directly. It is an upsell bullet (Pro = full
  fidelity originals) AND a marketing surface: **the starter grows with
  every new pack release as a free update** (put this on the Standard PDP
  and the website). So `package_library.sh`'s starter step must become
  re-runnable as packs are added (today it skips if the zip exists);
  library is 90 packs now (Seagulls, Antique Clock), our built `library/`
  still has 88 and older pack folder names.
- **Pack zip layout is fine for the scanner**: `<Pack>/Audio Files/*.wav`
  plus jpg/pdf/.DS_Store; `scanLibrary` finds WAVs recursively per pack
  folder and ignores the rest. Canonical pack folder name for the library
  = the zip's top folder minus the "SilverPlatter Audio - " prefix (the
  same rule `build_library.sh` uses); SPAStation's `catalog-releases.json`
  `name` is the display name.
- **SPAStation gets an "Install to SPASynth library" action** (Mike,
  2026-09-13: build right away): after a verified download, extract the
  zip into `<libraryRoot>/<Pack Name>/`, where libraryRoot is read from
  SPASynth's settings file (`libraryRoot` in
  `~/Library/Application Support/Silverplatter Audio/SPASynth/SPASynth.settings`
  on macOS, `%APPDATA%\Silverplatter Audio\SPASynth\` on Windows) and
  falls back to the default `/Users/Shared/Silverplatter Audio/SPASynth
  Library` (macOS) / `C:\Users\Public\Documents\Silverplatter Audio\SPASynth
  Library` (Windows). Skip `__MACOSX`, `.DS_Store`; a 96k pack replacing a
  48k starter pack of the same name overwrites file-for-file (same names),
  so presets keep resolving. SPASynth rescans on next launch/Rescan.
  Blocked as of 2026-09-13 on SPAStation's working tree having 32
  uncommitted files (Download-all work) in the same files.
- Open question (Mike): send SPASynth Pro buyers to the EB PDP instead of
  a separate Pro PDP.

## What's actually left before launch

1. Mike installs 1.0.15 (`sudo installer -pkg
   /Users/mikejerugim/spasynth/dist/installers/SPASynth-1.0.15-macOS.pkg
   -target /`, Reset & Rescan, relaunch Logic), runs the gauntlet.
2. Send both 1.0.15 installers + the tester note
   (`docs/tester-note-1.0.15.txt`, paste-ready) to Paul and Phil (nothing
   since 1.0.8 has gone out). Bump to 1.0.16 for anything after.
3. Decide: one more tester round after that, or send the announcement
   directly once Mike's happy.
4. Shopify build-out per `docs/shopify-setup-guide.md`.
5. Send `docs/launch-email.md` / `docs/social-posts.md` (now current, reflect
   the full shipping feature set) — marketing site is already confirmed live
   and accurate by Mike.
6. Rebase `hardening-safe` (`d7f38c3`) onto current main and get Mike's Logic
   verification before shipping any of it.

Windows real-DAW smoke test is **done** — Paul and Phil both tested Windows
on 1.0.8, no issues. Marketing site is **done** — confirmed live/accurate by
Mike directly.

## Versioning rule (Mike's call)

Bump the version the moment a build has been SENT to anyone, testers
included. If a build never left Mike's machine, overwrite it in place at the
same version number instead (happened once: 1.0.8's broken→corrected fix).

## How to rebuild after a code fix

Signing + notary are set up on Mike's machine (Developer ID certs in the
login keychain, `SPASYNTH_NOTARY` profile). Per fix:

```
export CMAKE_BUILD_PARALLEL_LEVEL=2     # the Mac is memory-starved; 4 jobs gets OOM-killed
export SPASYNTH_CODESIGN_IDENTITY="Developer ID Application: Kenzora Games (7K9WY5T49S)"
export SPASYNTH_INSTALLER_IDENTITY="Developer ID Installer: Kenzora Games (7K9WY5T49S)"
export SPASYNTH_NOTARIZE_PROFILE="SPASYNTH_NOTARY"
./scripts/build_release.sh -            # "-" skips the slow library repackage (unchanged)
```

This now **automatically clears any dev-build shadow copy** from
`~/Library/Audio/Plug-Ins/` as its first step (fixed after this bit us twice
— 1.0.4 and 1.0.8 — a leftover dev/auval build there silently shadows the
signed release in Logic since macOS prefers the user domain over
`/Library`). No longer a manual habit to remember for release builds
specifically, but still do it by hand after any ad hoc `cmake --build
... SPASynth_AU SPASynth_VST3` dev/auval run:
```
rm -rf ~/Library/Audio/Plug-Ins/Components/SPASynth.component ~/Library/Audio/Plug-Ins/VST3/SPASynth.vst3
```

If the script dies after signing (notary wait killed, credentials gone):
`scripts/notarize.sh dist/installers/SPASynth-<v>-macOS.pkg` (uses
`~/.config/spasynth/notary.env`) then `scripts/build_release.sh
--stage-only <v>`. No rebuild needed.

Then Windows: push `main` to trigger the Windows-only-on-push CI (repo must
be public; never push again until the exe is fetched, the workflow cancels
in-progress runs), `scripts/fetch_windows_build.sh <sha7>`, copy the exe into both
`dist/shopify` folders. Verify: one distinct md5 per installer across all
locations, `otool -l <standalone> | grep minos` -> `minos 11.0`, `spctl -a -t
install <pkg>` -> accepted. Ask Mike before rebuilding (he batches findings).

**When handing Mike a pkg to install, always give an absolute path.** A
relative path from the wrong working directory failed silently on 1.0.14 —
the installer command just did nothing and looked like it worked.

## Gotchas learned

- **`setMouseClickGrabsKeyboardFocus`, not `setWantsKeyboardFocus`, for any
  focus-stealing bug.** See the 1.0.8 section above — this cost a whole
  extra round-trip once already.
- **Dev-build shadow copies in `~/Library` silently override the installed
  release in Logic** (macOS prefers user domain over system domain for AU
  lookup). `build_release.sh` now clears this automatically; still do it by
  hand after any dev/auval build outside that script.
- **A push right after flipping the repo public can silently fail to
  trigger CI** (no run appears, no error) — retry with an empty
  `ci: trigger` commit a bit later. Happened on 1.0.7.
- **Repo is currently PUBLIC, left that way on purpose** (Mike's call as of
  2026-08-21, to avoid blocking agent progress) — don't prompt him to
  re-private it unless he asks.
- **GitHub Actions storage quota is account-wide**, not per-repo, and the
  usage-alert email reflects cycle-peak/cumulative usage, not a live
  snapshot — check `gh api repos/{owner}/{repo}/actions/artifacts` and
  `.../actions/cache/usage` across *all* repos before assuming spasynth is
  the cause.
- Notary profile has vanished six times; `scripts/notarize.sh` now reads
  `~/.config/spasynth/notary.env` (Mike's file, never in the repo, never
  print it) and only falls back to the keychain profile.
- **Tests must stay hermetic**: presets root + settings file are overridden
  to temp dirs in the tests main and a leak guard exits 1 if anything lands
  in the real Factory folder. Tests polluted Mike's real settings and
  presets three times this round. macOS-only JUCE calls in tests go under
  `#if JUCE_MAC` or Windows CI breaks.
- **Every PopupMenu needs a focus anchor** (`showPopupAnchored`), or it
  flashes and closes under the QWERTY focus sweep.
- Subagent briefs: foreground only, never `git stash/checkout/reset`,
  strict file ownership, verify every claim yourself.
- Test binary: `build/SPASynthTests_artefacts/SPASynthTests` (no `Debug/`
  subdir — `build/` was reconfigured without `CMAKE_BUILD_TYPE`).
- Verification ritual for every change: build `SPASynthTests` and run it
  (expect ALL PASS), look at `--snapshot` renders for UI changes, then
  `auval` (+ `pluginval` strictness-8 if available) for anything touching
  the audio thread.
- **New step 1b (2026-09-07): also build and run the AddressSanitizer test
  suite in `build-asan/`.** This is what caught the FDNReverb off-path read
  (likely the real cause of the 1.0.13 audit-hardening noise bursts), the
  VOICE call-out lifetime bug, and a genuine test bug in
  `voicePanelEditorCloseTest` (fixed to use a SafePointer instead of a raw
  `CallOutBox*` across message pumps). That test's flake also aborts
  `build_release.sh` if it fires during a release build — if the release
  script dies there, it's the known flake, re-run.
- Load-bearing invariants (do not break): `CMAKE_OSX_DEPLOYMENT_TARGET=11.0`;
  append-only choice orders (FX module ids, EQ band types, voice/reverb/EQ
  character modes); RT-safety on the audio thread; per-preset `fxOrder`
  packed atomic. See CLAUDE.md's invariants section.

## House style (customer-facing copy)

First-person company voice ("we"/"our"/Silverplatter Audio), never name
individuals, **no em dashes**, sound count 11,474.
