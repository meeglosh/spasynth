#!/bin/zsh
# Rasterizes app_icon.svg -> app_icon.png (1024x1024), the source image JUCE
# turns into the standalone's .icns (macOS) / .ico (Windows) via ICON_BIG.
# Run after editing app_icon.svg. Uses qlmanage (ships with macOS).

set -e -u
HERE="${0:A:h}"
SVG="$HERE/app_icon.svg"
PNG="$HERE/app_icon.png"

TMP=$(mktemp -d)
trap "rm -rf '$TMP'" EXIT

# qlmanage renders at the SVG's own size; the svg is authored at 1024.
qlmanage -t -s 1024 -o "$TMP" "$SVG" >/dev/null 2>&1
mv "$TMP/app_icon.svg.png" "$PNG"

echo "wrote $PNG ($(sips -g pixelWidth -g pixelHeight "$PNG" | awk '/pixel/{print $2}' | paste -sd x -))"
