# SPASynth handoff (2026-09-13)

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
  the 11 Pro volumes and the links file are retired for Pro. **R2 now holds
  ONLY the Standard starter** (2026-09-13, Mike's call): the Pro volumes
  were deleted from the bucket, and the starter is at
  `https://downloads.spasynth.com/starter-library/SPASynth-Starter-Library.zip`
  (bucket `spasynth`, folder `starter-library/`; re-upload with `rclone
  copyto ... --s3-chunk-size 16M --s3-upload-concurrency 1
  --multi-thread-streams 1` -- larger chunks/concurrency dropped the
  connection twice on this link). Shopify Digital Downloads can't take a raw
  R2 link and the 3.6 GB browser upload crashed in every browser, so the
  Standard product uses an **External URL asset pointing at Dropbox**:
  `https://www.dropbox.com/scl/fi/m0jgglmd4lek75e96x1my/SPASynth-Starter-Library.zip?rlkey=tfhahebjx5tr95ivgfdxnva7t&dl=0`
  backed by the evergreen file
  `Silverplatter/Packs/_SPASynth Starter Library/SPASynth Starter Library.zip`
  (path-tied link; `build_starter.sh --publish` overwrites it in place, so
  the link never changes -- NEVER move/rename that file). SPAStation should
  fetch the starter from the R2 URL (Codex: allow downloads.spasynth.com as
  a source). SPAStation is recommended, never required. Pro and Upgrade link to
  the Everything Bundle (Shopify links or SPAStation). The old
  `SPASynth Pro Library - Downloads.html` pages were deleted from
  `dist/shopify/`. SPASynth
  plays 96k as-is (loader keeps source rate, voice resamples); cost is 2x
  disk and 2x RAM (whole-file float in memory). **Accepted for now; add a
  "load samples at 48k" preference only if testers hit memory trouble.**
- **Standard's starter library stays 24/48** (5 sounds from every pack),
  delivered via Shopify directly. It is an upsell bullet (Pro = full
  fidelity originals) AND a marketing surface: **the starter grows with
  every new pack release as a free update** (put this on the Standard PDP
  and the website). So `package_library.sh`'s starter step must become
  re-runnable as packs are added (today it skips if the zip exists);
  **DONE 2026-09-13: `scripts/build_starter.sh`** builds the starter
  straight from the commercial pack zips using SPAStation's catalog map as
  the pack list (`--catalog/--releases/--out/--overrides/--allow-missing`),
  derives pack folder names with the SAME rule as SPAStation's installer
  (zip top folder minus the company prefix, else catalog name), picks 5
  sounds per pack by size spread, converts only uncached files into
  `dist/starter-cache/`, and rezips only when the manifest
  (`dist/library/SPASynth Starter Library.manifest.txt`) changes.
  `build_release.sh` now calls it; `package_library.sh` is legacy. Current
  starter: **90 packs / 450 sounds / 3.4 GB**, verified 5 WAVs per pack,
  24/48, no junk. `scripts/starter-pack-overrides.tsv` pins zips the
  catalog can't resolve (Waterfalls). **`glitch-percussive`: SKIP (Mike, 2026-09-13)** —
  it is a SPAStation catalog entry with no zip file anywhere; **Codex fixes
  it in SPAStation's `server/shopify/catalog-map.json`** (remove or remap
  the entry). Until then `--allow-missing` is required and it stays out of
  the starter. Gotchas learned: 56 of the 89 Dropbox zips are online-only
  (~39 GB hydrates on first run; we freed 63 GB by deleting the retired
  Pro volumes + per-pack zips from `dist/library/`, so `library/` at 37 GB
  is now the only big local copy and only the `--real-library` test uses
  it); uppercase `.WAV` and NFD-decomposed accented filenames both bit the
  first run (fixed, case-insensitive + NFC-normalised). To ship a starter
  update: run the script, re-upload the zip to the Standard product.
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
  **DONE 2026-09-13: SPAStation main `3574085`** (on top of beta.13
  `be4a602`, pushed): `electron/spasynth-{library,install,prefs}.cjs`,
  IPC `spasynth:*`, Downloads-view buttons, 220 tests. Not yet in a
  packaged SPAStation release (beta.13 predates it); needs an end-to-end
  test (download Seagulls → Install → SPASynth Rescan). Note: Codex works
  in the SPAStation repo too; check `git status` there before agents
  touch it.
- Open question (Mike): send SPASynth Pro buyers to the EB PDP instead of
  a separate Pro PDP.

## 1.0.16 BUILT + STAGED (2026-09-16, main `0858427`), awaiting Mike's install

1.0.15 was SENT to Paul and Phil and tested by Mike. Landed since:
- `0afe777` **library auto-refresh** (design below, implemented as spec'd:
  watcher off the 150 ms timer, `computeLibraryFingerprint()` /
  `tickLibraryWatch()` in SPASynthProcessor, two-tick debounce, Rescan
  button kept with a new tooltip; `libraryAutoRefreshTest`).
- `6caebe4` **VOICE panel use-after-free** found by ASan while verifying the
  above: `ContentComponent` tracked only the latest VOICE panel, so one
  dismissed-but-not-yet-deleted panel escaped `detach()` and its Knob
  attachments hit freed parameters. Now `openVoicePanels` (array of
  SafePointers) + a self-deleting `DismissWatcher` that detaches on hide;
  `voicePanelDismissedThenClosedTest`. Also the ASSIGN overlay paint-budget
  assertion is skipped under ASan (`__has_feature(address_sanitizer)`).
- `bbe785e` **friendlier library setup**, from a Windows beta tester who
  could not get the library recognised. Explorer's "Extract All" appends
  the zip name to the destination, so the payload landed a folder deeper
  than `expandLibraryCandidates()` probed, and nothing told him anything
  was wrong. Discovery now accepts wrapper layouts plus Downloads and the
  Desktop (capped at `maxCandidateDirsExamined` = 300, canonical paths
  still win); the watcher's routine tick reads only the configured root
  and attempts discovery at most every 30 s and only when unconfigured
  (it was about to run the whole walk every 3 s for exactly the users we
  were helping); a one-time prompt offers the folder picker when nothing
  is found, persisted via `library::get/setEmptyLibraryPromptShown`; the
  Windows installer pre-creates the destination and adds a "SPASynth
  Sounds Folder" Start Menu item, and README/QUICKSTART give macOS and
  Windows matched steps. Note the extraction destination is the GRANDparent
  of the library folder on both platforms (`/Users/Shared`,
  `C:\Users\Public\Documents`), because the zip carries
  "Silverplatter Audio/SPASynth Library/" inside it.
- `7e73983` **ASSIGN one-shot + latch** (Mike, 2026-09-16). Single click
  arms one route and exits as soon as the row just written has BOTH a
  source and a destination (his call over "any single assignment"), so
  writing a destination into a row that already had a source exits
  immediately. Double click latches, caps-lock style, and latch is
  unconditional from any starting mode. Padlock glyph marks latch (his
  choice over a filled button or a longer label). `MatrixPanel::AssignMode`
  is the single source of truth with JUCE's own toggling disabled;
  `AssignOverlay::maybeCompleteOneShot` checks both route params against
  the real "None" index 0 from ParameterRegistry. Gotcha for anyone
  touching this: JUCE dispatches BOTH ordinary clicks before
  `mouseDoubleClick` (juce_Component.cpp internalMouseUp), so the double
  click handler promotes whatever the clicks left behind.
- `5e47bb7` **drag and drop audio** (Phil asked for an easier way to get
  his own audio in; shipped instead of Direct Audio Input, see the
  editions section above for why). `OscStrip` and `ConvolvePanel` are
  `FileDragAndDropTarget`s; `oscContentExtensions/Wildcard/Accepts` in
  ModulePanels is the ONE list both the choosers and the drop targets
  read, so keep it that way. Dropping on a non-file mode switches the slot
  to Sample via `setValueNotifyingHost`. Highlight is border-only using
  Theme's assign glow token (same blue as ASSIGN, deliberately: it means
  "this is a target"). `oscStripFileDropTest`.
- `8a4aed1` **new mod routes start at half depth** (user report: "every row
  should come standard with at least .5 level" -- rows were silent because
  Depth is bipolar and defaults to 0). Parameter default deliberately LEFT
  at 0 (correct centre and correct reset for a bipolar control);
  `maybeAutoFillRouteDepth` in MatrixPanel.h nudges to +0.5 only when a row
  becomes complete AND depth is still exactly 0. **The rule that must not
  be broken: this fires on genuine user edits only.** Preset load, session
  restore, reset-to-default and RANDOMIZE ALL all write parameters
  programmatically and a saved route deliberately at 0 must survive. ASSIGN
  is inherently user-only; the dropdowns use `RouteComboBox`, whose flag is
  set in the virtual `showPopup()` that JUCE reaches only from mouse/key
  handling, never from an attachment sync. An abandoned popup clears the
  flag via a self-stopping 40 ms poll on `isPopupActive()` -- without that,
  opening a dropdown, backing out, then loading a preset silently rewrote
  the preset's depth, which is the bug class to watch for here. Failure
  direction is deliberately "missed nudge", never "rewritten value".
  `modRouteAutoDepthTest` has guards for all four programmatic paths.
Suite 1431 ALL PASS (Debug x2, Release, ASan, leak checks clean).

**1.0.16 artifacts (2026-09-16):** macOS pkg from `0858427`, signed +
notarized + stapled, `spctl` accepted, universal (x86_64 + arm64), minos
11.0, md5 `c821da7a04130867a8c0ddc5c33ce549`. Windows exe from draft release
`ci-windows-0858427`, md5 `08dd4a22a9935ea7965e7491a8cdfe4e`. Both
byte-identical across `dist/installers/` and
`dist/shopify/SPASynth-{Standard,Pro}-1.0.16/`. Dev plugin copies cleared.
**Pending: Mike installs (`sudo installer -pkg
/Users/mikejerugim/spasynth/dist/installers/SPASynth-1.0.16-macOS.pkg
-target /`, then Plug-in Manager -> Reset & Rescan -> relaunch Logic), runs
the gauntlet, and sends both installers to Paul and Phil.** Things to
exercise that are new here: drag audio onto an oscillator, ASSIGN single vs
double click, a freshly made mod route being audible immediately, and a
saved preset with a deliberately zeroed route still loading at zero.

## Planned for 1.0.17 (Mike, 2026-09-16)

- **Direct Audio Input** (Phil). See the parked write-up in the editions
  section above for why it waited and the three implementation paths;
  capture-to-slot over a sidechain bus is the recommended one. The bus
  topology change is the risk, so give it its own round and its own Logic
  verification.
- **Granular effect** in the style of Absynth's Aetherizer. Starting param
  set: grain size, density, pitch, spread, feedback, mix, plus a freeze that
  holds the buffer. Two things established while scoping it:
  - **Do NOT merge Reverb and Convolve into one tab** to make room. Their
    ids live in an append-only enum packed into every preset's FX order;
    removing one breaks saved presets, and a merged tab also breaks
    per-module drag reorder.
  - **Adding a 10th module has a hidden migration bug.**
    `FXChain::unpackOrder` reads exactly `numModules` nibbles, so bumping 9
    to 10 makes it read a tenth entry out of old nine-module values, see a
    duplicate, decide the value is corrupt and fall back to the default
    order. Every preset with a custom FX order would silently revert.
    Handle a short packed value explicitly by appending the new module.
  - **Space: rebalance row 3** (Mike's call). The FX tab bar is 44% of the
    row with the matrix taking the rest; nine tabs sit in roughly 600 px
    and a tenth needs about 60 more, which is about five points moved from
    the matrix. Shortening labels alone does not work, since EQ and MOD are
    already on the tab-width floor.
  - The live ring buffer this needs is the same machinery Direct Audio
    Input's rolling-buffer option would want, so build them in that order.
Phil's SPAStation-installed pack not appearing: asked him for (1) Mac or
Windows + did he Rescan, (2) SPAStation's "SPASynth library:" path, (3)
SPASynth's Set Library Folder path. If the paths differ it's the root
resolver in SPAStation; if they match, we need the pack name + folder
listing. Next: any further 1.0.16 findings, then build + send.

### Original 1.0.16 queue note
- **Library auto-refresh.** Phil installed a pack via SPAStation and it did
  not appear in SPASynth (likely no Rescan/relaunch; root mismatch not yet
  ruled out -- asked him for the two paths). Regardless: SPASynth should
  watch its library root and refresh itself when a pack folder appears or
  disappears. Design: message-thread timer (every ~3 s while an editor is
  open, ~10 s otherwise) compares the root's immediate-subfolder listing
  (names + mtimes) to the last scan; on change, `refreshLibrary()` (which
  already generates presets for new packs), debounced so a pack mid-copy
  is not scanned half-written (wait for the listing to be stable for two
  ticks). Never on the audio thread; filesystem reads stay off the audio
  path (they already do). Test: temp root, add a pack folder with a WAV
  after construction, pump, assert the pack + its presets appear without a
  manual rescan. Do not bump the version until a build is sent.

## Product decisions of record: editions (2026-09-16)

- **One binary, forever.** Mike considered splitting Standard and Pro by
  FEATURE (Standard = factory presets only, no user audio, 48 kHz; Pro =
  user audio, drag and drop, 96 kHz) and decided against it after review.
  Reasons, so this does not get relitigated: the no-DRM decision means
  nothing stops a Standard buyer running a Pro binary, so the gate is
  unenforceable while content differentiation is self-enforcing; both
  editions share a plugin identity, so upgraders would face
  uninstall/reinstall and cross-edition sessions and shared presets would
  break; it doubles every release (two signings, notarizations,
  installers, staging folders, test matrices) on the machine that is
  already the bottleneck; and the sample-rate axis needs no code at all,
  since Standard gets the 24/48 starter and Pro the 24/96 originals by
  content alone. **Editions stay differentiated by content only.**
- If feature tiers are ever wanted, the only credible path is SPAStation
  writing a signed entitlement file that the plugin reads, since SPAStation
  already verifies real Shopify entitlements. That conflicts with
  "SPAStation recommended, never required" and would be a deliberate 1.1+
  conversation.
- **Direct Audio Input (Phil's request) is PARKED for 1.1.** SPASynth is a
  pure instrument: one stereo output, no input bus, `IS_SYNTH TRUE`,
  `AU_MAIN_TYPE kAudioUnitType_MusicDevice`. Live input means changing bus
  topology, which is the exact class of change that has repeatedly cost us
  Logic validation days (see the 2026-09-04 saga). Options when it is
  picked up, cheapest first: (1) optional sidechain input bus + "capture
  to slot" recording a few seconds into an oscillator's sample buffer,
  which reuses granular/stretch/loop/filters/chaos unchanged and is by far
  the best value; (2) a live rolling ring buffer the granular engine reads
  behind the write head, which breaks the static-buffer assumption through
  the sample engine; (3) shipping an audio-effect build alongside the
  instrument, cleanest conceptually but a second plugin identity, payload,
  validation and store decision. Drag and drop shipped in 1.0.16 instead as
  the cheap, zero-topology-risk answer to most of the same need.

## Library figures: the numbers are automated, do not hand-edit them

`~/spasynth-landing/scripts/update-library-stats.py` is the source of truth.
It sums a per-pack `fileCount` out of SPAStation's `catalog-releases.json`
under Mike's 2026-09-13 inclusion rule (count only what is currently sold;
retired packs like Wood Impacts do not count), adds the Vault bonus
recordings from `vault-stats.json`, and rewrites every `data-stat` span and
the meta descriptions in the site's `index.html`. It also derives starter =
5 x packs and presets = 3 x packs. Run it after every pack release.
Current, 2026-09-16: **90 packs, 11,479 pack sounds, 339 Vault bonus,
11,818 total, 450 starter, 270 presets, 76 GB.**

**The two sound figures are not interchangeable.** 11,479 live in the
packs; the other 339 are Vault bonus recordings. "90 packs, 11,818 sounds"
is false. Use the packs figure for inventory claims and "up to 11,818" for
what a Pro owner can play; `docs/shopify-listings.md`'s "How many sounds"
FAQ reconciles them.

The repo's marketing copy (`docs/shopify-listings.md`,
`docs/launch-email.md`, `docs/social-posts.md`, `docs/marketing-brief.md`)
is still HAND-maintained, which is why it drifted on four separate figures
at once and needed `6c4885b` + `4c65476` to fix. Anything derived from the
pack count drifts together: pack count, starter size, preset count, totals.
Worth pointing the same script at these files, or at minimum running it and
diffing before any release.

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
