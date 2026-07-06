#!/bin/zsh
# One-shot release build: plugin (universal Release), tests, macOS installer,
# library packages, and the assembled Shopify deliverable folders.
#
#   ./scripts/build_release.sh [<library folder>]
#
# <library folder> defaults to ./library (the build_library.sh output). Pass
# "-" to skip library packaging (much faster; installers + docs only).
#
# Output layout (dist/):
#   installers/SPASynth-<v>-macOS.pkg        (signed if identities are set —
#                                             see installers/macos/build_installer.sh)
#   library/packs/<Pack>.zip                 88 add-on packs
#   library/SPASynth Starter Library.zip
#   shopify/SPASynth-Standard-<v>/           ready-to-zip download folders
#   shopify/SPASynth-Pro-<v>/
#
# The Windows installer is built by CI (windows job, Inno Setup) — download
# the artifact and drop it into both shopify folders before uploading.

set -e -u

REPO_ROOT="${0:A:h:h}"
cd "$REPO_ROOT"

LIBRARY="${1:-$REPO_ROOT/library}"
VERSION=$(sed -n 's/^project(SPASynth VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)
DIST="$REPO_ROOT/dist"
BUILD="$REPO_ROOT/build-release"

echo "=== SPASynth $VERSION release build ==="

# --- 1. Plugin (universal Release) + tests ------------------------------------
cmake -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DSPASYNTH_UNIVERSAL_BINARY=ON -DSPASYNTH_COPY_PLUGIN=OFF
cmake --build "$BUILD"
"$BUILD/SPASynthTests_artefacts/Release/SPASynthTests"

# --- 2. macOS installer ---------------------------------------------------------
mkdir -p "$DIST/installers"
"$REPO_ROOT/installers/macos/build_installer.sh" "$BUILD" "$DIST/installers"

# --- 3. Library packages --------------------------------------------------------
if [[ "$LIBRARY" != "-" ]]; then
    "$REPO_ROOT/scripts/package_library.sh" "$LIBRARY" "$DIST/library"
fi

# --- 4. Shopify download folders ------------------------------------------------
for sku in Standard Pro; do
    folder="$DIST/shopify/SPASynth-$sku-$VERSION"
    mkdir -p "$folder/Library"
    cp "$DIST/installers/SPASynth-$VERSION-macOS.pkg" "$folder/"
    cp packaging/docs/README.txt packaging/docs/QUICKSTART.txt \
       packaging/docs/EULA.txt "$folder/"

    if [[ "$LIBRARY" != "-" ]]; then
        if [[ "$sku" == "Standard" ]]; then
            cp "$DIST/library/SPASynth Starter Library.zip" "$folder/Library/"
        else
            cp "$DIST/library/packs/"*.zip "$folder/Library/"
        fi
    fi
done

echo ""
echo "=== done ==="
echo "Shopify folders: $DIST/shopify/"
echo "Remaining manual steps:"
echo "  1. Drop the CI-built SPASynth-$VERSION-Windows.exe into each shopify folder"
echo "  2. Zip each folder and upload to Shopify (Digital Downloads)"
[[ -z "${SPASYNTH_INSTALLER_IDENTITY:-}" ]] \
    && echo "  3. (pkg is UNSIGNED - set SPASYNTH_*_IDENTITY env vars and re-run step 2)"
exit 0
