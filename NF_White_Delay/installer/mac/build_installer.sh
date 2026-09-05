#!/bin/bash
# Builds the full "final product" macOS installer for NF White Delay:
# a welcome/choices/conclusion .pkg (VST3 + AU + AAX + manual, user picks
# which plug-in formats) wrapped in a DMG.
# Requires VST3/AU/AAX to already be built (Release config, build-release
# dir) and the manual PDF to already be generated (manual/build_manual.py).
set -euo pipefail

PLUGIN_NAME="NF White Delay"
VERSION="1.0.0"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
INSTALLER_DIR="$ROOT_DIR/installer/mac"
MANUAL_DIR="$ROOT_DIR/manual"
ARTEFACTS_DIR="$ROOT_DIR/build-release/NFWhiteDelay_artefacts/Release"
VST3_PATH="$ARTEFACTS_DIR/VST3/$PLUGIN_NAME.vst3"
AU_PATH="$ARTEFACTS_DIR/AU/$PLUGIN_NAME.component"
AAX_PATH="$ARTEFACTS_DIR/AAX/$PLUGIN_NAME.aaxplugin"
MANUAL_EN_PATH="$MANUAL_DIR/NF_White_Delay_Manual_EN.pdf"
MANUAL_PT_PATH="$MANUAL_DIR/NF_White_Delay_Manual_PT.pdf"
OUT_DIR="$ROOT_DIR/$PLUGIN_NAME - Download Package"
FINAL_PKG_NAME="NF White Delay Installer.pkg"
DMG_NAME="$PLUGIN_NAME - Mac Installer.dmg"

for path in "$VST3_PATH" "$AU_PATH" "$AAX_PATH" "$MANUAL_EN_PATH" "$MANUAL_PT_PATH"; do
    if [ ! -e "$path" ]; then
        echo "error: not found: $path" >&2
        echo "Build all plugin formats first with: cmake --build \"$ROOT_DIR/build-release\" --config Release" >&2
        echo "and generate the manual with: python3 \"$MANUAL_DIR/build_manual.py\"" >&2
        exit 1
    fi
done

WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT

VST3_ROOT="$WORK_DIR/vst3-root/Library/Audio/Plug-Ins/VST3"
AU_ROOT="$WORK_DIR/au-root/Library/Audio/Plug-Ins/Components"
AAX_ROOT="$WORK_DIR/aax-root/Library/Application Support/Avid/Audio/Plug-Ins"
DOCS_ROOT="$WORK_DIR/docs-root/Users/Shared/NF Audio Tools/$PLUGIN_NAME/Manual"
mkdir -p "$VST3_ROOT" "$AU_ROOT" "$AAX_ROOT" "$DOCS_ROOT"
cp -R "$VST3_PATH" "$VST3_ROOT/"
cp -R "$AU_PATH" "$AU_ROOT/"
cp -R "$AAX_PATH" "$AAX_ROOT/"
cp "$MANUAL_EN_PATH" "$DOCS_ROOT/"
cp "$MANUAL_PT_PATH" "$DOCS_ROOT/"

PKG_DIR="$WORK_DIR/pkgs"
mkdir -p "$PKG_DIR"

pkgbuild \
    --root "$WORK_DIR/vst3-root" \
    --identifier "com.nfaudiotools.nfwhitedelay.vst3" \
    --version "$VERSION" \
    --install-location "/" \
    "$PKG_DIR/NFWhiteDelay-VST3.pkg"

pkgbuild \
    --root "$WORK_DIR/au-root" \
    --identifier "com.nfaudiotools.nfwhitedelay.au" \
    --version "$VERSION" \
    --install-location "/" \
    "$PKG_DIR/NFWhiteDelay-AU.pkg"

pkgbuild \
    --root "$WORK_DIR/aax-root" \
    --identifier "com.nfaudiotools.nfwhitedelay.aax" \
    --version "$VERSION" \
    --install-location "/" \
    "$PKG_DIR/NFWhiteDelay-AAX.pkg"

pkgbuild \
    --root "$WORK_DIR/docs-root" \
    --identifier "com.nfaudiotools.nfwhitedelay.docs" \
    --version "$VERSION" \
    --install-location "/" \
    "$PKG_DIR/NFWhiteDelay-Docs.pkg"

mkdir -p "$OUT_DIR"
FINAL_PKG_PATH="$WORK_DIR/$FINAL_PKG_NAME"

productbuild \
    --distribution "$INSTALLER_DIR/Distribution.xml" \
    --resources "$INSTALLER_DIR" \
    --package-path "$PKG_DIR" \
    "$FINAL_PKG_PATH"

DMG_STAGING="$WORK_DIR/dmg-staging"
mkdir -p "$DMG_STAGING"
cp "$FINAL_PKG_PATH" "$DMG_STAGING/"

rm -f "$OUT_DIR/$DMG_NAME"
hdiutil create \
    -volname "$PLUGIN_NAME" \
    -srcfolder "$DMG_STAGING" \
    -ov -format UDZO \
    "$OUT_DIR/$DMG_NAME"

echo "Created: $OUT_DIR/$DMG_NAME"
