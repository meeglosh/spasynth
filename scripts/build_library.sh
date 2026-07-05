#!/bin/zsh
# Builds the shippable SPASynth library from the raw Silverplatter pack zips.
#
#   ./scripts/build_library.sh "<zips folder>" "<output library folder>"
#
# For each zip: extracts WAVs (skipping __MACOSX junk, resource forks, and
# docs), converts them to 24-bit/48kHz WAV (the distribution format — film/TV
# ready; keep the 24/96 originals archived), and writes them into one folder
# per pack. Pack folders map 1:1 to SPASynth preset categories.
# Idempotent: packs with a non-empty output folder are skipped.

set -u

SRC="$1"
DEST="$2"
mkdir -p "$DEST"

for zip in "$SRC"/*.zip; do
    name=$(basename "$zip" .zip)
    name=${name/#SilverPlatterAudio - /}
    name=${name/#SilverPlatter Audio Explorer - /}
    name=${name/#SilverPlatter Audio - /}
    name=${name/#Silverplatter Audio - /}
    name=${name//_/ }

    out="$DEST/$name"
    if [[ -d "$out" && -n "$(ls -A "$out" 2>/dev/null)" ]]; then
        echo "skip: $name (already built)"
        continue
    fi

    tmp=$(mktemp -d)
    if ! unzip -qq "$zip" -d "$tmp" -x "__MACOSX/*" 2>/dev/null; then
        echo "UNZIP FAILED: $name"
        rm -rf "$tmp"
        continue
    fi

    mkdir -p "$out"
    converted=0
    failed=0
    while IFS= read -r -d '' f; do
        base=$(basename "$f")
        if afconvert -f WAVE -d LEI24@48000 "$f" "$out/$base" 2>/dev/null; then
            converted=$((converted + 1))
        else
            failed=$((failed + 1))
            echo "  convert failed: $name / $base"
        fi
    done < <(find "$tmp" -type f -iname "*.wav" ! -path "*__MACOSX*" ! -name "._*" -print0)

    rm -rf "$tmp"

    if [[ $converted -eq 0 ]]; then
        echo "EMPTY PACK (no wavs converted): $name"
        rmdir "$out" 2>/dev/null
    else
        echo "done: $name ($converted files${failed:+, $failed failed})"
    fi
done

echo "ALL DONE"
