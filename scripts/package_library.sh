#!/bin/zsh
# Packages the built SPASynth library (see build_library.sh) for delivery.
#
#   ./scripts/package_library.sh <library folder> <output folder>
#
# Produces, under <output folder>:
#   packs/<Pack Name>.zip          one zip per pack (Pro bundle + add-on SKUs)
#   SPASynth Starter Library.zip   5 representative sounds from every pack
#                                  (the Standard edition's included content)
#
# Every zip contains the full "Silverplatter Audio/SPASynth Library/<Pack>/"
# path, so customers extract into /Users/Shared (macOS) or
# C:\Users\Public\Documents (Windows) and SPASynth's auto-discovery finds it.
# Idempotent: existing zips are skipped.

set -e -u

LIB="$1"
OUT="$2"

PREFIX="Silverplatter Audio/SPASynth Library"
mkdir -p "$OUT/packs"

# --- Per-pack zips -------------------------------------------------------------
for pack in "$LIB"/*(-/); do
    name=$(basename "$pack")
    zipfile="$OUT/packs/$name.zip"
    if [[ -f "$zipfile" ]]; then
        echo "skip: $name (already packaged)"
        continue
    fi

    # Stage via APFS clones (instant, no extra disk) — zip won't traverse a
    # directory symlink, so a real directory tree is required.
    stage=$(mktemp -d)
    mkdir -p "$stage/$PREFIX"
    cp -RLc "$pack" "$stage/$PREFIX/$name" 2>/dev/null \
        || cp -RL "$pack" "$stage/$PREFIX/$name"
    (cd "$stage" && zip -q -r -X "$zipfile" "Silverplatter Audio")
    rm -rf "$stage"
    echo "packaged: $name"
done

# --- Starter selection (Standard edition) --------------------------------------
STARTER="$OUT/SPASynth Starter Library.zip"
if [[ -f "$STARTER" ]]; then
    echo "skip: starter library (already packaged)"
else
    stage=$(mktemp -d)
    for pack in "$LIB"/*(-/); do
        name=$(basename "$pack")
        dest="$stage/$PREFIX/$name"
        mkdir -p "$dest"

        # 5 sounds spread across the pack's size range (matches the spread
        # the factory preset generator samples from).
        wavs=("${(@f)$(find -L "$pack" -type f -iname '*.wav' | sort)}")
        n=${#wavs[@]}
        [[ $n -eq 0 ]] && continue
        for idx in 1 $(( n / 4 + 1 )) $(( n / 2 + 1 )) $(( 3 * n / 4 + 1 )) $n; do
            (( idx >= 1 && idx <= n )) && cp -n "${wavs[$idx]}" "$dest/" 2>/dev/null || true
        done
    done
    (cd "$stage" && zip -q -r -X "$STARTER" "Silverplatter Audio")
    rm -rf "$stage"
    echo "packaged: starter library"
fi

echo "library packages in: $OUT"
