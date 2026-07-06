#!/bin/zsh
# Packages the built SPASynth library (see build_library.sh) for delivery.
#
#   ./scripts/package_library.sh <library folder> <output folder>
#
# Produces, under <output folder>:
#   packs/<Pack Name>.zip             one zip per pack (add-on pack SKUs)
#   SPASynth Starter Library.zip      5 representative sounds from every pack
#                                     (the Standard edition's included content)
#   SPASynth Pro Library (Part N).zip the full library, split into volumes of
#                                     whole packs under the Shopify Digital
#                                     Downloads 5 GB file cap (override with
#                                     SPASYNTH_VOLUME_CAP_MB, default 4500)
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

# --- Pro library volumes --------------------------------------------------------
# Whole packs per volume; every volume extracts with the same one instruction.
VOLCAP_MB=${SPASYNTH_VOLUME_CAP_MB:-4500}
existing_volumes=("$OUT"/SPASynth\ Pro\ Library*.zip(N))
if (( ${#existing_volumes[@]} > 0 )); then
    echo "skip: pro library volumes (already packaged)"
else
    vol=1
    volMB=0
    stage=$(mktemp -d)

    flush_volume() {
        [[ $volMB -eq 0 ]] && return
        (cd "$stage" && zip -q -r -X "$OUT/SPASynth Pro Library (Part $vol).zip" \
                            "Silverplatter Audio")
        rm -rf "$stage"
        echo "packaged: pro volume $vol (${volMB} MB)"
        stage=$(mktemp -d)
        (( vol++ )) || true
        volMB=0
    }

    for pack in "$LIB"/*(-/); do
        name=$(basename "$pack")
        packMB=$(du -smL "$pack" | cut -f1)
        (( volMB > 0 && volMB + packMB > VOLCAP_MB )) && flush_volume

        mkdir -p "$stage/$PREFIX"
        cp -RLc "$pack" "$stage/$PREFIX/$name" 2>/dev/null \
            || cp -RL "$pack" "$stage/$PREFIX/$name"
        (( volMB += packMB )) || true
    done
    flush_volume
    rm -rf "$stage"

    # A library that fits one volume doesn't need part numbering.
    if [[ $vol -eq 2 && -f "$OUT/SPASynth Pro Library (Part 1).zip" ]]; then
        mv "$OUT/SPASynth Pro Library (Part 1).zip" "$OUT/SPASynth Pro Library.zip"
    fi
fi

echo "library packages in: $OUT"
