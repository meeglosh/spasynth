#!/bin/zsh
# Builds the SPASynth Standard edition's "starter library": 5 representative
# WAVs from every commercial SFX pack in the catalog, converted to
# 24-bit/48kHz and zipped with path prefix
# "Silverplatter Audio/SPASynth Library/<Pack>/..." (extracts to the default
# library root, matching auto-discovery). Unlike the old package_library.sh
# starter step (retired, see that script's header), this reads directly from
# the raw Silverplatter pack zips via the SPAStation catalog, so it grows
# automatically as new packs are added -- no stale `library/` folder to
# rebuild first.
#
#   scripts/build_starter.sh [--catalog <catalog-map.json>] [--releases <catalog-releases.json>]
#                             [--out <dir>] [--overrides <overrides.tsv>] [--allow-missing]
#
# Defaults: --catalog and --releases point at the SPAStation checkout's
# catalog files (sibling repo on this machine); --out defaults to
# dist/library (relative to the repo root); --overrides defaults to
# scripts/starter-pack-overrides.tsv (checked in). --allow-missing lets the
# run proceed (and produce a starter library) even if some packs' zips can't
# be found; without it, any unresolved pack is a hard error (exit 1).
#
# scripts/starter-pack-overrides.tsv is a checked-in tab-separated file
# (`catalog-id<TAB>absolute zip path`, `#` comments and blank lines allowed)
# for catalog entries whose zipCandidates are stale and whose display name
# doesn't match the Dropbox folder name closely enough for the guess below to
# find it. It is consulted FIRST, before both the catalog's zipCandidates and
# the Dropbox name-guess fallback; a resolved-via-override pack prints "via
# override" in the resolution table. Add a line here rather than editing the
# catalog when a specific pack's path just needs a manual pin.
#
# Pack resolution mirrors SPAStation's electron/spasynth-library.cjs
# `derivePackName` exactly (via a small embedded Node script): the overrides
# file is checked first; then each sfx catalog entry's zipCandidates are
# tried in order; if none exist on disk, the Dropbox "Packs/<Pack
# Folder>/Zip for Distribution/*.zip" convention (and its "_Micro Packs"
# subfolder) is tried as a fallback, matching the catalog entry's display
# name. The pack's folder NAME then comes from the zip's single top-level
# directory (vendor prefix stripped, underscores -> spaces) if it has
# exactly one, else from the catalog display name (e.g. the Seagulls zip,
# whose "Audio Files/" sits at the zip root alongside other root entries).
#
# For each pack: the 5-sound "spread across the pack's size range" selection
# (really: 5 alphabetically-spread indices -- 1, n/4+1, n/2+1, 3n/4+1, n --
# same rule package_library.sh used) is computed by LISTING the zip
# (`unzip -Z1`), not fully extracting it. Only the ~5 needed members per pack
# are actually extracted (`unzip -j`) and converted (`afconvert -f WAVE -d
# LEI24@48000`, same as build_library.sh), and only if not already cached at
# dist/starter-cache/<Pack>/<file>.wav -- reruns after the first are fast.
# Stale cached files for a pack that are no longer selected are pruned.
#
# The starter zip is only rebuilt if the computed pack/file selection
# differs from dist/library/SPASynth Starter Library.manifest.txt (sorted
# "pack/file" lines) -- an unchanged catalog reruns instantly and prints
# "starter unchanged (N packs, M sounds)".
#
# Idempotent, safe to interrupt and rerun.
#
# --- Evergreen publish step ------------------------------------------------
# Shopify's Digital Downloads app for the Standard SKU points at a Dropbox
# shared link for the starter zip. A Dropbox shared link is tied to the file
# PATH, not its content -- so the published copy at --publish must always be
# overwritten IN PLACE at the exact same path, never deleted and recreated,
# or the existing Shopify download link breaks for every past and future
# buyer. This path MUST NOT be moved or renamed:
#   /Users/mikejerugim/Library/CloudStorage/Dropbox-Flat7Inc./Michael Jerugim/Silverplatter/Packs/_SPASynth Starter Library/SPASynth Starter Library.zip
# Pass --no-publish to skip publishing (e.g. for a local test build), or
# --publish <path> to publish somewhere else. See --publish-only below for a
# way to run just this step against the existing dist/library zip.

set -e -u
setopt extendedglob

REPO_ROOT="${0:A:h:h}"
cd "$REPO_ROOT"

CATALOG="/Users/mikejerugim/SPAStation/server/shopify/catalog-map.json"
RELEASES="/Users/mikejerugim/SPAStation/shared/catalog-releases.json"
OUT="$REPO_ROOT/dist/library"
PACKS_ROOT="/Users/mikejerugim/Library/CloudStorage/Dropbox-Flat7Inc./Michael Jerugim/Silverplatter/Packs"
OVERRIDES="$REPO_ROOT/scripts/starter-pack-overrides.tsv"
ALLOW_MISSING=0
PUBLISH="/Users/mikejerugim/Library/CloudStorage/Dropbox-Flat7Inc./Michael Jerugim/Silverplatter/Packs/_SPASynth Starter Library/SPASynth Starter Library.zip"
PUBLISH_EXPLICIT=0
DO_PUBLISH=1
PUBLISH_ONLY=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --catalog) CATALOG="$2"; shift 2 ;;
        --releases) RELEASES="$2"; shift 2 ;;
        --out) OUT="$2"; shift 2 ;;
        --overrides) OVERRIDES="$2"; shift 2 ;;
        --allow-missing) ALLOW_MISSING=1; shift ;;
        --publish) PUBLISH="$2"; PUBLISH_EXPLICIT=1; shift 2 ;;
        --no-publish) DO_PUBLISH=0; shift ;;
        --publish-only) PUBLISH_ONLY=1; shift ;;
        *) echo "usage: $0 [--catalog <path>] [--releases <path>] [--out <dir>] [--overrides <file>] [--allow-missing] [--publish <path>] [--no-publish] [--publish-only]" >&2; exit 2 ;;
    esac
done

CACHE="$REPO_ROOT/dist/starter-cache"
PREFIX="Silverplatter Audio/SPASynth Library"
STARTER_ZIP="$OUT/SPASynth Starter Library.zip"
MANIFEST="$OUT/SPASynth Starter Library.manifest.txt"

mkdir -p "$OUT" "$CACHE"

# publish_starter: copy dist/library's starter zip + manifest to $PUBLISH,
# overwriting IN PLACE at the same path (write a temp file alongside the
# target, then `mv -f` over it) so the Dropbox path is never briefly missing
# and a partial copy is never visible. Skips (with a WARNING, exit 0 from the
# caller) if the Dropbox path is unavailable, unless --publish was given
# explicitly, in which case that's a hard error.
publish_starter() {
    if [[ "$DO_PUBLISH" -eq 0 ]]; then
        return 0
    fi
    if [[ ! -f "$STARTER_ZIP" ]]; then
        echo "WARNING: publish skipped -- no starter zip at $STARTER_ZIP" >&2
        return 0
    fi

    local publish_dir="${PUBLISH:h}"
    if [[ ! -d "$publish_dir" ]]; then
        if ! mkdir -p "$publish_dir" 2>/dev/null; then
            if [[ "$PUBLISH_EXPLICIT" -eq 1 ]]; then
                echo "ERROR: publish target directory unavailable: $publish_dir" >&2
                exit 1
            fi
            echo "WARNING: publish target directory unavailable (Dropbox not mounted?): $publish_dir" >&2
            return 0
        fi
    fi

    local src_size dst_size
    src_size=$(stat -f%z "$STARTER_ZIP")

    if [[ -f "$PUBLISH" ]]; then
        dst_size=$(stat -f%z "$PUBLISH")
        if [[ "$src_size" == "$dst_size" ]]; then
            local src_md5 dst_md5
            src_md5=$(md5 -q "$STARTER_ZIP")
            dst_md5=$(md5 -q "$PUBLISH")
            if [[ "$src_md5" == "$dst_md5" ]]; then
                echo "publish target up to date"
                return 0
            fi
        fi
    fi

    local tmp_zip="${publish_dir}/.$(basename "$PUBLISH").tmp-$$"
    if ! cp "$STARTER_ZIP" "$tmp_zip"; then
        rm -f "$tmp_zip"
        if [[ "$PUBLISH_EXPLICIT" -eq 1 ]]; then
            echo "ERROR: failed to copy starter zip to publish target" >&2
            exit 1
        fi
        echo "WARNING: failed to copy starter zip to publish target ($publish_dir)" >&2
        return 0
    fi
    mv -f "$tmp_zip" "$PUBLISH"

    # Manifest travels alongside, same overwrite-in-place discipline. Not
    # load-bearing for the Dropbox link, so failures here are non-fatal
    # regardless of --publish-explicit.
    if [[ -f "$MANIFEST" ]]; then
        local publish_manifest="${PUBLISH%.zip}.manifest.txt"
        local tmp_manifest="${publish_dir}/.$(basename "$publish_manifest").tmp-$$"
        if cp "$MANIFEST" "$tmp_manifest" 2>/dev/null; then
            mv -f "$tmp_manifest" "$publish_manifest"
        fi
    fi

    dst_size=$(stat -f%z "$PUBLISH")
    echo "published: $PUBLISH ($dst_size bytes)"
}

if [[ "$PUBLISH_ONLY" -eq 1 ]]; then
    publish_starter
    exit 0
fi

# --- Step 1: resolve every sfx catalog entry to a zip path + derived pack ------
# folder name. Embedded Node script mirrors SPAStation's derivePackName().
RESOLVER=$(mktemp -t spasynth-resolve-XXXX).mjs
cat > "$RESOLVER" <<'NODE_EOF'
import fs from 'node:fs';
import path from 'node:path';
import { execFile } from 'node:child_process';
import { promisify } from 'node:util';
const execFileP = promisify(execFile);

const [ , , CATALOG, RELEASES, PACKS_ROOT, OVERRIDES ] = process.argv;

function loadOverrides(overridesPath) {
  const map = {};
  let text;
  try { text = fs.readFileSync(overridesPath, 'utf8'); } catch { return map; }
  for (const rawLine of text.split('\n')) {
    const line = rawLine.replace(/\r$/, '');
    if (!line.trim() || line.trim().startsWith('#')) continue;
    const [id, zipPath] = line.split('\t');
    if (id && zipPath) map[id.trim()] = zipPath.trim();
  }
  return map;
}

const PACK_PREFIXES = ['SilverPlatterAudio - ', 'SilverPlatter Audio Explorer - ', 'SilverPlatter Audio - ', 'Silverplatter Audio - '];

function sanitizePackName(name) {
  if (typeof name !== 'string') return null;
  const trimmed = name.trim();
  if (!trimmed || trimmed.startsWith('.') || trimmed.includes('/') || trimmed.includes('\\') || trimmed.includes('..') || /[\x00-\x1f]/.test(trimmed)) return null;
  return trimmed;
}
function stripKnownPrefix(name) {
  for (const prefix of PACK_PREFIXES) {
    if (name.startsWith(prefix)) return name.slice(prefix.length);
  }
  return name;
}
function derivePackName({ topLevelDirs, catalogId, catalogReleases }) {
  if (Array.isArray(topLevelDirs) && topLevelDirs.length === 1) {
    const underscoresToSpaces = topLevelDirs[0].replace(/_/g, ' ');
    const raw = stripKnownPrefix(underscoresToSpaces).trim();
    const sanitized = sanitizePackName(raw);
    if (sanitized) return sanitized;
  }
  const fallback = catalogReleases && catalogReleases[catalogId] && catalogReleases[catalogId].name;
  return sanitizePackName(fallback) ?? undefined;
}
async function listZipTopLevelDirs(zipPath) {
  // unzip -Z1 reads only the central directory (instant), unlike tar -tf
  // which streams the whole archive -- matters here since these are
  // multi-GB zips read over Dropbox cloud storage (90 zips x 1-3GB each).
  const { stdout } = await execFileP('/usr/bin/unzip', ['-Z1', zipPath], { maxBuffer: 64 * 1024 * 1024 });
  const tops = new Set();
  for (const line of stdout.split('\n')) {
    const entry = line.trim();
    if (!entry || entry.startsWith('__MACOSX/') || entry === '.DS_Store' || entry.endsWith('/.DS_Store') || entry.startsWith('.') || entry.split('/').pop().startsWith('._')) continue;
    const first = entry.split('/')[0];
    if (first) tops.add(first);
  }
  return [...tops];
}
function norm(s) { return s.toLowerCase().replace(/[^a-z0-9]+/g, ' ').trim(); }

function findDropboxZip(displayName) {
  const roots = [PACKS_ROOT, path.join(PACKS_ROOT, '_Micro Packs')];
  const targetNorm = norm(displayName);
  for (const root of roots) {
    let entries;
    try { entries = fs.readdirSync(root, { withFileTypes: true }); } catch { continue; }
    for (const ent of entries) {
      if (!ent.isDirectory() || norm(ent.name) !== targetNorm) continue;
      const packDir = path.join(root, ent.name);
      for (const sub of ['Zip for Distribution', 'Zip For Distribution', 'Zip for distribution']) {
        const zipDir = path.join(packDir, sub);
        if (!fs.existsSync(zipDir)) continue;
        const zips = fs.readdirSync(zipDir).filter(f => f.toLowerCase().endsWith('.zip'));
        if (zips.length > 0) return path.join(zipDir, zips[0]);
      }
    }
  }
  return null;
}

async function main() {
  const catalog = JSON.parse(fs.readFileSync(CATALOG, 'utf8'));
  const releases = JSON.parse(fs.readFileSync(RELEASES, 'utf8'));
  const overrides = loadOverrides(OVERRIDES);
  const libs = catalog.libraries.filter(v => v.kind === 'sfx');
  const results = [];
  for (const lib of libs) {
    const id = lib.id;
    const displayName = (releases[id] && releases[id].name) || id;
    let zipPath = null, via = null;
    if (overrides[id] && fs.existsSync(overrides[id])) { zipPath = overrides[id]; via = 'override'; }
    if (!zipPath) {
      for (const cand of lib.zipCandidates || []) {
        if (fs.existsSync(cand)) { zipPath = cand; via = 'catalog'; break; }
      }
    }
    if (!zipPath) {
      const found = findDropboxZip(displayName);
      if (found) { zipPath = found; via = 'dropbox-guess'; }
    }
    if (!zipPath) { results.push({ id, displayName, zipPath: null, packName: null, via: null, error: 'no-zip-found' }); continue; }
    let packName;
    try {
      const tops = await listZipTopLevelDirs(zipPath);
      packName = derivePackName({ topLevelDirs: tops, catalogId: id, catalogReleases: releases });
    } catch (e) {
      results.push({ id, displayName, zipPath, packName: null, via, error: 'zip-list-failed: ' + e.message }); continue;
    }
    if (!packName) { results.push({ id, displayName, zipPath, packName: null, via, error: 'no-pack-name' }); continue; }
    results.push({ id, displayName, zipPath, packName, via, error: null });
  }
  console.log(JSON.stringify(results));
}
main();
NODE_EOF

RESOLVED=$(mktemp -t spasynth-resolved-XXXX).json
node "$RESOLVER" "$CATALOG" "$RELEASES" "$PACKS_ROOT" "$OVERRIDES" > "$RESOLVED"
rm -f "$RESOLVER"

echo "=== pack resolution ==="
printf '%-28s  %-70s  %-14s  %s\n' "id" "zip" "via" "pack name"
python3 - "$RESOLVED" <<'PY_EOF'
import json, sys
rows = json.load(open(sys.argv[1]))
for r in rows:
    zp = r["zipPath"] or "(NOT FOUND)"
    pn = r["packName"] or ("MISSING: " + (r["error"] or "?"))
    via = r.get("via") or "-"
    if via == "override":
        via = "via override"
    print(f'{r["id"]:<28}  {zp:<70}  {via:<14}  {pn}')
PY_EOF

MISSING_COUNT=$(python3 -c "
import json
rows = json.load(open('$RESOLVED'))
print(sum(1 for r in rows if not r['zipPath'] or not r['packName']))
")

if [[ "$MISSING_COUNT" -gt 0 ]]; then
    echo ""
    echo "=== unresolved packs ($MISSING_COUNT) ==="
    python3 -c "
import json
rows = json.load(open('$RESOLVED'))
for r in rows:
    if not r['zipPath'] or not r['packName']:
        print(f\"  {r['id']}: {r.get('error')}\")
"
    if [[ "$ALLOW_MISSING" -eq 0 ]]; then
        echo ""
        echo "ERROR: $MISSING_COUNT pack(s) unresolved. Re-run with --allow-missing to proceed anyway." >&2
        rm -f "$RESOLVED"
        exit 1
    fi
    echo "(--allow-missing: proceeding without them)"
fi

# --- Step 2: per-pack selection, conversion cache -------------------------------
typeset -a MANIFEST_LINES
RESOLVED_COUNT=0

pack_count=$(python3 -c "
import json
rows = json.load(open('$RESOLVED'))
print(sum(1 for r in rows if r['zipPath'] and r['packName']))
")

echo ""
echo "=== selecting + converting (pack cache: $CACHE) ==="

# Emit "id\tzipPath\tpackName" lines for resolved packs only, NUL-safe-ish via
# a delimiter unlikely to appear in these fields (tab).
python3 -c "
import json
rows = json.load(open('$RESOLVED'))
for r in rows:
    if r['zipPath'] and r['packName']:
        print(f\"{r['id']}\t{r['zipPath']}\t{r['packName']}\")
" > "${RESOLVED}.tsv"

# Filenames as afconvert/the filesystem actually store and hand back to a
# glob are NFC-composed, but the zip's central directory can list an
# accented name NFD-decomposed (e.g. "François" as c + combining cedilla +
# o, not the single precomposed cedilla-c). -f/-lookup by path tolerates the
# difference, but a plain string/array compare (used below to tell a wanted
# cache file from a stale one) does not -- it silently disagrees even though
# the two names print identically. Route every basename through this so
# wanted-list membership checks match what glob/readdir hands back.
to_nfc() { iconv -f UTF-8-MAC -t UTF-8 2>/dev/null <<< "$1" }

while IFS=$'\t' read -r pack_id zip_path pack_name; do
    # List WAV entries (not extracting the zip), skipping __MACOSX / AppleDouble junk.
    typeset -U all_entries
    all_entries=("${(@f)$(unzip -Z1 "$zip_path" 2>/dev/null | grep -vi '__MACOSX' | grep -v '/\._' | grep -v '^\._' | grep -iE '\.wav$' | sort)}")
    n=${#all_entries[@]}
    if [[ $n -eq 0 ]]; then
        echo "WARNING: no WAVs found in zip for pack '$pack_name' ($zip_path)" >&2
        continue
    fi

    typeset -U selected_entries
    selected_entries=()
    for idx in 1 $(( n / 4 + 1 )) $(( n / 2 + 1 )) $(( 3 * n / 4 + 1 )) "$n"; do
        (( idx >= 1 && idx <= n )) && selected_entries+=("${all_entries[$idx]}")
    done

    cache_dir="$CACHE/$pack_name"
    mkdir -p "$cache_dir"

    typeset -U wanted_basenames
    wanted_basenames=()
    typeset -a need_extract
    need_extract=()
    for entry in "${selected_entries[@]}"; do
        base=$(to_nfc "${entry:t}")
        wanted_basenames+=("$base")
        [[ -f "$cache_dir/$base" ]] || need_extract+=("$entry")
    done

    if (( ${#need_extract[@]} > 0 )); then
        tmpx=$(mktemp -d)
        unzip -qq -j -o "$zip_path" "${need_extract[@]}" -d "$tmpx"
        for entry in "${need_extract[@]}"; do
            base=$(to_nfc "${entry:t}")
            src="$tmpx/${entry:t}"
            if [[ -f "$src" ]]; then
                afconvert -f WAVE -d LEI24@48000 "$src" "$cache_dir/$base" \
                    && echo "  converted: $pack_name / $base" \
                    || echo "  CONVERT FAILED: $pack_name / $base" >&2
            else
                echo "  EXTRACT FAILED: $pack_name / $entry" >&2
            fi
        done
        rm -rf "$tmpx"
    fi

    # Prune cached files no longer selected for this pack. The glob matches
    # both .wav and .WAV via explicit alternation (a plain *.wav pattern
    # previously missed .WAV cache files as staleness candidates entirely,
    # and the same gap hit the staging cp below); the compare is exact-byte
    # against wanted_basenames, deliberately WITHOUT zsh's (L)/(#i)
    # case-folding, which recomposes NFD accented filenames (e.g. macOS's
    # own on-disk "François" -- combining cedilla -> precomposed cedilla) and
    # made an unrelated correctly-cached accented file compare unequal to
    # itself, pruning it every run.
    for f in "$cache_dir"/*.(wav|WAV)(N); do
        base="${f:t}"
        if (( ! ${wanted_basenames[(Ie)$base]} )); then
            rm -f "$f"
            echo "  pruned stale cache file: $pack_name / $base"
        fi
    done

    for base in "${wanted_basenames[@]}"; do
        MANIFEST_LINES+=("$pack_name/$base")
    done
    RESOLVED_COUNT=$((RESOLVED_COUNT + 1))
done < "${RESOLVED}.tsv"

rm -f "${RESOLVED}.tsv" "$RESOLVED"

# --- Step 3: compare against the existing manifest; rezip only if changed ------
typeset -a NEW_MANIFEST_SORTED
NEW_MANIFEST_SORTED=("${(@on)MANIFEST_LINES}")
new_sound_count=${#NEW_MANIFEST_SORTED[@]}

NEW_MANIFEST_TMP=$(mktemp -t spasynth-manifest-XXXX)
{
    echo "# SPASynth Starter Library manifest"
    echo "# packs: $RESOLVED_COUNT"
    echo "# sounds: $new_sound_count"
    for line in "${NEW_MANIFEST_SORTED[@]}"; do
        echo "$line"
    done
} > "$NEW_MANIFEST_TMP"

if [[ -f "$MANIFEST" ]] && diff -q "$MANIFEST" "$NEW_MANIFEST_TMP" > /dev/null 2>&1; then
    echo ""
    echo "starter unchanged ($RESOLVED_COUNT packs, $new_sound_count sounds)"
    rm -f "$NEW_MANIFEST_TMP"
    # Even when the starter itself didn't change, still verify the published
    # copy is present and matches -- covers the case where the publish
    # target was deleted, moved, or drifted out from under a prior run.
    publish_starter
else
    echo ""
    if [[ -f "$MANIFEST" ]]; then
        echo "=== changes vs previous manifest ==="
        echo "-- added --"
        comm -13 <(grep -v '^#' "$MANIFEST" | sort) <(grep -v '^#' "$NEW_MANIFEST_TMP" | sort) || true
        echo "-- removed --"
        comm -23 <(grep -v '^#' "$MANIFEST" | sort) <(grep -v '^#' "$NEW_MANIFEST_TMP" | sort) || true
    else
        echo "=== no previous manifest; building starter library for the first time ==="
    fi

    echo ""
    echo "=== staging + zipping ==="
    stage=$(mktemp -d)
    for pack_dir in "$CACHE"/*(N/); do
        pack_name=$(basename "$pack_dir")
        dest="$stage/$PREFIX/$pack_name"
        mkdir -p "$dest"
        # Explicit extension alternation: cached files can be .wav or .WAV
        # depending on the source zip's own casing, and a plain *.wav glob
        # here silently dropped the .WAV ones from the zip while the
        # manifest still counted them as staged. (Avoid extendedglob's
        # (#i)/(L) case-folding for this -- see the prune step below for why.)
        cp -c "$pack_dir"/*.(wav|WAV)(N) "$dest/" 2>/dev/null || cp "$pack_dir"/*.(wav|WAV)(N) "$dest/"
    done
    rm -f "$STARTER_ZIP"
    (cd "$stage" && zip -q -r -X "$STARTER_ZIP" "Silverplatter Audio")

    # Report (and record in the manifest) what was ACTUALLY staged into the
    # zip, not the packs*5 figure computed from selection -- a staging bug
    # can silently drop files and the two used to disagree.
    actual_staged_count=$(find "$stage" -type f -iname '*.wav' | wc -l | tr -d ' ')
    rm -rf "$stage"

    if [[ "$actual_staged_count" != "$new_sound_count" ]]; then
        echo "WARNING: selected $new_sound_count sound(s) but staged $actual_staged_count -- using the staged count" >&2
        new_sound_count="$actual_staged_count"
        sed -i '' "s/^# sounds: .*/# sounds: $new_sound_count/" "$NEW_MANIFEST_TMP"
    fi

    mv -f "$NEW_MANIFEST_TMP" "$MANIFEST"
    zip_size=$(du -h "$STARTER_ZIP" | cut -f1)
    echo "packaged: starter library ($RESOLVED_COUNT packs, $new_sound_count sounds, $zip_size)"

    publish_starter
fi

echo ""
echo "=== summary ==="
echo "packs: $RESOLVED_COUNT   sounds: $new_sound_count   zip: $STARTER_ZIP"
[[ "$MISSING_COUNT" -gt 0 ]] && echo "missing packs: $MISSING_COUNT (see 'unresolved packs' above)"
echo "done."
