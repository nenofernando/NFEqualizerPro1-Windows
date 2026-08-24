#!/bin/bash
# Builds a drag-and-drop macOS DMG installer for NF Mono Check.
# Requires the VST3 to already be built (Release config).
set -euo pipefail

PLUGIN_NAME="NF Mono Check"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
VST3_PATH="$ROOT_DIR/build/NFMonoCheck_artefacts/Release/VST3/$PLUGIN_NAME.vst3"
OUT_DIR="$ROOT_DIR/$PLUGIN_NAME - Download Package"
DMG_NAME="$PLUGIN_NAME - Mac Installer.dmg"

if [ ! -d "$VST3_PATH" ]; then
    echo "error: VST3 not found at: $VST3_PATH" >&2
    echo "Build it first with: cmake --build \"$ROOT_DIR/build\" --config Release" >&2
    exit 1
fi

STAGING_DIR="$(mktemp -d)"
trap 'rm -rf "$STAGING_DIR"' EXIT

cp -R "$VST3_PATH" "$STAGING_DIR/"
ln -s /Library/Audio/Plug-Ins/VST3 "$STAGING_DIR/VST3"

mkdir -p "$OUT_DIR"
rm -f "$OUT_DIR/$DMG_NAME"

hdiutil create \
    -volname "$PLUGIN_NAME" \
    -srcfolder "$STAGING_DIR" \
    -ov -format UDZO \
    "$OUT_DIR/$DMG_NAME"

echo "Created: $OUT_DIR/$DMG_NAME"
