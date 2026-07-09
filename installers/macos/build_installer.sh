#!/bin/zsh
# Builds the macOS installer package for SPASynth.
#
#   ./installers/macos/build_installer.sh <build dir> <output dir>
#
# <build dir> is a CMake build tree containing SPASynth_artefacts/Release
# (build with -DCMAKE_BUILD_TYPE=Release -DSPASYNTH_UNIVERSAL_BINARY=ON).
#
# Signing is opt-in via environment (leave unset for an unsigned pkg —
# fine for local testing, required unset on CI):
#   SPASYNTH_CODESIGN_IDENTITY   "Developer ID Application: ..." — signs the
#                                bundles before packaging
#   SPASYNTH_INSTALLER_IDENTITY  "Developer ID Installer: ..." — signs the pkg
#   SPASYNTH_NOTARIZE_PROFILE    notarytool keychain profile — submits the
#                                signed pkg for notarization + staples it

set -e -u

BUILD_DIR="$1"
OUT_DIR="$2"

REPO_ROOT="${0:A:h:h:h}"
VERSION=$(sed -n 's/^project(SPASynth VERSION \([0-9.]*\).*/\1/p' "$REPO_ROOT/CMakeLists.txt")
ARTEFACTS="$BUILD_DIR/SPASynth_artefacts/Release"

[[ -d "$ARTEFACTS/VST3/SPASynth.vst3" ]] || { echo "error: no Release artefacts in $BUILD_DIR"; exit 1; }
mkdir -p "$OUT_DIR"

WORK=$(mktemp -d)
trap "rm -rf '$WORK'" EXIT

# --- Optional code signing of the bundles ------------------------------------
if [[ -n "${SPASYNTH_CODESIGN_IDENTITY:-}" ]]; then
    for bundle in "$ARTEFACTS/VST3/SPASynth.vst3" \
                  "$ARTEFACTS/AU/SPASynth.component" \
                  "$ARTEFACTS/Standalone/SPASynth.app"; do
        echo "codesign: $bundle"
        codesign --force --deep --options runtime --timestamp \
                 --sign "$SPASYNTH_CODESIGN_IDENTITY" "$bundle"
    done
fi

# --- Component packages -------------------------------------------------------
# Each component MUST get a unique --identifier. All three bundles share one
# CFBundleIdentifier (com.silverplatteraudio.spasynth), and pkgbuild derives
# the package id from it when --identifier is omitted — so the three payloads
# would collide on one receipt id and macOS Installer would lay down only one
# (the bug where AU/VST3 silently didn't install). --version must match the
# distribution pkg-refs too.
pkgbuild --quiet --identifier "com.silverplatteraudio.spasynth.vst3" --version "$VERSION" \
         --component "$ARTEFACTS/VST3/SPASynth.vst3" \
         --install-location "/Library/Audio/Plug-Ins/VST3" \
         "$WORK/SPASynthVST3.pkg"
pkgbuild --quiet --identifier "com.silverplatteraudio.spasynth.au" --version "$VERSION" \
         --component "$ARTEFACTS/AU/SPASynth.component" \
         --install-location "/Library/Audio/Plug-Ins/Components" \
         "$WORK/SPASynthAU.pkg"
pkgbuild --quiet --identifier "com.silverplatteraudio.spasynth.app" --version "$VERSION" \
         --component "$ARTEFACTS/Standalone/SPASynth.app" \
         --install-location "/Applications" \
         "$WORK/SPASynthApp.pkg"

# --- Distribution (choices + license) -----------------------------------------
mkdir -p "$WORK/resources"
cp "$REPO_ROOT/packaging/docs/EULA.txt" "$WORK/resources/License.txt"

cat > "$WORK/distribution.xml" <<XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>SPASynth $VERSION</title>
    <license file="License.txt"/>
    <options customize="always" require-scripts="false" hostArchitectures="arm64,x86_64"/>
    <choices-outline>
        <line choice="au"/>
        <line choice="vst3"/>
        <line choice="app"/>
    </choices-outline>
    <choice id="au" title="Audio Unit" description="For Logic Pro, GarageBand and other AU hosts.">
        <pkg-ref id="com.silverplatteraudio.spasynth.au"/>
    </choice>
    <choice id="vst3" title="VST3" description="For Ableton Live, Cubase, Reaper and other VST3 hosts.">
        <pkg-ref id="com.silverplatteraudio.spasynth.vst3"/>
    </choice>
    <choice id="app" title="Standalone App" description="Play SPASynth without a DAW.">
        <pkg-ref id="com.silverplatteraudio.spasynth.app"/>
    </choice>
    <pkg-ref id="com.silverplatteraudio.spasynth.au" version="$VERSION">SPASynthAU.pkg</pkg-ref>
    <pkg-ref id="com.silverplatteraudio.spasynth.vst3" version="$VERSION">SPASynthVST3.pkg</pkg-ref>
    <pkg-ref id="com.silverplatteraudio.spasynth.app" version="$VERSION">SPASynthApp.pkg</pkg-ref>
</installer-gui-script>
XML

UNSIGNED="$WORK/SPASynth-$VERSION-macOS.pkg"
FINAL="$OUT_DIR/SPASynth-$VERSION-macOS.pkg"

productbuild --quiet --distribution "$WORK/distribution.xml" \
             --package-path "$WORK" --resources "$WORK/resources" \
             "$UNSIGNED"

# --- Optional installer signing + notarization --------------------------------
if [[ -n "${SPASYNTH_INSTALLER_IDENTITY:-}" ]]; then
    productsign --sign "$SPASYNTH_INSTALLER_IDENTITY" "$UNSIGNED" "$FINAL"
    if [[ -n "${SPASYNTH_NOTARIZE_PROFILE:-}" ]]; then
        echo "notarizing (this can take a few minutes)..."
        xcrun notarytool submit "$FINAL" \
              --keychain-profile "$SPASYNTH_NOTARIZE_PROFILE" --wait
        xcrun stapler staple "$FINAL"
    fi
else
    cp "$UNSIGNED" "$FINAL"
    echo "note: unsigned pkg (set SPASYNTH_INSTALLER_IDENTITY to sign)"
fi

echo "installer: $FINAL"
